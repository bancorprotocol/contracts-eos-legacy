const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const fetch = require('node-fetch')
const { TextDecoder, TextEncoder } = require('util')
const { Api, JsonRpc } = require('eosjs')

const signatureProvider = new JsSignatureProvider(["5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"])
const rpc = new JsonRpc('http://127.0.0.1:8888', { fetch })
const api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })

const transfer = async function (token, from, to, quantity, memo) { 
    return api.transact({ 
        actions: [{
            account: token,
            name: "transfer",
            authorization: [{
                actor: from,
                permission: 'active',
            }],
            data: {
                from,
                to,
                quantity,
                memo
            }
        }]
    }, 
    {
        blocksBehind: 3,
        expireSeconds: 30,
    })
}

const getBalance = async function (account, token, symbol) {
    return (await rpc.get_table_rows({
        "code": token,
        "scope": account,
        "table": "accounts",
        "limit": 1,
        "lower_bound": symbol,
    })).rows[0].balance;
}

const getReserveBalance = async function (converterCurrencySymbol, converter, reserveSymbol) {
    return (await rpc.get_table_rows({
        "code": converter,
        "scope": converterCurrencySymbol,
        "table": "reserves",
        "limit": 1,
        "lower_bound": reserveSymbol,
    })).rows[0].balance;
}

module.exports = {
    api,
    transfer,
    getBalance,
    getReserveBalance
};