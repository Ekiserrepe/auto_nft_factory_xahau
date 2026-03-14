const xahau = require('xahau');
const fs = require('fs');
const crypto = require('crypto');

const seed = 'yourSeed';
const network = "wss://xahau-test.net";

async function connectAndQuery() {
  const client = new xahau.Client(network);
  const my_wallet = xahau.Wallet.fromSeed(seed, {algorithm: 'secp256k1'});
  console.log(`Account: ${my_wallet.address}`);

  try {
    await client.connect();
    console.log('Connected to Xahau');

    const prepared = await client.autofill({
      "TransactionType": "SetHook",
      "Account": my_wallet.address,
      "Flags": 0,
      "Hooks": [
        {
            Hook: {
                CreateCode: "",
                Flags: 1,
            }
        }
    ],
    });

    const { tx_blob } = my_wallet.sign(prepared);
    const tx = await client.submitAndWait(tx_blob);
    console.log("Info tx:", JSON.stringify(tx, null, 2));
  } catch (error) {
    console.error('Error:', error);
  } finally {
    await client.disconnect();
    console.log('Disconnecting from Xahau node');
  }
}

connectAndQuery();
