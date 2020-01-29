#pragma once

#include <vector> 

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/singleton.hpp>

#include "../LegacyBancorConverter/LegacyBancorConverter.hpp"


using namespace eosio;
using namespace std;

CONTRACT MultiConverterMigration : public contract {
    public:
        using contract::contract;

        enum EMigrationStage {
            INITIAL = 0,
            CONVERTER_CREATED = 1,
            LIQUIDATION = 2,
            FUNDING = 3,
            DONE = 4
        };
        friend EMigrationStage operator++ (EMigrationStage& s, int) {
            int final_stage = static_cast<int>(EMigrationStage(DONE));
            int next_stage = static_cast<int>(s) + 1;
            check(next_stage <= final_stage, "EMigrationStage overflow");
            return static_cast<EMigrationStage>(next_stage);
        }

        TABLE migration_t {
            name account;
            symbol currency; 
            uint8_t stage;
            name next_owner;
            uint64_t primary_key() const { return currency.code().raw(); }
        };
        
        TABLE converter_currency_t {
            symbol_code sym;
            name        account;
            uint64_t primary_key() const { return sym.raw(); }
        };

        TABLE reserve_balance_t {
            extended_asset reserve;
            uint64_t primary_key() const { return reserve.quantity.symbol.code().raw(); }
        };

        TABLE context_t {
            symbol_code current_converter;
        };

        typedef eosio::multi_index<"cnvrtrs.curr"_n, converter_currency_t> converters_currencies;
        typedef eosio::multi_index<"migrations"_n, migration_t> migrations;
        typedef eosio::multi_index<"rsrvbalances"_n, reserve_balance_t> reserve_balances;

        typedef eosio::singleton<"context"_n, context_t> context;
        typedef eosio::multi_index<"context"_n, context_t> dummy_for_abi; // hack until abi generator generates correct name
        

        ACTION migrate(symbol_code converter_currency_sym);
        ACTION migrate2(symbol_code converter_currency_sym);
        ACTION addcnvrtrcur(symbol_code converter_sym, name converter_account);
        ACTION delcnvrtrcur(symbol_code converter_sym);
        
        [[eosio::on_notify("*::transfer")]]
        void on_transfer(name from, name to, asset quantity, string memo);
    private:
        void create_converter(name from, asset quantity);
        void handle_liquidated_reserve(name from, asset quantity);
        
        void increment_converter_stage(symbol_code converter_currency);
        void clear(symbol_code converter_currency);
        vector<LegacyBancorConverter::reserve_t> get_original_reserves(converter_currency_t converter_currency);
        const LegacyBancorConverter::settings_t& get_original_converter_settings(converter_currency_t converter_currency);
        const converter_currency_t& get_converter_currency(symbol_code sym);
        asset get_balance(name contract, name owner, symbol_code sym);
    
        const name MULTI_CONVERTER = "multiconvert"_n;
        const name MULTI_TOKENS = "multi4tokens"_n;
        const name BANCOR_NETWORK = "thisisbancor"_n;
};
