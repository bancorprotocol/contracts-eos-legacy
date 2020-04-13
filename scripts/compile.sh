#!/bin/bash
eosiocpp() {
    COMMAND="eosio-cpp $*"
    docker run --rm --name eosio.cdt_v1.7.0 -v $HOME/git/legacy-converter:/project eostudio/eosio.cdt:v1.7.0 /bin/bash -c "$COMMAND"
}

PROJECT_PATH=./project

GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${GREEN}Compiling ...${NC}"

eosiocpp $PROJECT_PATH/src/BancorConverterMigration/BancorConverterMigration.cpp -o $PROJECT_PATH/build/BancorConverterMigration/BancorConverterMigration.wasm --abigen -I.
eosiocpp $PROJECT_PATH/src/LegacyBancorConverter/LegacyBancorConverter.cpp -o $PROJECT_PATH/build/LegacyBancorConverter/LegacyBancorConverter.wasm --abigen -I.
