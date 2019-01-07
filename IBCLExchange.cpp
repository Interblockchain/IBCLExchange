/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 * 
 */

#include "IBCLExchange.hpp"

namespace eosio
{
/*
This fonctions adds an order to the orders table.
*/
void IBCLExchange::createorder(name user,
                               asset base,  
                               asset counter, 
                               asset fees,
                               string memo,
                               uint64_t timestamp,
                               uint64_t expires)
{
    eosio::print("Entering contract \n");

    //Only the account on which the contract is deployed can make operations
    require_auth(_self);

    eosio::print("After _self authorization \n");
    //BasicContract address
    auto basic = "basiccontrac"_n;

    //Checks on the base amount: check that it is valid
    auto symbase = base.symbol;
    eosio_assert(symbase.is_valid(), "invalid symbol name");
    eosio_assert(base.is_valid(), "invalid quantity of base token");
    eosio_assert(base.amount > 0, "Base amount must be positive");
    stats basestatstable(basic, symbase.code().raw()); //basic or _self?
    const auto &bst = basestatstable.get(symbase.code().raw());
    eosio_assert(symbase == bst.supply.symbol, "symbol precision mismatch");

    // Checks on the allowed table of the user for the base amount
    allowed allowedtable(basic, user.value);                  //basic or _self?
    auto existing = allowedtable.find(_self.value + symbase.code().raw()); //Find returns an iterator pointing to the found object
    eosio_assert(existing != allowedtable.end(), "IBCLExchange not allowed");
    const auto &at = *existing;
    eosio_assert(at.quantity.is_valid(), "invalid allowed quantity");
    eosio_assert(at.quantity.amount > 0, "allowed must be a positive quantity");
    eosio_assert(at.quantity.symbol == bst.supply.symbol, "symbol precision mismatch");
    eosio_assert(at.quantity.amount >= base.amount + fees.amount, "Allowed quantity < Order Quantity");

    //Checks on the counter amount: check if it is valid
    auto symcounter = counter.symbol;
    eosio_assert(symcounter.is_valid(), "invalid symbol name");
    eosio_assert(counter.is_valid(), "invalid quantity of base token");
    eosio_assert(counter.amount > 0, "Base amount must be positive");
    stats counterstatstable(basic, symcounter.code().raw()); //basic or _self?
    const auto &cst = counterstatstable.get(symcounter.code().raw());
    eosio_assert(symcounter == cst.supply.symbol, "symbol precision mismatch");

    //Should we check the fees?

    //Check memo size
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    //Check if the order already exists
    const uint64_t key = user.value + symbase.code().raw();
    eosio::print("KEY: " , uint64_t{key}, "\n");

    orders orderstable(_self, key);
    auto existo = orderstable.find(key);
    eosio_assert(existo == orderstable.end(), "Order with same key already exists");

    eosio::print("Before sendtransfer \n");

    //Since everything is ok, we can proceed with the paiement of the fees.
    //We can only use the allowed token that we checked.
    //Do this by calling the transferFrom action from Basic Contract
    // Should to address be other then _self?
    sendtransfer(user, _self, fees, memo);

    eosio::print("After paiement of fees \n");

    //Now, emplace the order.
    orderstable.emplace(_self, [&](auto &o) {
        o.key = user.value + symbase.code().raw();
        o.user = user;
        o.base = base;
        o.counter = counter;
        o.timestamp = timestamp;
        o.expires = expires;
    });
}

/*
This function is called after two orders are paired. It proceeds to transfer the required funds.
Then it deletes orders (if they were both completely fulfilled) or just modifies their amounts.
It's arguments are the two keys of the orders.
*/
void IBCLExchange::settleorders(uint64_t maker,
                                uint64_t taker,
                                asset quantity,
                                string memo )
{

    orders morderstable(_self, maker);
    auto makerorder = morderstable.find(maker);
    const auto &mot = *makerorder;
    orders torderstable(_self, taker);
    auto takerorder = torderstable.find(taker);
    const auto &tot = *takerorder;

    /*Allow fractional fulfillment
     To do this: 1) calculate both rates,
                 2) verify that rate of taker is higher then rate of maker,
                 3) user taker rate and given amount to calculate the payoff to maker,
    */
    auto mrate = mot.counter.amount / mot.base.amount;
    auto trate = tot.base.amount / tot.counter.amount;
    eosio_assert( trate >= mrate, "Taker rate smaller than maker rate");
    auto tamount = trate * quantity.amount;

    //Some sanity checks
    eosio_assert( mot.base.amount >= quantity.amount, "Amount bigger than specified in maker order");
    eosio_assert( tot.base.amount >= tamount, "Amount bigger than specified in taker order");

    //Now make the transfers between both parties
    //Don't need to redo all the checks since they are done in transferfrom
    // MUST TRANSFORM AMOUNT AND TAMOUNT TO ASSET.
    sendtransfer(mot.user, tot.user, quantity, memo);
    //sendtransfer(tot.user, mot.user, _self, {tamount, tot.base.symbol}, memo);
    sendtransfer(tot.user, mot.user, tot.base, memo);

    //Now modify/delete the orders accordingly
    // Maker:
    //updateorder(amount, morderstable, mot, mrate);
    if(quantity.amount == mot.base.amount) 
    {
        morderstable.erase(mot);
    } else {
        morderstable.modify(mot, _self, [&](auto &o) {
        o.base.amount -= quantity.amount;
        o.counter.amount -= quantity.amount * mrate;
    });
    }
    //Taker:
    //updateorder(tamount, torderstable, tot, trate);
    if(tamount == tot.base.amount) 
    {
        torderstable.erase(tot);
    } else {
        torderstable.modify(tot, _self, [&](auto &o) {
        o.base.amount -= tamount;
        o.counter.amount -= tamount / trate;
    });
    }
}

/*
Allows to update an order. 
For now, you can modify only the base.amount, counter and expire time.
*/
void IBCLExchange::editorder(uint64_t key,
                             name user,
                             asset base,
                             asset counter,
                             uint64_t timestamp,
                             uint64_t expires)
{
    //Check that the order exists and get it
    orders orderstable(_self, key);
    auto element = orderstable.find(key);
    eosio_assert(element != orderstable.end(), "Order not found!");
    const auto &ot = *element;

    //Checks on the base amount: check that it is valid
    auto symbase = base.symbol;
    eosio_assert(symbase.is_valid(), "invalid symbol name");
    eosio_assert(base.is_valid(), "invalid quantity of base token");
    eosio_assert(base.amount > 0, "Base amount must be positive");
    stats basestatstable(_self, symbase.code().raw()); //Should put the BasicContract account and not _self?
    const auto &bst = basestatstable.get(symbase.code().raw());
    eosio_assert(symbase == bst.supply.symbol, "symbol precision mismatch");
    eosio_assert(symbase == ot.base.symbol, "Cannot change the base asset type");

    // Checks on the allowed table of the user for the base amount
    allowed allowedtable(_self, user.value);                  //Should put the BasicContract account and not _self?
    auto existing = allowedtable.find(_self.value + symbase.code().raw()); //Find returns an iterator pointing to the found object
    eosio_assert(existing != allowedtable.end(), "IBCLExchange not allowed");
    const auto &at = *existing;
    eosio_assert(at.quantity.is_valid(), "invalid allowed quantity");
    eosio_assert(at.quantity.amount > 0, "allowed must be a positive quantity");
    eosio_assert(at.quantity.symbol == bst.supply.symbol, "symbol precision mismatch");
    eosio_assert(at.quantity.amount >= base.amount, "Allowed quantity < Order Quantity");

    //Checks on the counter amount: check if it is valid
    auto symcounter = counter.symbol;
    eosio_assert(symcounter.is_valid(), "invalid symbol name");
    eosio_assert(counter.is_valid(), "invalid quantity of base token");
    eosio_assert(counter.amount > 0, "Base amount must be positive");
    stats counterstatstable(_self, symcounter.code().raw()); //Should put the BasicContract account and not _self?
    const auto &cst = counterstatstable.get(symcounter.code().raw());
    eosio_assert(symcounter == cst.supply.symbol, "symbol precision mismatch");

    orderstable.modify(ot, _self, [&](auto &o) {
        o.base.amount = base.amount;
        o.counter = counter;
        o.expires = expires;
    });
}

/*
This functions deletes an order from the orders table.
*/
void IBCLExchange::cancelorder(uint64_t key)
{
    orders orderstable(_self, key);
    auto ot = orderstable.find(key);
    eosio_assert(ot != orderstable.end(), "Order not found!");
    orderstable.erase(ot);
}

/*
Helper function to send the transferfrom action to the basic contract.
NOTE: the account in which the basic contract is deployed is explicitely stated here (CHANGE THIS AS NEEDED).
*/
void IBCLExchange::sendtransfer(name from, name to, asset amount, string memo)
{
    eosio::print("Inside sendtransfer: ", name{get_self()}, "\n");
    action transferfrom = action(
      permission_level{get_self(),"active"_n},
      "basiccontrac"_n,
      "transferfrom"_n,
      std::make_tuple(from, to, get_self(), amount, memo));

    transferfrom.send();
}

} // namespace eosio

EOSIO_DISPATCH(eosio::IBCLExchange, (createorder)(settleorders)(editorder)(cancelorder))
