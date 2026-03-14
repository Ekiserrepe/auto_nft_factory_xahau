# nft_factory — Xahau Hook

A Xahau hook written in C that automatically mints a sequential URIToken NFT in response to an incoming XAH payment. Each NFT receives a unique, zero-padded URI built from a configurable base path, an auto-incremented counter persisted in hook state, and a file extension.

---

## How it works

1. A user sends a XAH payment to the hook account.
2. The hook validates the payment amount against the `AMOUNT` parameter.
3. It reads the current mint counter from hook state (key `CNT`).
4. It increments the counter and checks it does not exceed `NUM`.
5. It builds the final URI: `URI` + zero-padded number + `EXT`.
6. It emits a **URIToken Remit** transaction to the sender with that URI.
7. On successful emit, the new counter is saved back to state.

---

## Hook parameters

All parameters are set at install time.

| Parameter | Type | Size | Description |
|-----------|------|------|-------------|
| `AMOUNT`  | uint64 big-endian | 8 bytes | Exact XAH payment required to mint, in drops |
| `URI`     | ASCII string | 1–200 bytes | Base IPFS URI, including trailing slash. E.g. `ipfs://bafybei.../` |
| `EXT`     | ASCII string | 1–16 bytes | File extension appended after the number. E.g. `.json`, `.png` |
| `NUM`     | uint32 big-endian | 4 bytes | Maximum number of NFTs that can be minted |

You can translate your own values here: [https://transia-rnd.github.io/xrpl-hex-visualizer/]()
### Encoding examples

**AMOUNT** — 1 XAH = 1,000,000 drops:
```
00000000000F4240
```

**NUM** — 1000 NFTs:
```
000003E8
```

**URI** — plain ASCII bytes:
```
ipfs://bafybeiauv5kyvlv5r4slbendy5e6pwdgckcigkg6fww2rndwryk446q7mu/
```

**EXT** — plain ASCII bytes:
```
.json
```

---

## URI format

The final URI written into each minted URIToken is:

```
<URI> + <zero-padded counter> + <EXT>
```

The counter padding width equals the number of digits in `NUM`:

| NUM    | Width | Example URI |
|--------|-------|-------------|
| 25     | 2     | `ipfs://.../01.png` |
| 1000   | 4     | `ipfs://.../0001.json` |
| 100000 | 6     | `ipfs://.../000001.json` |

---

## State

The hook uses a single state entry scoped to the hook account:

| Key   | Size | Content |
|-------|------|---------|
| `CNT` | 4 bytes | Last minted NFT number, big-endian uint32 |

- If no state exists (first mint), the counter starts at 0 and the first NFT is number `1`.
- The state is written **only after** a successful emit, so a failed transaction never advances the counter.

---

## Rollback conditions

| Condition | Message |
|-----------|---------|
| `hook_account` failed | `hook_account failed` |
| `sfAccount` read failed | `sfAccount read failed` |
| Payment is IOU (not XAH) | `Payment must be native XAH, not IOU` |
| Paid amount ≠ `AMOUNT` | `Payment amount does not match required AMOUNT` |
| `AMOUNT` parameter missing/invalid | `Hook Parameter 'AMOUNT' not found or invalid` |
| `NUM` parameter missing/invalid | `Hook Parameter 'NUM' not found or invalid` |
| `EXT` parameter missing | `Hook Parameter 'EXT' not found` |
| `URI` parameter missing | `Hook Parameter 'URI' not found` |
| Base URI exceeds 200 bytes | `Base URI too long` |
| Final URI exceeds 255 bytes | `Final URI too long` |
| Counter would exceed `NUM` | `Maximum NFT limit reached` |
| Emit failed | `Emit failed` |

---

## Accepted (non-minting) transactions

- Any transaction that is not a Payment (`tt != 0`) is silently accepted.
- Outgoing payments where the sender is the hook account itself are silently accepted.

---

## Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_URI_LEN` | 200 | Max length of the `URI` parameter |
| `MAX_FINAL_URI_LEN` | 255 | Max length of the assembled final URI |
| `MAX_EXT_LEN` | 16 | Max length of the `EXT` parameter |
| Max `NUM` | 4,294,967,295 | uint32 ceiling (10 digits) |

---

## Install

There are two ways to install this hook on your account:

### Option A — Create from your own compiled `.wasm`

Use `create_hook_nft_factory.js` to deploy the hook from a `.wasm` binary you compiled yourself. This gives you full control over the code running on-chain.

### Option B — Install using the pre-deployed HookHash

Use `install_hook_nft_factory.js` to reference the already-deployed hook binary on the network. No compilation needed — the script points directly to the hook by its hash.

The HookHash for the version matching the `nft_factory.c` source in this repository is:

```
8B6B79F5E361D97C04ADF75693FFB4E7AF56355FEAD30F8FC1EC86D72EF38818
```

This hash is valid on both **Xahau Testnet** and **Xahau Mainnet**.

> **Disclaimer:** This hook is provided as-is. The author takes no responsibility for any issues it may cause. You are strongly encouraged to compile and audit your own version. Community improvements and forks are welcome.

---

## Build

Compile with the standard Xahau [hook-toolkit](https://hooks-toolkit.com/) targeting WASM:

```bash
hooks-cli init c nft_factory
cd nft_factory
yarn install
yarn run build
```
---

## Acknowledgements

Thanks to [Handy_4ndy](https://x.com/Handy_4ndy) for the [Hooks101](https://github.com/handyAndy/hookhttps://github.com/Handy4ndy/XahauHooks101) library, which made this solution possible.
