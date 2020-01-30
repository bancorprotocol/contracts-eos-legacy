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

cleos push action bntbntbntbnt open '["migration", "BNT", "eosio"]' -p eosio 


CONVERTER="bnt2ccccnvrt"
POOL_TOKEN="bnt2cccrelay"
POOL_TOKEN_SYM="BNTCCC"
RESERVE="ccc"
RESERVE_SYM="CCC"
cleos system newaccount eosio $CONVERTER EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $POOL_TOKEN EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $RESERVE EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos set contract $CONVERTER ./build/BancorConverter/
cleos set contract $POOL_TOKEN ./build/eosio.token/
cleos set contract $RESERVE ./build/eosio.token/
cleos set account permission $CONVERTER active --add-code

cleos push action $POOL_TOKEN create '["'$CONVERTER'", "250000000.00000000 '$POOL_TOKEN_SYM'"]' -p $POOL_TOKEN
cleos push action $POOL_TOKEN issue '[ "'$CONVERTER'", "100000.00000000 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER
cleos push action $POOL_TOKEN transfer '["'$CONVERTER'", "bnttestuser1", "100000.00000000 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER

cleos push action $RESERVE create '["'$CONVERTER'", "250000000.00000000 '$RESERVE_SYM'"]' -p $RESERVE

cleos push action $CONVERTER init '["'$POOL_TOKEN'", "0.00000000 '$POOL_TOKEN_SYM'", "1", "1", "thisisbancor", "0", "30000", "0"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["bntbntbntbnt", "8,BNT","500000", "1"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["'$RESERVE'", "8,'$RESERVE_SYM'","500000", "1"]' -p $CONVERTER
cleos push action $RESERVE open '["migration", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 
cleos push action $RESERVE open '["bnttestuser1", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 
cleos push action $RESERVE issue '[ "'$CONVERTER'", "1201.20000000 '$RESERVE_SYM'", "setup"]' -p $CONVERTER
cleos push action bntbntbntbnt transfer '["bnttestuser1", "'$CONVERTER'", "600.00000300 BNT", "setup"]' -p bnttestuser1

cleos push action migration addcnvrtrcur '["'$POOL_TOKEN_SYM'", "'$CONVERTER'"]' -p migration



CONVERTER="bnt2dddcnvrt"
POOL_TOKEN="bnt2dddrelay"
POOL_TOKEN_SYM="BNTDDD"
RESERVE="ddd"
RESERVE_SYM="DDD"
cleos system newaccount eosio $CONVERTER EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $POOL_TOKEN EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $RESERVE EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos set contract $CONVERTER ./build/BancorConverter/
cleos set contract $POOL_TOKEN ./build/eosio.token/
cleos set contract $RESERVE ./build/eosio.token/
cleos set account permission $CONVERTER active --add-code

cleos push action $POOL_TOKEN create '["'$CONVERTER'", "250000000.00000000 '$POOL_TOKEN_SYM'"]' -p $POOL_TOKEN
cleos push action $POOL_TOKEN issue '[ "'$CONVERTER'", "12000.02009001 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER
cleos push action $POOL_TOKEN transfer '["'$CONVERTER'", "bnttestuser1", "12000.02009001 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER

cleos push action $RESERVE create '["'$CONVERTER'", "250000000.00000000 '$RESERVE_SYM'"]' -p $RESERVE


cleos push action $CONVERTER init '["'$POOL_TOKEN'", "0.00000000 '$POOL_TOKEN_SYM'", "1", "1", "thisisbancor", "0", "30000", "0"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["bntbntbntbnt", "8,BNT","500000", "1"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["'$RESERVE'", "8,'$RESERVE_SYM'","500000", "1"]' -p $CONVERTER
cleos push action $RESERVE open '["migration", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 
cleos push action $RESERVE open '["bnttestuser1", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 

cleos push action $RESERVE issue '[ "'$CONVERTER'", "1201.20000000 '$RESERVE_SYM'", "setup"]' -p $CONVERTER
cleos push action bntbntbntbnt transfer '["bnttestuser1", "'$CONVERTER'", "600.00000300 BNT", "setup"]' -p bnttestuser1

cleos push action migration addcnvrtrcur '["'$POOL_TOKEN_SYM'", "'$CONVERTER'"]' -p migration



CONVERTER="bnt2eeecnvrt"
POOL_TOKEN="bnt2eeerelay"
POOL_TOKEN_SYM="BNTEEE"
RESERVE="eee"
RESERVE_SYM="EEE"
cleos system newaccount eosio $CONVERTER EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $POOL_TOKEN EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos system newaccount eosio $RESERVE EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV --stake-cpu "50 EOS" --stake-net "10 EOS" --buy-ram-kbytes 50000 --transfer
cleos set contract $CONVERTER ./build/BancorConverter/
cleos set contract $POOL_TOKEN ./build/eosio.token/
cleos set contract $RESERVE ./build/eosio.token/
cleos set account permission $CONVERTER active --add-code

cleos push action $POOL_TOKEN create '["'$CONVERTER'", "250000000.00000000 '$POOL_TOKEN_SYM'"]' -p $POOL_TOKEN
cleos push action $POOL_TOKEN issue '[ "'$CONVERTER'", "602.03450000 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER
cleos push action $POOL_TOKEN transfer '["'$CONVERTER'", "bnttestuser1", "602.03450000 '$POOL_TOKEN_SYM'", ""]' -p $CONVERTER

cleos push action $RESERVE create '["'$CONVERTER'", "250000000.00000000 '$RESERVE_SYM'"]' -p $RESERVE

cleos push action $CONVERTER init '["'$POOL_TOKEN'", "0.00000000 '$POOL_TOKEN_SYM'", "1", "1", "thisisbancor", "0", "30000", "0"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["bntbntbntbnt", "8,BNT","500000", "1"]' -p $CONVERTER
cleos push action $CONVERTER setreserve '["'$RESERVE'", "8,'$RESERVE_SYM'","500000", "1"]' -p $CONVERTER
cleos push action $RESERVE open '["migration", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 
cleos push action $RESERVE open '["bnttestuser1", "8,'$RESERVE_SYM'", "eosio"]' -p eosio 
cleos push action $RESERVE issue '[ "'$CONVERTER'", "6532001.20000000 '$RESERVE_SYM'", "setup"]' -p $CONVERTER
cleos push action bntbntbntbnt transfer '["bnttestuser1", "'$CONVERTER'", "702.01000030 BNT", "setup"]' -p bnttestuser1

cleos push action migration addcnvrtrcur '["'$POOL_TOKEN_SYM'", "'$CONVERTER'"]' -p migration



on_exit
echo -e "${GREEN}--> Done${NC}"
