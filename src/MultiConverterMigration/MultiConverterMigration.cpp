#include <math.h>
#include "MultiConverterMigration.hpp"

#include "../includes/Token.hpp"
#include "../lib/math_utils.cpp"

void MultiConverterMigration::liquidate_old_converter(symbol_code converter_currency_sym){
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter = converters_table.get(converter_currency_sym.raw(), "converter_currency wasn't found");

    context current_context(get_self(), get_self().value);
    check(!current_context.exists(), "context already set");
    current_context.set(context_t{ converter_currency_sym }, get_self());

    const LegacyBancorConverter::settings_t& settings = get_original_converter_settings(converter);
    action(
        permission_level{ converter.account, "manager"_n },
        converter.account, "update"_n,
        make_tuple(settings.smart_enabled, settings.enabled, settings.require_balance, uint64_t(0))
    ).send();

    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    uint8_t reserve_index = 0;
    asset old_pool_tokens = Token::get_balance(settings.smart_contract, get_self(), settings.smart_currency.symbol.code());

    asset first_reserve_liquidation_amount;
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        string lowest_asset = asset(1, reserve.currency.symbol).to_string();

        string conversion_path = converter.account.to_string() + " " + reserve.currency.symbol.code().to_string();
        string min_return = lowest_asset.erase(lowest_asset.find(" "));
        string memo = "1," + conversion_path + "," + min_return + "," + get_self().to_string();

        asset liquidation_amount;
        if (reserve_index == 0) {
            asset supply = Token::get_supply(settings.smart_contract, settings.smart_currency.symbol.code());
            first_reserve_liquidation_amount = asset(calculate_first_reserve_liquidation_amount(supply.amount, old_pool_tokens.amount), old_pool_tokens.symbol);
            liquidation_amount = first_reserve_liquidation_amount;
        }
        else {
            liquidation_amount = old_pool_tokens - first_reserve_liquidation_amount;
        }
        reserve_index++;

        action(
            permission_level{ get_self(), "active"_n },
            settings.smart_contract, "transfer"_n,
            make_tuple(get_self(), BANCOR_NETWORK, liquidation_amount, memo)
        ).send();
    }
    action(
        permission_level{ converter.account, "manager"_n },
        converter.account, "update"_n,
        make_tuple(settings.smart_enabled, settings.enabled, settings.require_balance, settings.fee)
    ).send();

    increment_converter_stage(converter_currency_sym);
}


ACTION MultiConverterMigration::fundexisting(symbol_code converter_currency_sym) {
    require_auth(get_self());
    
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter_currency = converters_table.get(converter_currency_sym.raw(), "converter_currency wasn't found");
    
    BancorConverter::converters new_converters_table(MULTI_CONVERTER, converter_currency_sym.raw());
    const BancorConverter::converter_t& converter = new_converters_table.get(converter_currency_sym.raw());

    reserve_balances reserve_balances_table(get_self(), converter_currency_sym.raw());
    auto reserve_balance = reserve_balances_table.begin();
    while (reserve_balance != reserve_balances_table.end()) {
        string lowest_asset = asset(1, converter.currency).to_string();

        string conversion_path = MULTI_CONVERTER.to_string() + ":" + converter.currency.code().to_string()
            + " " + converter.currency.code().to_string();
        string min_return = lowest_asset.erase(lowest_asset.find(" "));
        string memo = "1," + conversion_path + "," + min_return + "," + get_self().to_string();
        action(
            permission_level{ get_self(), "active"_n },
            reserve_balance->reserve.contract, "transfer"_n,
            make_tuple(get_self(), BANCOR_NETWORK, reserve_balance->reserve.quantity, memo)
        ).send();

        reserve_balance = reserve_balances_table.erase(reserve_balance);
    }
    
    migrations migrations_table(get_self(), converter_currency_sym.raw());
    const migration_t& migration = migrations_table.get(converter_currency_sym.raw(), "cannot find migration data");
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "transferpool"_n,
        make_tuple(migration.migration_initiator, converter_currency_sym)
    ).send();

    increment_converter_stage(converter_currency_sym);
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "assertsucess"_n,
        make_tuple(converter_currency_sym)
    ).send();
}

ACTION MultiConverterMigration::fundnew(symbol_code converter_currency_sym) {
    require_auth(get_self());
    
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter_currency = converters_table.get(converter_currency_sym.raw(), "converter_currency wasn't found");

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
    action(
        permission_level{ get_self(), "active"_n },
        MULTI_CONVERTER, "updateowner"_n,
        make_tuple(converter_currency_sym, migration.migration_initiator)
    ).send();
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "transferpool"_n,
        make_tuple(migration.migration_initiator, converter_currency_sym)
    ).send();

    increment_converter_stage(converter_currency_sym);
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "assertsucess"_n,
        make_tuple(converter_currency_sym)
    ).send();
}

ACTION MultiConverterMigration::transferpool(name to, symbol_code pool_tokens) {
    require_auth(get_self());
    asset new_pool_tokens = Token::get_balance(MULTI_TOKENS, get_self(), pool_tokens);
    action(
        permission_level{ get_self(), "active"_n },
        MULTI_TOKENS, "transfer"_n,
        make_tuple(get_self(), to, new_pool_tokens, string("new converter pool tokens"))
    ).send();
}

ACTION MultiConverterMigration::addconverter(symbol_code converter_sym, name converter_account, name owner) {
    require_auth(get_self());
    converters converters_table(get_self(), converter_sym.raw());
    
    converters_table.emplace(get_self(), [&](auto& cc) {
        cc.sym = converter_sym;
        cc.account = converter_account;
        cc.owner = owner;
    });
}

ACTION MultiConverterMigration::delconverter(symbol_code converter_sym) {
    require_auth(get_self());
    converters converters_table(get_self(), converter_sym.raw());
    const converter_t& converter = converters_table.get(converter_sym.raw(), "converter_currency wasn't found");
    
    converters_table.erase(converter);
}

ACTION MultiConverterMigration::assertsucess(symbol_code converter_sym) {
    require_auth(get_self());
    converters converters_table(get_self(), converter_sym.raw());
    const converter_t& converter = converters_table.get(converter_sym.raw(), "converter_currency wasn't found"); 
    const LegacyBancorConverter::settings_t& settings = get_original_converter_settings(converter);
    asset old_pool_tokens = Token::get_balance(settings.smart_contract, get_self(), settings.smart_currency.symbol.code());
    asset new_pool_tokens = Token::get_balance(MULTI_TOKENS, get_self(), converter_sym);
    
    check(old_pool_tokens.amount == 0, "migration contract's old pool tokens balance is not 0");
    check(new_pool_tokens.amount == 0, "migration contract's new pool tokens balance is not 0");

    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        asset reserve_balance = Token::get_balance(reserve.contract, get_self(), reserve.currency.symbol.code());
        check(reserve_balance.amount == 0, "migration contract's reserve tokens balance is not 0");
    }

    clear(converter_sym);
}

void MultiConverterMigration::on_transfer(name from, name to, asset quantity, string memo) {
    if (get_first_receiver() == MULTI_TOKENS || from == get_self() || from == "eosio.ram"_n || from == "eosio.stake"_n || from == "eosio.rex"_n) 
	    return;
    
    context current_context(get_self(), get_self().value);
    symbol_code converter_currency;
    if (current_context.exists())
        converter_currency = current_context.get().current_converter;
    else
        converter_currency = quantity.symbol.code();
        
    migrations migrations_table(get_self(), converter_currency.raw());
    const auto migration = migrations_table.find(converter_currency.raw());

    uint8_t current_stage = EMigrationStage::INITIAL;
    if (migration != migrations_table.end())
        current_stage = migration->stage;

    switch(current_stage) {
        case EMigrationStage::INITIAL : {
            bool converter_exists = does_converter_exist(quantity.symbol.code());
            init_migration(from, quantity, converter_exists);
            if (!converter_exists)
                create_converter(from, quantity);
            liquidate_old_converter(quantity.symbol.code());
            if (converter_exists)
                action( 
                    permission_level{ get_self(), "active"_n },
                    get_self(), "fundexisting"_n,
                    make_tuple(quantity.symbol.code())
                ).send();
            else
                action( 
                    permission_level{ get_self(), "active"_n },
                    get_self(), "fundnew"_n,
                    make_tuple(quantity.symbol.code())
                ).send();
            break;   
        }
        case EMigrationStage::LIQUIDATION : {
            handle_liquidated_reserve(from, quantity);
            break;
        }
        default: {
            check(false, "should not happen");
        }
    }
}

void MultiConverterMigration::create_converter(name from, asset quantity) {
    const converter_t& converter = get_converter(quantity.symbol.code());
    require_auth(converter.owner);

    const LegacyBancorConverter::settings_t& settings = get_original_converter_settings(converter);
    check(settings.smart_contract == get_first_receiver(), "unknown token contract");

    double initial_supply = quantity.amount / pow(10, quantity.symbol.precision());
    action( 
        permission_level{ get_self(), "active"_n },
        MULTI_CONVERTER, "create"_n,
        make_tuple(get_self(), quantity.symbol.code(), initial_supply)
    ).send();

    action( 
        permission_level{ get_self(), "active"_n },
        MULTI_CONVERTER, "updatefee"_n,
        make_tuple(quantity.symbol.code(), settings.fee)
    ).send();

    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        action(
            permission_level{ get_self(), "active"_n },
            MULTI_CONVERTER, "setreserve"_n,
            make_tuple(quantity.symbol.code(), reserve.currency.symbol, reserve.contract, reserve.ratio)
        ).send();
    }

}

void MultiConverterMigration::handle_liquidated_reserve(name from, asset quantity) {
    context current_context(get_self(), get_self().value);
    const symbol_code& current_converter = current_context.get().current_converter;
    if (current_converter == quantity.symbol.code()) return; // ignore pool token issuance notification

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

void MultiConverterMigration::init_migration(name from, asset quantity, bool converter_exists) {
    const converter_t& converter = get_converter(quantity.symbol.code());
    migrations migrations_table(get_self(), quantity.symbol.code().raw());
    migrations_table.emplace(get_self(), [&](auto& c) {
        c.converter_account = converter.account;
        c.currency = quantity.symbol;
        c.migration_initiator = from;
        c.converter_exists = converter_exists;
    });
}
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
    check(migration_data->stage == EMigrationStage::DONE, "cannot clear migation while it's still in progress");
    migrations_table.erase(migration_data);
}

vector<LegacyBancorConverter::reserve_t> MultiConverterMigration::get_original_reserves(MultiConverterMigration::converter_t converter) {
    LegacyBancorConverter::reserves original_converter_reserves_table(converter.account, converter.account.value);

    vector<LegacyBancorConverter::reserve_t> reserves;
    for (const auto& reserve : original_converter_reserves_table) {
        reserves.push_back(reserve);
    }

    return reserves;
}

const BancorConverter::reserve_t& MultiConverterMigration::get_new_converter_reserve(symbol_code converter_sym, symbol_code reserve_sym) {
    BancorConverter::reserves new_converter_reserves_table(MULTI_CONVERTER, converter_sym.raw());

    return new_converter_reserves_table.get(reserve_sym.raw());
}

const MultiConverterMigration::converter_t& MultiConverterMigration::get_converter(symbol_code sym) {
    converters converters_table(get_self(), sym.raw());
    const converter_t& converter_currency = converters_table.get(sym.raw());
    return converter_currency;
}


const LegacyBancorConverter::settings_t& MultiConverterMigration::get_original_converter_settings(MultiConverterMigration::converter_t converter) {
    LegacyBancorConverter::settings original_converter_settings_table(converter.account, converter.account.value);
    const auto& st = original_converter_settings_table.get("settings"_n.value, "converter settings do not exist");
    
    return st;
}

bool MultiConverterMigration::does_converter_exist(symbol_code sym) {
    BancorConverter::converters new_converters_table(MULTI_CONVERTER, sym.raw());
    return new_converters_table.find(sym.raw()) != new_converters_table.end();
}

double MultiConverterMigration::calculate_first_reserve_liquidation_amount(double pool_token_supply, double quantity) {
    const double a = 1;
    const double b = -2 * pool_token_supply;
    const double c = quantity * pool_token_supply;
    const auto [x1, x2] = find_quadratic_roots(a, b, c);

    if (x1 > 1.0 && x1 <= quantity)
        return x1 - 1.0;
    else if (x2 > 1.0 && x2 <= quantity)
        return x2 - 1.0;
        
    check(false, "couldn't find valid quadratic root. x1=" + to_string(x1) + " x2=" + to_string(x2) +", quantity: " + to_string(quantity));
    return 0;
}

// poolTokensToSell4ReserveA / _supply = (poolTokensSent - poolTokensToSell4ReserveA) / (_supply - poolTokensToSell4ReserveA)

// (poolTokensToSell4ReserveA*_supply - poolTokensToSell4ReserveA^2) / (_supply^2 - 2*poolTokensToSell4ReserveA*_supply) = (poolTokensSent*_supply - poolTokensToSell4ReserveA*_supply) / (_supply^2 - 2*poolTokensToSell4ReserveA*_supply)

// 2*poolTokensToSell4ReserveA*_supply - poolTokensToSell4ReserveA^2 - poolTokensSent*_supply =  0

// poolTokensToSell4ReserveA^2 - 2*_supply*poolTokensToSell4ReserveA + poolTokensSent*_supply = 0
