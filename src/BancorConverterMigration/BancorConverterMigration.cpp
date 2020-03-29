#include <math.h>
#include "BancorConverterMigration.hpp"

#include "../includes/Token.hpp"
#include "../lib/math_utils.cpp"

inline BancorConverterMigration::BancorConverterMigration(name receiver, name code, datastream<const char *> ds):contract(receiver, code, ds),
    st(receiver, receiver.value),
    p_global_settings(st.find("settings"_n.value)) {}


void BancorConverterMigration::liquidate_old_converter(symbol_code converter_currency_sym){
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter = converters_table.get(converter_currency_sym.raw(), "[liquidate_old_converter] converter_currency wasn't found");

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
            make_tuple(get_self(), p_global_settings->network, liquidation_amount, memo)
        ).send();
    }
    action(
        permission_level{ converter.account, "manager"_n },
        converter.account, "update"_n,
        make_tuple(settings.smart_enabled, settings.enabled, settings.require_balance, settings.fee)
    ).send();

    increment_converter_stage(converter_currency_sym);
}


ACTION BancorConverterMigration::fundexisting(symbol_code converter_currency_sym) {
    require_auth(get_self());
    check(p_global_settings != st.end(), "settings must be initialized");
    
    migrations migrations_table(get_self(), get_self().value);
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter_currency = converters_table.get(converter_currency_sym.raw(), "[fundexisting] converter_currency wasn't found");
    const migration_t& migration = migrations_table.get();
    
    BancorConverter::converters new_converters_table(p_global_settings->bancor_converter, migration.new_pool_token.raw());
    const BancorConverter::converter_t& converter = new_converters_table.get(migration.new_pool_token.raw(), "converter not found");
    

    double funding_pool_return = std::numeric_limits<double>::infinity();
    reserve_balances reserve_balances_table(get_self(), converter_currency_sym.raw());
    auto reserve_balance = reserve_balances_table.begin();
    while (reserve_balance != reserve_balances_table.end()) {
        double supply = Token::get_supply(p_global_settings->multi_token, migration.new_pool_token).amount;
        double converter_reserve_balance = get_new_converter_reserve(migration.new_pool_token, reserve_balance->reserve.quantity.symbol.code()).balance.amount;
        funding_pool_return = std::min(funding_pool_return, calculate_fund_pool_return(reserve_balance->reserve.quantity.amount, converter_reserve_balance, supply));
        
        string memo = "fund;" + migration.new_pool_token.to_string();
        action(
            permission_level{ get_self(), "active"_n },
            reserve_balance->reserve.contract, "transfer"_n,
            make_tuple(get_self(), p_global_settings->bancor_converter, reserve_balance->reserve.quantity, memo)
        ).send();
        reserve_balance = reserve_balances_table.erase(reserve_balance);
    }

    increment_converter_stage(converter_currency_sym);

    asset funding_amount = asset(funding_pool_return, converter.currency);
    action(
        permission_level{ get_self(), "active"_n },
        p_global_settings->bancor_converter, "fund"_n,
        make_tuple(get_self(), funding_amount)
    ).send();
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "refundrsrvs"_n,
        make_tuple(converter_currency_sym)
    ).send();
    
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "transferpool"_n,
        make_tuple(migration.migration_initiator, migration.new_pool_token)
    ).send();

    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "assertsucess"_n,
        make_tuple(converter_currency_sym)
    ).send();
}

ACTION BancorConverterMigration::fundnew(symbol_code converter_currency_sym) {
    require_auth(get_self());
    check(p_global_settings != st.end(), "settings must be initialized");

    migrations migrations_table(get_self(), get_self().value);
    converters converters_table(get_self(), converter_currency_sym.raw());
    const converter_t& converter_currency = converters_table.get(converter_currency_sym.raw(), "[fundnew] converter_currency wasn't found");
    const migration_t& migration = migrations_table.get();
    
    reserve_balances reserve_balances_table(get_self(), converter_currency_sym.raw());
    auto reserve_balance = reserve_balances_table.begin();
    while (reserve_balance != reserve_balances_table.end()) {
        const string memo = "fund;" + migration.new_pool_token.to_string();
        action(
            permission_level{ get_self(), "active"_n },
            reserve_balance->reserve.contract, "transfer"_n,
            make_tuple(get_self(), p_global_settings->bancor_converter, reserve_balance->reserve.quantity, memo)
        ).send();

        reserve_balance = reserve_balances_table.erase(reserve_balance);
    }
    action(
        permission_level{ get_self(), "active"_n },
        p_global_settings->bancor_converter, "updateowner"_n,
        make_tuple(migration.new_pool_token, migration.migration_initiator)
    ).send();
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "transferpool"_n,
        make_tuple(migration.migration_initiator, migration.new_pool_token)
    ).send();

    increment_converter_stage(converter_currency_sym);
    action(
        permission_level{ get_self(), "active"_n },
        get_self(), "assertsucess"_n,
        make_tuple(converter_currency_sym)
    ).send();
}

ACTION BancorConverterMigration::transferpool(name to, symbol_code pool_tokens) {
    require_auth(get_self());
    check(p_global_settings != st.end(), "settings must be initialized");

    asset new_pool_tokens = Token::get_balance(p_global_settings->multi_token, get_self(), pool_tokens);
    action(
        permission_level{ get_self(), "active"_n },
        p_global_settings->multi_token, "transfer"_n,
        make_tuple(get_self(), to, new_pool_tokens, string("new converter pool tokens"))
    ).send();
}

ACTION BancorConverterMigration::refundrsrvs(symbol_code converter_pool_token) {
    require_auth(get_self());
    check(p_global_settings != st.end(), "settings must be initialized");

    migrations migrations_table(get_self(), get_self().value);
    const migration_t& migration = migrations_table.get();
    
    BancorConverter::reserves new_converter_reserves_table(p_global_settings->bancor_converter, migration.new_pool_token.raw());

    for (const BancorConverter::reserve_t& reserve : new_converter_reserves_table) {
        BancorConverter::accounts accounts_balances_table(p_global_settings->bancor_converter, get_self().value);

        const uint128_t secondary_key = BancorConverter::_by_cnvrt(reserve.balance, migration.new_pool_token);
        const auto index = accounts_balances_table.get_index<"bycnvrt"_n >();
        const auto account_balance = index.find(secondary_key);
        
        if (account_balance != index.end() && account_balance->quantity.amount > 0) {
            action(
                permission_level{ get_self(), "active"_n },
                p_global_settings->bancor_converter, "withdraw"_n,
                make_tuple(get_self(), account_balance->quantity,migration.new_pool_token)
            ).send();

            const string memo = "pool tokens migration reserves refund";
            action(
                permission_level{ get_self(), "active"_n },
                reserve.contract, "transfer"_n,
                make_tuple(get_self(), migration.migration_initiator, account_balance->quantity, memo)
            ).send();
        }
    }
}

ACTION BancorConverterMigration::addconverter(symbol_code converter_sym, name converter_account, name owner) {
    require_auth(get_self());
    converters converters_table(get_self(), converter_sym.raw());
    
    converters_table.emplace(get_self(), [&](auto& cc) {
        cc.sym = converter_sym;
        cc.account = converter_account;
        cc.owner = owner;
    });
}

ACTION BancorConverterMigration::delconverter(symbol_code converter_sym) {
    require_auth(get_self());
    converters converters_table(get_self(), converter_sym.raw());
    const converter_t& converter = converters_table.get(converter_sym.raw(), "[delconverter] converter_currency wasn't found");
    
    converters_table.erase(converter);
}

ACTION BancorConverterMigration::setsettings(name bancor_converter, name multi_token, name network) {   
    require_auth(get_self());

    check(is_account(bancor_converter), "bancor_converter is not an account");
    check(is_account(multi_token), "multi_token is not an account");
    check(is_account(network), "network is not an account");

    if (p_global_settings == st.end()) {
        st.emplace(get_self(), [&](auto& s) {
            s.bancor_converter = bancor_converter;
            s.multi_token = multi_token;
            s.network = network;
        });
    }
    else {
        st.modify(p_global_settings, same_payer, [&](auto& s) {
            s.bancor_converter = bancor_converter;
            s.multi_token = multi_token;
            s.network = network;  
        });
    }
}

ACTION BancorConverterMigration::assertsucess(symbol_code converter_sym) {
    require_auth(get_self());
    check(p_global_settings != st.end(), "settings must be initialized");
    
    migrations migrations_table(get_self(), get_self().value);
    converters converters_table(get_self(), converter_sym.raw());
    const migration_t& migration = migrations_table.get();
    const converter_t& converter = converters_table.get(converter_sym.raw(), "[assertsucess] converter_currency wasn't found"); 
    
    const LegacyBancorConverter::settings_t& settings = get_original_converter_settings(converter);
    
    asset old_pool_tokens = Token::get_balance(settings.smart_contract, get_self(), settings.smart_currency.symbol.code());
    asset new_pool_tokens = Token::get_balance(p_global_settings->multi_token, get_self(), migration.new_pool_token);
    
    check(old_pool_tokens.amount == 0, "migration contract's old pool tokens balance is not 0");
    check(new_pool_tokens.amount == 0, "migration contract's new pool tokens balance is not 0");

    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        asset reserve_balance = Token::get_balance(reserve.contract, get_self(), reserve.currency.symbol.code());
        check(reserve_balance.amount == 0, "migration contract's reserve tokens balance is not 0");
    }
    
    clear(converter_sym);
}

void BancorConverterMigration::on_transfer(name from, name to, asset quantity, string memo) {
    check(p_global_settings != st.end(), "settings must be initialized");

    if (get_first_receiver() == p_global_settings->multi_token || from == get_self() || from == "eosio.ram"_n || from == "eosio.stake"_n || from == "eosio.rex"_n) 
	    return;
    
    converters converters_table(get_self(), quantity.symbol.code().raw());
    migrations migrations_table(get_self(), get_self().value);

    uint8_t current_stage = EMigrationStage::INITIAL;
    if (migrations_table.exists()) {
        current_stage = migrations_table.get().stage;

        const auto converter = converters_table.find(quantity.symbol.code().raw());
        if (converter == converters_table.end() && current_stage != EMigrationStage::LIQUIDATION)
            return;
    }

    switch(current_stage) {
        case EMigrationStage::INITIAL : {
            const symbol_code new_converter_sym = generate_converter_symbol(quantity.symbol.code());
            bool converter_exists = does_converter_exist(new_converter_sym);
            init_migration(from, quantity, converter_exists, new_converter_sym);
            if (!converter_exists)
                create_converter(from, quantity, new_converter_sym);
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

void BancorConverterMigration::create_converter(name from, asset quantity, const symbol_code& new_pool_token) {
    const converter_t& converter = get_converter(quantity.symbol.code());
    require_auth(converter.owner);

    const LegacyBancorConverter::settings_t& settings = get_original_converter_settings(converter);
    check(settings.smart_contract == get_first_receiver(), "unknown token contract");

    double initial_supply = quantity.amount / pow(10, quantity.symbol.precision());
    action( 
        permission_level{ get_self(), "active"_n },
        p_global_settings->bancor_converter, "create"_n,
        make_tuple(get_self(), new_pool_token, initial_supply)
    ).send();

    action( 
        permission_level{ get_self(), "active"_n },
        p_global_settings->bancor_converter, "updatefee"_n,
        make_tuple(new_pool_token, settings.fee)
    ).send();

    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        action(
            permission_level{ get_self(), "active"_n },
            p_global_settings->bancor_converter, "setreserve"_n,
            make_tuple(new_pool_token, reserve.currency.symbol, reserve.contract, reserve.ratio)
        ).send();
    }

}

void BancorConverterMigration::handle_liquidated_reserve(name from, asset quantity) {
    migrations migrations_table(get_self(), get_self().value);
    const symbol_code& current_converter = migrations_table.get().old_pool_token.code();
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
    if (reserves_length >= 2)
        increment_converter_stage(current_converter);
}

// helpers

void BancorConverterMigration::init_migration(name from, asset quantity, bool converter_exists, const symbol_code& new_pool_token) {
    const converter_t& converter = get_converter(quantity.symbol.code());
    migrations migrations_table(get_self(), get_self().value);
    const migration_t migration = migration_t{
        quantity.symbol,
        new_pool_token,
        converter.account,
        EMigrationStage::INITIAL,
        from,
        converter_exists
    };
    migrations_table.set(migration, get_self());
}
void BancorConverterMigration::increment_converter_stage(symbol_code converter_currency) {
    migrations migrations_table(get_self(), get_self().value);
    migration_t migration = migrations_table.get();
    migration.stage++;

    migrations_table.set(migration, get_self());
}

void BancorConverterMigration::clear(symbol_code converter_currency) {
    migrations migrations_table(get_self(), get_self().value);
    const migration_t migration_data = migrations_table.get();
    check(migration_data.stage == EMigrationStage::DONE, "cannot clear migation while it's still in progress");
    migrations_table.remove();
}

vector<LegacyBancorConverter::reserve_t> BancorConverterMigration::get_original_reserves(BancorConverterMigration::converter_t converter) {
    LegacyBancorConverter::reserves original_converter_reserves_table(converter.account, converter.account.value);

    vector<LegacyBancorConverter::reserve_t> reserves;
    for (const auto& reserve : original_converter_reserves_table) {
        reserves.push_back(reserve);
    }

    return reserves;
}

const BancorConverter::reserve_t& BancorConverterMigration::get_new_converter_reserve(symbol_code converter_sym, symbol_code reserve_sym) {
    BancorConverter::reserves new_converter_reserves_table(p_global_settings->bancor_converter, converter_sym.raw());

    return new_converter_reserves_table.get(reserve_sym.raw(), "reserve not found");
}

const BancorConverterMigration::converter_t& BancorConverterMigration::get_converter(symbol_code sym) {
    converters converters_table(get_self(), sym.raw());
    const converter_t& converter_currency = converters_table.get(sym.raw(), "converter not found");
    return converter_currency;
}


const LegacyBancorConverter::settings_t& BancorConverterMigration::get_original_converter_settings(BancorConverterMigration::converter_t converter) {
    LegacyBancorConverter::settings original_converter_settings_table(converter.account, converter.account.value);
    const auto& st = original_converter_settings_table.get("settings"_n.value, "converter settings do not exist");
    
    return st;
}

const symbol_code BancorConverterMigration::generate_converter_symbol(symbol_code old_sym) {
    converters converters_table(get_self(), old_sym.raw());
    const converter_t& converter = converters_table.get(old_sym.raw(), "[generate_converter_symbol] converter_currency wasn't found");
    const vector<LegacyBancorConverter::reserve_t> reserves = get_original_reserves(converter);
    
    symbol_code converter_reserve;
    for (const LegacyBancorConverter::reserve_t& reserve : reserves) {
        if (reserve.currency.symbol.code() != NETWORK_TOKEN_CODE) {
            converter_reserve = reserve.currency.symbol.code();
            break;
        }
    }
    check(converter_reserve.to_string() != "", "couldn't find reserve code");
    
    return symbol_code(converter_reserve.to_string() + NETWORK_TOKEN_CODE.to_string());
}

bool BancorConverterMigration::does_converter_exist(symbol_code sym) {
    BancorConverter::converters new_converters_table(p_global_settings->bancor_converter, sym.raw());
    return new_converters_table.find(sym.raw()) != new_converters_table.end();
}

double BancorConverterMigration::calculate_first_reserve_liquidation_amount(double pool_token_supply, double quantity) {
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

// inputReserve * supply / reserveBalance = amount
double BancorConverterMigration::calculate_fund_pool_return(double funding_amount, double reserve_balance, double supply) {
    return supply * funding_amount / reserve_balance;
}

// poolTokensToSell4ReserveA / _supply = (poolTokensSent - poolTokensToSell4ReserveA) / (_supply - poolTokensToSell4ReserveA)

// (poolTokensToSell4ReserveA*_supply - poolTokensToSell4ReserveA^2) / (_supply^2 - 2*poolTokensToSell4ReserveA*_supply) = (poolTokensSent*_supply - poolTokensToSell4ReserveA*_supply) / (_supply^2 - 2*poolTokensToSell4ReserveA*_supply)

// 2*poolTokensToSell4ReserveA*_supply - poolTokensToSell4ReserveA^2 - poolTokensSent*_supply =  0

// poolTokensToSell4ReserveA^2 - 2*_supply*poolTokensToSell4ReserveA + poolTokensSent*_supply = 0
