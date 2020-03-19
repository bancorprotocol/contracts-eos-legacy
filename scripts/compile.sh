#!/bin/bash
shopt -s expand_aliases
source ~/.bash_profile
source ~/.profile
source ~/.bashrc

GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${GREEN}Compiling ...${NC}"
eosio-cpp ./src/BancorConverterMigration/BancorConverterMigration.cpp -o ./build/BancorConverterMigration/BancorConverterMigration.wasm --abigen -I.
eosio-cpp ./src/LegacyBancorConverter/LegacyBancorConverter.cpp -o ./build/LegacyBancorConverter/LegacyBancorConverter.wasm --abigen -I.
