
const { assert } = require('chai')
const Decimal = require('decimal.js')
const {
    api,
    transfer,
    convert,
    getBalance,
    getReserveBalance,
    getTableRows,
    expectNoError,
    expectError
} = require('./utils')

const testAccount1 = 'bnttestuser1'
const testAccount2 = 'bnttestuser2'
const migrationContract = 'migration'
const bancorConverter = 'multiconvert'
const multiTokens = 'multi4tokens'

const singleLiquidityProviderMigrations = [
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
const multipleLiquidityProvidersMigrations = [
    {
        converter: 'bnt2ccccnvrt',
        relay: { account: 'bnt2cccrelay', symbol: 'BNTCCC' },
        reserve: { account: 'ccc', symbol: 'CCC' }
    },
    {
        converter: 'bnt2fffcnvrt',
        relay: { account: 'bnt2fffrelay', symbol: 'BNTFFF' },
        reserve: { account: 'fff', symbol: 'FFF' }
    }
]

describe('MultiConverterMigration', () => {
    it('ensures MultiConverterMigration::addconverter cannot be called without proper permissions', async () => {
        const addconverter = api.transact({ 
            actions: [{
                account: migrationContract,
                name: "addconverter",
                authorization: [{
                    actor: testAccount1,
                    permission: 'active',
                }],
                data: {
                    converter_sym: 'ABC',
                    converter_account: 'multi4tokens',
                    owner: 'bnttestuser1'
                }
            }]
        }, 
        {
            blocksBehind: 3,
            expireSeconds: 30,
        })

        await expectError(
            addconverter,
            'missing authority'
        )
    })
    it('ensures MultiConverterMigration::delconverter cannot be called without proper permissions', async () => {
        const delconverter = api.transact({ 
            actions: [{
                account: migrationContract,
                name: "delconverter",
                authorization: [{
                    actor: testAccount1,
                    permission: 'active',
                }],
                data: {
                    converter_sym: 'BNTEEE'
                }
            }]
        }, 
        {
            blocksBehind: 3,
            expireSeconds: 30,
        })

        await expectError(
            delconverter,
            'missing authority'
        )
    })
    for (const converterToBeMigrated of singleLiquidityProviderMigrations)
        it(`[end2end] single liquidity provider - ${converterToBeMigrated.converter}`, async () => singleLiquidityProviderEndToEnd(converterToBeMigrated))
    
    for (const converterToBeMigrated of multipleLiquidityProvidersMigrations)
        it(`[end2end] multiple liquidity providers - ${converterToBeMigrated.converter}`, async () => multipleLiquidityProvidersEndToEnd(converterToBeMigrated))
        
})

async function singleLiquidityProviderEndToEnd(converterToBeMigrated) {
    const newPoolTokenSym = converterToBeMigrated.relay.symbol.split('BNT')[1] + 'BNT'
    const { rows: [oldConverterSettings] } = await getTableRows(converterToBeMigrated.converter, converterToBeMigrated.converter, 'settings')
    
    const preMigrationReserveABalance = await getBalance(converterToBeMigrated.converter, 'bntbntbntbnt', 'BNT');
    const preMigrationReserveBBalance = await getBalance(converterToBeMigrated.converter, converterToBeMigrated.reserve.account, converterToBeMigrated.reserve.symbol);
    
    const totalSupply = await getBalance(testAccount1, converterToBeMigrated.relay.account, converterToBeMigrated.relay.symbol);
    await transfer(converterToBeMigrated.relay.account, testAccount1, migrationContract, totalSupply, '');
    
    const { rows: [newConverter] } = await getTableRows(bancorConverter, newPoolTokenSym, 'converters')
    
    const postMigrationReserveABalance = await getReserveBalance(newPoolTokenSym, bancorConverter, 'BNT');
    const postMigrationReserveBBalance = await getReserveBalance(newPoolTokenSym, bancorConverter, converterToBeMigrated.reserve.symbol);
    
    assert.equal(preMigrationReserveABalance, postMigrationReserveABalance, 'unexpected reserve balance')
    assert.equal(preMigrationReserveBBalance, postMigrationReserveBBalance, 'unexpected reserve balance')

    assert.equal(newConverter.fee, oldConverterSettings.fee, "unexpected fee")
    assert.equal(newConverter.owner, testAccount1, "unexpected converter owner")
    assert.equal(newConverter.currency.split(',')[1], newPoolTokenSym, "unexpected pool token symbol")

    const { rows: [poolTokenStats] } = await getTableRows(multiTokens, newPoolTokenSym, 'stat')
    const poolTokenBalance = await getBalance(testAccount1, multiTokens, newPoolTokenSym);
    assert.equal(poolTokenStats.supply, poolTokenBalance)

    await expectNoError(
        convert('1.00000000 BNT', 'bntbntbntbnt', [`${bancorConverter}:${newPoolTokenSym}`, converterToBeMigrated.reserve.symbol])
    )
}

async function multipleLiquidityProvidersEndToEnd(converterToBeMigrated) {
    const newPoolTokenSym = converterToBeMigrated.relay.symbol.split('BNT')[1] + 'BNT'
    const { rows: [oldConverterSettings] } = await getTableRows(converterToBeMigrated.converter, converterToBeMigrated.converter, 'settings')
    
    const preMigrationOldConverterReserveABalance = await getBalance(converterToBeMigrated.converter, 'bntbntbntbnt', 'BNT');
    const preMigrationOldConverterReserveBBalance = await getBalance(converterToBeMigrated.converter, converterToBeMigrated.reserve.account, converterToBeMigrated.reserve.symbol);
    
    const testAccount1PoolTokens = await getBalance(testAccount1, converterToBeMigrated.relay.account, converterToBeMigrated.relay.symbol);
    await transfer(converterToBeMigrated.relay.account, testAccount1, migrationContract, testAccount1PoolTokens, '');
    
    const { rows: [newConverter] } = await getTableRows(bancorConverter, newPoolTokenSym, 'converters')
    
    const postMigrationOldConverterReserveABalance = await getBalance(converterToBeMigrated.converter, 'bntbntbntbnt', 'BNT');
    const postMigrationOldConverterReserveBBalance = await getBalance(converterToBeMigrated.converter, converterToBeMigrated.reserve.account, converterToBeMigrated.reserve.symbol);
    const postMigrationNewConverterReserveABalance = await getReserveBalance(newPoolTokenSym, bancorConverter, 'BNT');
    const postMigrationNewConverterReserveBBalance = await getReserveBalance(newPoolTokenSym, bancorConverter, converterToBeMigrated.reserve.symbol);
    
    assert.equal(
        Decimal(preMigrationOldConverterReserveABalance.split(' ')[0]).sub(postMigrationOldConverterReserveABalance.split(' ')[0]).toFixed(),
        Decimal(postMigrationNewConverterReserveABalance.split(' ')[0]).toFixed(),
        'unexpected reserve balance'
    )
    assert.equal(
        Decimal(preMigrationOldConverterReserveBBalance.split(' ')[0]).sub(postMigrationOldConverterReserveBBalance.split(' ')[0]).toFixed(),
        Decimal(postMigrationNewConverterReserveBBalance.split(' ')[0]).toFixed(),
        'unexpected reserve balance'
    )

    assert.equal(newConverter.fee, oldConverterSettings.fee, "unexpected fee")
    assert.equal(newConverter.owner, testAccount1, "unexpected converter owner")
    assert.equal(newConverter.currency.split(',')[1], newPoolTokenSym, "unexpected pool token symbol")

    const { rows: [poolTokenStats] } = await getTableRows(multiTokens, newPoolTokenSym, 'stat')
    const poolTokenBalance = await getBalance(testAccount1, multiTokens, newPoolTokenSym);
    assert.equal(poolTokenStats.supply, poolTokenBalance)
    await expectNoError(
        convert('1.10204000 BNT', 'bntbntbntbnt', [`${bancorConverter}:${newPoolTokenSym}`, converterToBeMigrated.reserve.symbol])
    )
    const testAccount2oldPoolTokens = await getBalance(testAccount2, converterToBeMigrated.relay.account, converterToBeMigrated.relay.symbol);
    await transfer(converterToBeMigrated.relay.account, testAccount2, migrationContract, testAccount2oldPoolTokens, '');
    await expectNoError(
        convert('1.00000000 BNT', 'bntbntbntbnt', [`${bancorConverter}:${newPoolTokenSym}`, converterToBeMigrated.reserve.symbol])
    )
}