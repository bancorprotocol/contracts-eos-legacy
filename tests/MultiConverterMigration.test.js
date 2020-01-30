
const { assert } = require('chai')

const { api, transfer, convert, getBalance, getReserveBalance, getTableRows } = require('./utils')

const testAccount = 'bnttestuser1'
const migrationContract = 'migration'
const bancorConverter = 'multiconvert'

describe('MultiConverterMigration', () => {
    const converters = [
        {
            converter: 'bnt2ccccnvrt',
            relay: { account: 'bnt2cccrelay', symbol: 'BNTCCC' },
            reserve: { account: 'ccc', symbol: 'CCC' }
        },
        {
            converter: 'bnt2dddcnvrt',
            relay: { account: 'bnt2dddrelay', symbol: 'BNTDDD' },
            reserve: { account: 'ddd', symbol: 'DDD' }
        },
        {
            converter: 'bnt2eeecnvrt',
            relay: { account: 'bnt2eeerelay', symbol: 'BNTEEE' },
            reserve: { account: 'eee', symbol: 'EEE' }
        }
    ]

    for (const converterToBeMigrated of converters)
        it(`end to end - ${converterToBeMigrated.converter}`, async () => endToEnd(converterToBeMigrated))
})

async function endToEnd(converterToBeMigrated) {
    const { rows: [oldConverterSettings] } = await getTableRows(converterToBeMigrated.converter, converterToBeMigrated.converter, 'settings')
    
    const preMigrationReserveABalance = await getBalance(converterToBeMigrated.converter, 'bntbntbntbnt', 'BNT');
    const preMigrationReserveBBalance = await getBalance(converterToBeMigrated.converter, converterToBeMigrated.reserve.account, converterToBeMigrated.reserve.symbol);
    
    const totalSupply = await getBalance(testAccount, converterToBeMigrated.relay.account, converterToBeMigrated.relay.symbol);
    await transfer(converterToBeMigrated.relay.account, testAccount, migrationContract, totalSupply, '');
    
    await api.transact({ 
        actions: [{
            account: migrationContract,
            name: "migrate",
            authorization: [{
                actor: testAccount,
                permission: 'active',
            }],
            data: {
                converter_currency_sym: converterToBeMigrated.relay.symbol
            }
        }]
    }, 
    {
        blocksBehind: 3,
        expireSeconds: 30,
    })

    await api.transact({ 
        actions: [{
            account: migrationContract,
            name: "migrate2",
            authorization: [{
                actor: testAccount,
                permission: 'active',
            }],
            data: {
                converter_currency_sym: converterToBeMigrated.relay.symbol
            }
        }]
    }, 
    {
        blocksBehind: 3,
        expireSeconds: 30,
    })

    const { rows: [newConverter] } = await getTableRows(bancorConverter, converterToBeMigrated.relay.symbol, 'converters')
    
    const postMigrationReserveABalance = await getReserveBalance(converterToBeMigrated.relay.symbol, bancorConverter, 'BNT');
    const postMigrationReserveBBalance = await getReserveBalance(converterToBeMigrated.relay.symbol, bancorConverter, converterToBeMigrated.reserve.symbol);
    
    assert.equal(preMigrationReserveABalance, postMigrationReserveABalance, 'unexpected reserve balance')
    assert.equal(preMigrationReserveBBalance, postMigrationReserveBBalance, 'unexpected reserve balance')

    assert.equal(newConverter.fee, oldConverterSettings.fee, "unexpected fee")
    assert.equal(newConverter.owner, testAccount, "unexpected converter owner")

    await convert('1.00000000 BNT', 'bntbntbntbnt', [`${bancorConverter}:${converterToBeMigrated.relay.symbol}`, converterToBeMigrated.reserve.symbol])
}