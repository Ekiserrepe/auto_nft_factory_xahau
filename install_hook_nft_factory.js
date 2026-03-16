const xahau = require('xahau');
const fs = require('fs');
const crypto = require('crypto');

const seed = 'yourSeed';
const network = "wss://xahau-test.net";

function stringToHex(str) {
  return Buffer.from(str, "utf8").toString("hex").toUpperCase();
}

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
          "Hook": {
            "HookHash": "6786DE24C37FFCDEED643CED581793D4E47BC54FA0D796D35FEA0CBF1338C19B",
            "HookOn": 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBFFFFE', //Only Payments https://richardah.github.io/xrpl-hookon-calculator/
            "HookCanEmit": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFBFFFFF", //Can emit remits
            "HookNamespace": crypto.createHash('sha256').update('nft_factory').digest('hex').toUpperCase(),
            "Flags": 1,
            "HookParameters": [
              {
              "HookParameter": {
              "HookParameterName": "555249",
              "HookParameterValue": stringToHex("ipfs://bafybeig5t3mn2s2mv6ns7o65ee4qz6gw7w47hdovjtmq54qz6fsiv2ctfu/"),
              }
              },
              {
              "HookParameter": {
              "HookParameterName": "455854",
              "HookParameterValue": "2E6A736F6E"
              }
              },
              {
              "HookParameter": {
              "HookParameterName": "4E554D",
              "HookParameterValue": "00000064"
              }
              },
                            {
              "HookParameter": {
              "HookParameterName": "414D4F554E54",
              "HookParameterValue": "0000000000989680"
              }
              }
              ]
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
