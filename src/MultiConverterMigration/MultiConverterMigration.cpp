#include <math.h>
#include "MultiConverterMigration.hpp"
#include "../includes/Token.hpp"

ACTION MultiConverterMigration::migrate(symbol_code converter_currency_sym){
    converters_currencies converters_currencies_table(get_self(), converter_currency_sym.raw());
    const converter_currency_t& converter_currency = converters_currencies_table.get(converter_currency_sym.raw(), "converter_currency wasn't found");

    context current_context(get_self(), get_self().value);
    check(!current_context.exists(), "context already set");
    current_context.set(context_t{ converter_currency_sym }, get_self());

    const BancorConverter::settings_t& settings = get_original_converter_settings(converter_currency);
    const vector<BancorConverter::reserve_t> reserves = get_original_reserves(converter_currency);
    uint8_t reserve_index = 0;
    asset old_pool_tokens = get_balance(settings.smart_contract, get_self(), settings.smart_currency.symbol.code());
    for (const BancorConverter::reserve_t& reserve : reserves) {
        string lowest_asset = asset(1, reserve.currency.symbol).to_string();

        string conversion_path = converter_currency.account.to_string() + " " + reserve.currency.symbol.code().to_string();
        string min_return = lowest_asset.erase(lowest_asset.find(" "));
        string memo = "1," + conversion_path + "," + min_return + "," + get_self().to_string();

        asset ONE = asset(1, old_pool_tokens.symbol);
        asset liquidation_amount = reserve_index == 0 ? old_pool_tokens - ONE : ONE;
        reserve_index++;

        action(
            permission_level{ get_self(), "active"_n },
            settings.smart_contract, "transfer"_n,
            make_tuple(get_self(), BANCOR_NETWORK, liquidation_amount, memo)
        ).send();
    }
    // add a check to ensure total ratio is 100%

    increment_converter_stage(converter_currency_sym);
}


ACTION MultiConverterMigration::migrate2(symbol_code converter_currency_sym){
    converters_currencies converters_currencies_table(get_self(), converter_currency_sym.raw());
    const converter_currency_t& converter_currency = converters_currencies_table.get(converter_currency_sym.raw(), "converter_currency wasn't found");

    reserve_balances reserve_balances_table(get_self(), converter_currency_sym.raw());
    auto reserve_balance = reserve_balances_table.begin();
    while (reserve_balance != reserve_balances_table.end()) {
        const string memo = "fund;" + converter_currency_sym.to_string();
        action(
            permission_level{ get_self(), "active"_n },
            reserve_balance->reserve.contract, "transfer"_n,
            make_tuple(get_self(), MULTI_CONVERTER, reserve_balance->reserve.quantity, memo)
        ).send();

        reserve_balance = reserve_balances_table.erase(reserve_balance);
    }
    migrations migrations_table(get_self(), converter_currency_sym.raw());
    const migration_t& migration = migrations_table.get(converter_currency_sym.raw(), "cannot find migration data");

    asset new_pool_tokens = get_balance(MULTI_TOKENS, get_self(), migration.currency.code());
    action(
        permission_level{ get_self(), "active"_n },
        MULTI_CONVERTER, "transfer"_n,
        make_tuple(get_self(), migration.next_owner, new_pool_tokens, string("new converter pool tokens"))
    ).send();

    increment_converter_stage(converter_currency_sym);
    clear(converter_currency_sym);
}

ACTION MultiConverterMigration::addcnvrtrcur(symbol_code converter_sym, name converter_account) {
    require_auth(get_self());
    converters_currencies converters_currencies_table(get_self(), converter_sym.raw());
    
    converters_currencies_table.emplace(get_self(), [&](auto& cc) {
        cc.sym = converter_sym;
        cc.account = converter_account;
    });
}

ACTION MultiConverterMigration::delcnvrtrcur(symbol_code converter_sym) {
    require_auth(get_self());
    converters_currencies converters_currencies_table(get_self(), converter_sym.raw());
    const auto& converter_currency = converters_currencies_table.get(converter_sym.raw(), "converter_currency wasn't found");
    
    converters_currencies_table.erase(converter_currency);
}

void MultiConverterMigration::on_transfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || from == "eosio.ram"_n || from == "eosio.stake"_n || from == "eosio.rex"_n) 
	    return;
    
    context current_context(get_self(), get_self().value);
    symbol_code converter_currency;
    if (current_context.exists()) {
        converter_currency = current_context.get().current_converter;
    }
    else {
        converter_currency = quantity.symbol.code();
    }
    migrations migrations_table(get_self(), converter_currency.raw());
    const auto migration = migrations_table.find(converter_currency.raw());

    uint8_t current_stage = EMigrationStage::INITIAL;
    if (migration != migrations_table.end())
        current_stage = migration->stage;

    switch(current_stage) {
        case EMigrationStage::INITIAL :
            create_converter(from, quantity);
            break;
        case EMigrationStage::CONVERTER_CREATED :
            break;
        case EMigrationStage::LIQUIDATION :
            handle_liquidated_reserve(from, quantity);
            break;
        case EMigrationStage::FUNDING :
            break;
        case EMigrationStage::DONE :
            break;
    }
}

void MultiConverterMigration::create_converter(name from, asset quantity) {
    converter_currency_t converter_currency = get_converter_currency(quantity.symbol.code());
    const BancorConverter::settings_t& settings = get_original_converter_settings(converter_currency);
    check(settings.smart_contract == get_first_receiver(), "unknown token contract");

    migrations migrations_table(get_self(), quantity.symbol.code().raw());
    migrations_table.emplace(get_self(), [&](auto& c) {
        c.account = converter_currency.account;
        c.currency = quantity.symbol;
        c.next_owner = from;
    });

    double initial_supply = quantity.amount / pow(10, quantity.symbol.precision());
    action( 
        permission_level{ get_self(), "active"_n },
        MULTI_CONVERTER, "create"_n,
        make_tuple(get_self(), quantity.symbol.code(), initial_supply)
    ).send();

    const vector<BancorConverter::reserve_t> reserves = get_original_reserves(converter_currency);
    for (const BancorConverter::reserve_t& reserve : reserves) {
        action(
            permission_level{ get_self(), "active"_n },
            MULTI_CONVERTER, "setreserve"_n,
            make_tuple(quantity.symbol.code(), reserve.currency.symbol, reserve.contract, reserve.ratio)
        ).send();
    }

    increment_converter_stage(quantity.symbol.code());
}

void MultiConverterMigration::handle_liquidated_reserve(name from, asset quantity) {
    context current_context(get_self(), get_self().value);
    const symbol_code current_converter = current_context.get().current_converter;

    reserve_balances reserve_balances_table(get_self(), current_converter.raw());
    const auto reserve = reserve_balances_table.find(quantity.symbol.code().raw());
    
    if (reserve == reserve_balances_table.end()) {
        reserve_balances_table.emplace(get_self(), [&](auto& r) {
            r.reserve = extended_asset(quantity, get_first_receiver());
        });
    }
    else { check(false, "not supported"); }

    uint8_t reserves_length = std::distance(reserve_balances_table.begin(), reserve_balances_table.end());
    if (reserves_length >= 2) {
        increment_converter_stage(current_converter); 
        current_context.remove();
    }  
}

// helpers

void MultiConverterMigration::increment_converter_stage(symbol_code converter_currency) {
    migrations migrations_table(get_self(), converter_currency.raw());
    const auto migration = migrations_table.find(converter_currency.raw());

    migrations_table.modify(migration, eosio::same_payer, [&](auto& m) {
        m.stage++;
    });
}

void MultiConverterMigration::clear(symbol_code converter_currency) {
    migrations migrations_table(get_self(), converter_currency.raw());
    const auto migration_data = migrations_table.find(converter_currency.raw());

    migrations_table.erase(migration_data);
}

vector<BancorConverter::reserve_t> MultiConverterMigration::get_original_reserves(MultiConverterMigration::converter_currency_t converter_currency) {
    BancorConverter::reserves original_converter_reserves_table(converter_currency.account, converter_currency.account.value);

    vector<BancorConverter::reserve_t> reserves;
    for (const auto& reserve : original_converter_reserves_table) {
        reserves.push_back(reserve);
    }

    return reserves;
}

const MultiConverterMigration::converter_currency_t& MultiConverterMigration::get_converter_currency(symbol_code sym) {
    converters_currencies converters_currencies_table(get_self(), sym.raw());
    const converter_currency_t& converter_currency = converters_currencies_table.get(sym.raw());

    return converter_currency;
}


const BancorConverter::settings_t& MultiConverterMigration::get_original_converter_settings(MultiConverterMigration::converter_currency_t converter_currency) {
    BancorConverter::settings original_converter_settings_table(converter_currency.account, converter_currency.account.value);
    const auto& st = original_converter_settings_table.get("settings"_n.value, "converter settings do not exist");
    
    return st;
}

// returns the balance object for an account
asset MultiConverterMigration::get_balance(name contract, name owner, symbol_code sym) {
    Token::accounts accountstable(contract, owner.value);
    const auto& ac = accountstable.get(sym.raw(), "cannot find balance row");
    return ac.balance;
}


// 1) (Receive old pool tokens) on_transfer
//    - Receive pool tokens 
//    - Creates a new converter in the multi converter deployment (if such a converter doesn't already exist)
// Note: should be able to withdraw here! add a withdraw method (maybe)
// 2) MultiConverterMigration::migrate(symbol_code sym)
//    - Liquidates the pool tokens in the old converter
// 3) (Receive reserve A) on_transfer
// 4) (Receive reserve B) on_transfer
//    - Funds the new pool with the reserves
// 5) (Receive new pool tokens) on_transfer
//    - Sends the new pool tokens back to the original sender
//    - Transfers ownership of the converter to the pool tokens owner (if the converter was actually created now)
