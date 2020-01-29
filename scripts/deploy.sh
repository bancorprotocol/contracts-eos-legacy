#!/bin/bash
source ./scripts/common.conf

function cleos() { command cleos --verbose --url=${NODEOS_ENDPOINT} --wallet-url=${WALLET_URL} "$@"; echo $@; }
on_exit(){
  echo -e "${CYAN}cleaning up temporary keosd process & artifacts${NC}";
  kill -9 $TEMP_KEOSD_PID &> /dev/null
  rm -r $WALLET_DIR
}
trap on_exit INT
trap on_exit SIGINT

# start temp keosd
rm -rf $WALLET_DIR
mkdir $WALLET_DIR
keosd --wallet-dir=$WALLET_DIR --unix-socket-path=$UNIX_SOCKET_ADDRESS &> /dev/null &
TEMP_KEOSD_PID=$!
sleep 1

# create temp wallet
cleos wallet create --file /tmp/.temp_keosd_pwd
PASSWORD=$(cat /tmp/.temp_keosd_pwd)
cleos wallet unlock --password="$PASSWORD"


# 1) Import user keys

echo -e "${CYAN}-----------------------CONTRACT / USER KEYS-----------------------${NC}"
cleos wallet import --private-key "$MASTER_PRV_KEY"


cleos system newaccount eosio migration $MASTER_PUB_KEY --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 5000 --transfer

cleos set contract migration $MY_CONTRACTS_BUILD/MultiConverterMigration

cleos set account permission migration active --add-code

cleos push action migration addcnvrtrcur '["BNTBBB", "bnt2bbbcnvrt"]' -p migration
cleos push action bnt2bbbrelay transfer '["bnt2bbbcnvrt", "bnttestuser1", "10000.00000000 BNTBBB", ""]' -p bnt2bbbcnvrt
cleos push action bnt2bbbrelay transfer '["bnttestuser2", "bnttestuser1", "100.00000000 BNTBBB", ""]' -p bnttestuser2

cleos push action bntbntbntbnt open '["migration", "BNT", "eosio"]' -p eosio 
cleos push action bbb open '["migration", "8,BBB", "eosio"]' -p eosio 

on_exit
echo -e "${GREEN}--> Done${NC}"
