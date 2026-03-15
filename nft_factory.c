#include "hookapi.h"

#define DEBUG 0

// Account-level flag: sender cannot receive incoming Remit transactions
#define lsfDisallowIncomingRemit 0x40000000UL

#define MAX_URI_LEN     200
#define MAX_FINAL_URI_LEN 255
#define MAX_EXT_LEN     16

// Macro to copy account ID to buffer
#define ACCOUNT_TO_BUF(buf_raw, i)\
{\
    unsigned char* buf = (unsigned char*)buf_raw;\
    *(uint64_t*)(buf + 0) = *(uint64_t*)(i +  0);\
    *(uint64_t*)(buf + 8) = *(uint64_t*)(i +  8);\
    *(uint32_t*)(buf + 16) = *(uint32_t*)(i + 16);\
}

// Set vlURI field (len byte + data + end marker 0xE1)
#define URI_TO_BUF(buf_raw, uri, len) \
{ \
    unsigned char* buf = (unsigned char*)buf_raw; \
    buf[0] = (uint8_t)(len); \
    for (int i = 0; GUARD(MAX_FINAL_URI_LEN), i < (int)(len); ++i) \
        buf[1 + i] = uri[i]; \
    buf[1 + (len)] = 0xE1U; \
}

// Convert n to zero-padded decimal ASCII using the digit count of max_val as width.
// E.g. n=1, max_val=1000 → "0001" (4 chars). Returns width.
static int uint32_to_str_padded(uint8_t *out, uint32_t n, uint32_t max_val)
{
    // Calculate width = number of digits in max_val
    int width = 0;
    uint32_t tmp = max_val;
    for (; GUARD(10), tmp > 0; tmp /= 10)
        width++;

    // Write digits right-to-left into out (already zero-padded via '0' fill)
    for (int i = width - 1; GUARD(10), i >= 0; --i)
    {
        out[i] = '0' + (uint8_t)(n % 10);
        n /= 10;
    }

    return width;
}

// Read AMOUNT hook parameter (8 bytes big-endian uint64, in drops).
// Returns -1 if missing or invalid.
static int64_t read_amount_param(void)
{
    uint8_t key[] = {'A','M','O','U','N','T'};
    uint8_t buf[8];

    int64_t len = hook_param(buf, sizeof(buf), key, sizeof(key));
    if (len != 8)
        return -1;

    uint64_t drops =
        ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
        ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
        ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
        ((uint64_t)buf[6] <<  8) | ((uint64_t)buf[7]);

    return (int64_t)drops;
}

// URIToken Remit tx skeleton
uint8_t txn[1000] =
{
/* size,upto */
/*   3,   0 */   0x12U, 0x00U, 0x5FU,
/*   5,   3 */   0x22U, 0x80U, 0x00U, 0x00U, 0x00U,
/*   5,   8 */   0x24U, 0x00U, 0x00U, 0x00U, 0x00U,
/*   5,  13 */   0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
/*   6,  18 */   0x20U, 0x1AU, 0x00U, 0x00U, 0x00U, 0x00U,
/*   6,  24 */   0x20U, 0x1BU, 0x00U, 0x00U, 0x00U, 0x00U,
/*   9,  30 */   0x68U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
/*  35,  39 */   0x73U, 0x21U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*  22,  74 */   0x81U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*  22,  96 */   0x83U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 116, 118 */   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

/*   3, 234 */  0xE0U, 0x5CU, 0x75U,
/*   1, 237 */  0xE1U,
/*   0, 238 */
};

// Tx builder defines
#define BYTES_LEN 238U
#define FLS_OUT   (txn + 20U)
#define LLS_OUT   (txn + 26U)
#define DTAG_OUT  (txn + 14U)
#define FEE_OUT   (txn + 31U)
#define HOOK_ACC  (txn + 76U)
#define OTX_ACC   (txn + 98U)
#define URI_OUT   (txn + 237U)
#define EMIT_OUT  (txn + 118U)

#define PREPARE_REMIT_TXN(hook_acc, dest_acc, uri, uri_len) do { \
    etxn_reserve(1); \
    if (otxn_field(DTAG_OUT, 4, sfSourceTag) == 4) \
        *(DTAG_OUT - 1) = 0x2EU; \
    uint32_t fls = (uint32_t)ledger_seq() + 1; \
    *((uint32_t *)(FLS_OUT)) = FLIP_ENDIAN_32(fls); \
    uint32_t lls = fls + 4; \
    *((uint32_t *)(LLS_OUT)) = FLIP_ENDIAN_32(lls); \
    ACCOUNT_TO_BUF(HOOK_ACC, hook_acc); \
    ACCOUNT_TO_BUF(OTX_ACC, dest_acc); \
    URI_TO_BUF(URI_OUT, uri, uri_len); \
    etxn_details(EMIT_OUT, 116U); \
    int64_t fee = etxn_fee_base(txn, BYTES_LEN + uri_len + 1); \
    uint8_t *b = FEE_OUT; \
    *b++ = 0b01000000 + ((fee >> 56) & 0b00111111); \
    *b++ = (fee >> 48) & 0xFFU; \
    *b++ = (fee >> 40) & 0xFFU; \
    *b++ = (fee >> 32) & 0xFFU; \
    *b++ = (fee >> 24) & 0xFFU; \
    *b++ = (fee >> 16) & 0xFFU; \
    *b++ = (fee >> 8)  & 0xFFU; \
    *b++ = (fee >> 0)  & 0xFFU; \
} while(0)

int64_t hook(uint32_t reserved)
{
    if (DEBUG) TRACESTR("NFT :: Factory Hook :: Called.");

    // Hook account
    uint8_t hook_acc[20];
    if (hook_account(hook_acc, 20) != 20)
        rollback(SBUF("NFT :: Error :: hook_account failed."), __LINE__);

    // Origin tx account (minter/payer)
    uint8_t otx_acc[20];
    if (otxn_field(otx_acc, 20, sfAccount) != 20)
        rollback(SBUF("NFT :: Error :: sfAccount read failed."), __LINE__);

    // Only Payment transactions
    int64_t tt = otxn_type();
    if (tt != 0)
        accept(SBUF("NFT :: Not a Payment. Accepting."), __LINE__);

    // Skip outgoing payments (hook account sending)
    int equal = 0;
    BUFFER_EQUAL(equal, hook_acc, otx_acc, 20);
    if (equal)
        accept(SBUF("NFT :: Outgoing or self Payment. Accepting."), __LINE__);

    // ── Check DisallowIncomingRemit flag on sender ────────────────────────────
    // If the sender has this flag set, a Remit back to them would fail.
    // Rollback early with a clear error instead of emitting a doomed tx.
    {
        uint8_t acc_kl[34];
        if (util_keylet(SBUF(acc_kl), KEYLET_ACCOUNT, otx_acc, 20, 0, 0, 0, 0) != 34)
            rollback(SBUF("NFT :: Error :: Could not build account keylet."), __LINE__);

        int64_t kl_slot = slot_set(SBUF(acc_kl), 1);
        if (kl_slot < 0)
            rollback(SBUF("NFT :: Error :: Could not load sender account root."), __LINE__);

        if (slot_subfield(kl_slot, sfFlags, 2) < 0)
            rollback(SBUF("NFT :: Error :: Could not access sender account flags."), __LINE__);

        uint8_t sender_flags_buf[4];
        if (slot(SBUF(sender_flags_buf), 2) != 4)
            rollback(SBUF("NFT :: Error :: Could not read sender account flags."), __LINE__);

        uint32_t sender_flags =
            ((uint32_t)sender_flags_buf[0] << 24) | ((uint32_t)sender_flags_buf[1] << 16) |
            ((uint32_t)sender_flags_buf[2] <<  8) | ((uint32_t)sender_flags_buf[3]);

        if (sender_flags & lsfDisallowIncomingRemit)
            rollback(SBUF("NFT :: Error :: Sender has DisallowIncomingRemit flag set."), __LINE__);
    }

    // ── AMOUNT validation ─────────────────────────────────────────────────────
    int64_t required_drops = read_amount_param();
    if (required_drops < 0)
        rollback(SBUF("NFT :: Error :: Hook Parameter 'AMOUNT' not found or invalid."), __LINE__);

    uint8_t amount_buf[8];
    int64_t amount_len = otxn_field(SBUF(amount_buf), sfAmount);
    if (amount_len != 8)
        rollback(SBUF("NFT :: Error :: Payment must be native XAH, not IOU."), __LINE__);

    int64_t incoming_drops = AMOUNT_TO_DROPS(amount_buf);
    if (DEBUG)
    {
        TRACEVAR(required_drops);
        TRACEVAR(incoming_drops);
    }

    if (incoming_drops != required_drops)
        rollback(SBUF("NFT :: Error :: Payment amount does not match required AMOUNT."), __LINE__);

    // ── NUM parameter: maximum NFTs allowed (4 bytes big-endian uint32) ───────
    uint8_t num_key[] = {'N','U','M'};
    uint8_t num_buf[4];
    if (hook_param(num_buf, sizeof(num_buf), num_key, sizeof(num_key)) != 4)
        rollback(SBUF("NFT :: Error :: Hook Parameter 'NUM' not found or invalid."), __LINE__);

    uint32_t max_nfts =
        ((uint32_t)num_buf[0] << 24) | ((uint32_t)num_buf[1] << 16) |
        ((uint32_t)num_buf[2] <<  8) | ((uint32_t)num_buf[3]);

    if (DEBUG) TRACEVAR(max_nfts);

    // ── EXT parameter: file extension, e.g. ".json" or ".png" ────────────────
    uint8_t ext_key[] = {'E','X','T'};
    uint8_t ext_buf[MAX_EXT_LEN];
    int64_t ext_len = hook_param(ext_buf, sizeof(ext_buf), ext_key, sizeof(ext_key));
    if (ext_len <= 0)
        rollback(SBUF("NFT :: Error :: Hook Parameter 'EXT' not found."), __LINE__);

    // ── State: read current NFT counter ──────────────────────────────────────
    // Key "CNT" stores the last minted NFT number as 4-byte big-endian uint32.
    uint8_t cnt_key[]  = {'C','N','T'};
    uint8_t cnt_buf[4] = {0, 0, 0, 0};

    uint32_t current_count = 0;
    int64_t  state_res = state(cnt_buf, sizeof(cnt_buf), cnt_key, sizeof(cnt_key));
    if (state_res == 4)
    {
        // State found: decode stored counter
        current_count =
            ((uint32_t)cnt_buf[0] << 24) | ((uint32_t)cnt_buf[1] << 16) |
            ((uint32_t)cnt_buf[2] <<  8) | ((uint32_t)cnt_buf[3]);
    }
    // else: no state yet → first mint, current_count stays 0

    uint32_t next_count = current_count + 1;
    if (DEBUG) TRACEVAR(next_count);

    if (next_count > max_nfts)
        rollback(SBUF("NFT :: Error :: Maximum NFT limit reached."), __LINE__);

    // ── URI parameter: base IPFS path ─────────────────────────────────────────
    uint8_t uri_key[]  = {'U','R','I'};
    uint8_t base_uri[MAX_URI_LEN];
    int64_t base_len = hook_param(base_uri, sizeof(base_uri), uri_key, sizeof(uri_key));
    if (base_len <= 0)
        rollback(SBUF("NFT :: Error :: Hook Parameter 'URI' not found."), __LINE__);
    if (base_len > MAX_URI_LEN)
        rollback(SBUF("NFT :: Error :: Base URI too long."), __LINE__);

    // ── Build final URI = base_uri + str(next_count) + EXT ───────────────────
    uint8_t final_uri[MAX_FINAL_URI_LEN];
    int64_t final_len = 0;

    for (int i = 0; GUARD(MAX_URI_LEN), i < (int)base_len; ++i)
        final_uri[final_len++] = base_uri[i];

    uint8_t num_str[10];
    int     num_str_len = uint32_to_str_padded(num_str, next_count, max_nfts);

    if (final_len + num_str_len + ext_len > MAX_FINAL_URI_LEN)
        rollback(SBUF("NFT :: Error :: Final URI too long."), __LINE__);

    for (int i = 0; GUARD(10), i < num_str_len; ++i)
        final_uri[final_len++] = num_str[i];

    for (int i = 0; GUARD(MAX_EXT_LEN), i < (int)ext_len; ++i)
        final_uri[final_len++] = ext_buf[i];

    if (DEBUG)
    {
        trace(SBUF("NFT :: Base URI: "),  base_uri,  base_len,  0);
        trace(SBUF("NFT :: Final URI: "), final_uri, final_len, 0);
    }

    // ── Prepare and emit URIToken Remit tx ────────────────────────────────────
    PREPARE_REMIT_TXN(hook_acc, otx_acc, final_uri, final_len);

    uint8_t emithash[32];
    int64_t emit_result = emit(SBUF(emithash), txn, BYTES_LEN + final_len + 1);

    if (emit_result > 0)
    {
        // Persist updated counter to state
        cnt_buf[0] = (next_count >> 24) & 0xFFU;
        cnt_buf[1] = (next_count >> 16) & 0xFFU;
        cnt_buf[2] = (next_count >>  8) & 0xFFU;
        cnt_buf[3] = (next_count >>  0) & 0xFFU;
        state_set(cnt_buf, sizeof(cnt_buf), cnt_key, sizeof(cnt_key));

        accept(SBUF("NFT :: Success :: URIToken minted!"), __LINE__);
    }

    rollback(SBUF("NFT :: Error :: Emit failed."), __LINE__);
    return 0;
}
