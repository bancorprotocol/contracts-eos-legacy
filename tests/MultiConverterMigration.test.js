
const { assert } = require('chai')

const { api, transfer, getBalance, getReserveBalance, getTableRows } = require('./utils')


describe('MultiConverterMigration', () => {
    const testAccount = 'bnttestuser1'
    const migrationContract = 'migration'
    const bancorConverter = 'multiconvert'
    const converterToBeMigrated = { converter: 'bnt2bbbcnvrt', currencySymbol: 'BNTBBB', relay: 'bnt2bbbrelay' }

    it('Basic test', async function () {
        const { rows: [oldConverterSettings] } = await getTableRows(converterToBeMigrated.converter, converterToBeMigrated.converter, 'settings')
        
        const preMigrationReserveABalance = await getBalance(converterToBeMigrated.converter, 'bntbntbntbnt', 'BNT');
        const preMigrationReserveBBalance = await getBalance(converterToBeMigrated.converter, 'bbb', 'BBB');
        
        const totalSupply = await getBalance(testAccount, converterToBeMigrated.relay, converterToBeMigrated.currencySymbol);
        await transfer(converterToBeMigrated.relay, testAccount, migrationContract, totalSupply, '');
        
        await api.transact({ 
            actions: [{
                account: migrationContract,
                name: "migrate",
                authorization: [{
                    actor: testAccount,
                    permission: 'active',
                }],
                data: {
                    converter_currency_sym: converterToBeMigrated.currencySymbol
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
                    converter_currency_sym: converterToBeMigrated.currencySymbol
                }
            }]
        }, 
        {
            blocksBehind: 3,
            expireSeconds: 30,
        })

        const { rows: [newConverter] } = await getTableRows(bancorConverter, converterToBeMigrated.currencySymbol, 'converters')
        
        const postMigrationReserveABalance = await getReserveBalance(converterToBeMigrated.currencySymbol, bancorConverter, 'BNT');
        const postMigrationReserveBBalance = await getReserveBalance(converterToBeMigrated.currencySymbol, bancorConverter, 'BBB');
        
        assert.equal(preMigrationReserveABalance, postMigrationReserveABalance, 'unexpected reserve balance')
        assert.equal(preMigrationReserveBBalance, postMigrationReserveBBalance, 'unexpected reserve balance')

        assert.equal(newConverter.fee, oldConverterSettings.fee, "unexpected fee")
        assert.equal(newConverter.owner, testAccount, "unexpected converter owner")
    })
})
