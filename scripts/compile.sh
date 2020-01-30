#!/bin/bash
shopt -s expand_aliases
source ~/.bash_profile
source ~/.profile
source ~/.bashrc

GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${GREEN}Compiling ...${NC}"
eosio-cpp ./src/MultiConverterMigration/MultiConverterMigration.cpp -o ./build/MultiConverterMigration/MultiConverterMigration.wasm --abigen -I.
eosio-cpp ./src/BancorConverter/BancorConverter.cpp -o ./build/BancorConverter/BancorConverter.wasm --abigen -I.
