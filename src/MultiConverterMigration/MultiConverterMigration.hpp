#pragma once

#include <vector> 

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/singleton.hpp>


#include "../LegacyBancorConverter/LegacyBancorConverter.hpp"
#include "../includes/BancorConverter.hpp"


using namespace eosio;
using namespace std;

CONTRACT MultiConverterMigration : public contract {
    public:
        using contract::contract;

        enum EMigrationStage {
            INITIAL = 0,
            LIQUIDATION = 1,
            FUNDING = 2,
            DONE = 3
        };
        friend EMigrationStage operator++ (EMigrationStage& s, int) {
            int final_stage = static_cast<int>(EMigrationStage(DONE));
            int next_stage = static_cast<int>(s) + 1;
            check(next_stage <= final_stage, "EMigrationStage overflow");
            return static_cast<EMigrationStage>(next_stage);
        }

        TABLE migration_t {
            symbol currency;
            name converter_account;
            uint8_t stage;
            name migration_initiator;
            bool converter_exists;
            uint64_t primary_key() const { return currency.code().raw(); }
        };
        
        TABLE converter_t {
            symbol_code sym;
            name        account;
            name        owner;
            uint64_t primary_key() const { return sym.raw(); }
        };

        TABLE reserve_balance_t {
            extended_asset reserve;
            uint64_t primary_key() const { return reserve.quantity.symbol.code().raw(); }
        };

        TABLE context_t {
            symbol_code current_converter;
        };

        typedef eosio::multi_index<"converters"_n, converter_t> converters;
        typedef eosio::multi_index<"migrations"_n, migration_t> migrations;
        typedef eosio::multi_index<"rsrvbalances"_n, reserve_balance_t> reserve_balances;

        typedef eosio::singleton<"context"_n, context_t> context;
        typedef eosio::multi_index<"context"_n, context_t> dummy_for_abi; // hack until abi generator generates correct name
        

        ACTION addconverter(symbol_code converter_sym, name converter_account, name owner);
        ACTION delconverter(symbol_code converter_sym);

        ACTION transferpool(name to, symbol_code pool_tokens);
        ACTION fundexisting(symbol_code converter_currency_sym);
        ACTION fundnew(symbol_code converter_currency_sym);
        ACTION assertsucess(symbol_code converter_sym);
        
        [[eosio::on_notify("*::transfer")]]
        void on_transfer(name from, name to, asset quantity, string memo);
    private:
        void create_converter(name from, asset quantity);
        void liquidate_old_converter(symbol_code converter_currency_sym);
        void handle_liquidated_reserve(name from, asset quantity);
        
        void init_migration(name from, asset quantity, bool converter_exists);
        void increment_converter_stage(symbol_code converter_currency);
        void clear(symbol_code converter_currency);
        vector<LegacyBancorConverter::reserve_t> get_original_reserves(converter_t converter);
        const BancorConverter::reserve_t& get_new_converter_reserve(symbol_code converter_sym, symbol_code reserve_sym);
        const LegacyBancorConverter::settings_t& get_original_converter_settings(converter_t converter);
        const converter_t& get_converter(symbol_code sym);
        bool does_converter_exist(symbol_code sym);
        
        double calculate_first_reserve_liquidation_amount(double pool_token_supply, double quantity);
        
        const name MULTI_CONVERTER = "multiconvert"_n;
        const name MULTI_TOKENS = "multi4tokens"_n;
        const name BANCOR_NETWORK = "thisisbancor"_n;
        const double MAX_RATIO = 1000000.0;
};
