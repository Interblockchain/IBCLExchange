/**
 *  @file
 *  @copyright (c) 2019 Copyright Transledger inc. All rights reserved 
 * 
**/

#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/print.hpp>


#include <string>

namespace eosiosystem
{
class system_contract;
}

namespace eosio
{
using std::string;
class [[eosio::contract("IBCLExchange")]] IBCLExchange : public contract
{
public:
  using contract::contract;
  //IBCLExchange(name self) : contract(self) {}

  [[eosio::action]]
  void createorder(name user,
                   name sender,
                   uint64_t key,
                   asset base,
                   asset counter,
                   asset fees,
                   string memo,
                   uint64_t timestamp,
                   uint64_t expires);

  [[eosio::action]]
  void settleorders(uint64_t maker,
                    uint64_t taker,
                    asset quantity_maker,
                    asset deduct_maker,
                    asset quantity_taker,
                    asset deduct_taker,
                    string memo );

  [[eosio::action]]
  void editorder(uint64_t key,
                 asset base,
                 asset counter,
                 uint64_t expires);

  [[eosio::action]]
  void cancelorder(uint64_t key);

  [[eosio::action]]
  void retireorder(uint64_t key);

private:

  struct [[eosio::table]] order_struct
  {
    uint64_t key;
    name user;
    name sender;
    asset base;
    asset counter;
    asset fees;
    uint64_t timestamp;
    uint64_t expires;
    uint64_t primary_key() const { return key; }
  };

  struct [[eosio::table]] currency_stats
  {
        asset supply;
        asset max_supply;
        name issuer;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
  };

  struct [[eosio::table]] allowed_struct
  {
        uint64_t key;
        name spender;
        asset quantity;

        uint64_t primary_key() const { return key; }
  };

  typedef eosio::multi_index<"orders"_n, order_struct> orders;
  typedef eosio::multi_index<"stat"_n, currency_stats> stats;
  typedef eosio::multi_index<"allowed"_n, allowed_struct> allowed;

  void sendtransfer(name from, name to, asset amount, string memo);
  uint32_t now();
};
} // namespace eosio
