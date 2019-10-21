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
                               name sender,
                               uint64_t key,
                               asset base,  
                               asset counter, 
                               asset fees,
                               string memo,
                               uint64_t timestamp,
                               uint64_t expires)
{
    eosio::print("Entering contract \n");

    //Only the account on which the contract is deployed can make operations
    require_auth(user);

    //BasicContract address
    auto basic = "ibclcontract"_n;

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
    auto symfees = fees.symbol;
    eosio_assert(symfees.is_valid(), "invalid symbol name");
    eosio_assert(fees.is_valid(), "invalid quantity of base token");
    eosio_assert(fees.amount >= 0, "Base amount must be positive or zero");
    eosio_assert(symfees.code().raw() == "INTER", "Fees must be in INTER"); //Fees must be paid in INTER
    stats feestatstable(basic, symfees.code().raw()); //basic or _self?
    const auto &fst = feestatstable.get(symfees.code().raw());
    eosio_assert(symfees == fst.supply.symbol, "symbol precision mismatch");

    //Check memo size
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    //Check if the order already exists
    orders orderstable(_self, key);
    auto existo = orderstable.find(key);
    eosio_assert(existo == orderstable.end(), "Order with same key already exists");

    //Since everything is ok, we can proceed with the paiement of the fees.
    //We can only use the allowed token that we checked.
    //Do this by calling the transferFrom action from Basic Contract
    // Should to address be other then _self?
    sendtransfer(user, _self, fees, memo);

    //Now, emplace the order.
    orderstable.emplace(_self, [&](auto &o) {
        o.key = key;
        o.user = user;
        o.sender = sender;
        o.base = base;
        o.counter = counter;
        o.timestamp = timestamp;
        o.expires = expires;
    });
}

/*
This function is called after two orders are paired. It proceeds to transfer the specified funds.
We explicitely specify th fubds to allow fractional fulfillement.
Then it deletes orders (if they were both completely fulfilled) or just modifies their amounts.
It's arguments are the two keys of the orders.
*/
void IBCLExchange::settleorders(uint64_t maker,
                                uint64_t taker,
                                asset quantity_maker,
                                asset deduct_maker,
                                asset quantity_taker,
                                asset deduct_taker,
                                string memo )
{

    // Get both maker and taker orders
    orders morderstable(_self, maker);
    auto makerorder = morderstable.find(maker);
    eosio_assert(makerorder != morderstable.end(), "Maker order not found");
    const auto &mot = *makerorder;

    orders torderstable(_self, taker);
    auto takerorder = torderstable.find(taker);
    eosio_assert(takerorder != torderstable.end(), "Taker order not found");
    const auto &tot = *takerorder;

    //Some sanity checks
    eosio::print("maker order quantity: " , asset{mot.base}, "\n");
    eosio::print("maker quantity: " , asset{quantity_maker}, "\n");
    eosio::print("taker order quantity: " , asset{tot.base}, "\n");
    eosio::print("taker quantity: " , asset{quantity_taker}, "\n");
    eosio_assert( mot.base.amount >= quantity_maker.amount, "Amount bigger than specified in maker order");
    eosio_assert( tot.base.amount >= quantity_taker.amount, "Amount bigger than specified in taker order");

    //Now make the transfers between both parties
    //Don't need to redo all the checks since they are done in transferfrom
    sendtransfer(mot.user, tot.user, quantity_maker, memo);
    sendtransfer(tot.user, mot.user, quantity_taker, memo);

    // Pay the fees to the respective senders
    sendtransfer(mot.user, mot.sender, mot.fee, memo)
    sendtransfer(tot.user, tot.sender, tot.fee, memo)

    //This is done by calling editorder or cancelorder when a settlement is done.
    //So, don't do it here.
    //Now modify/delete the orders accordingly
    // Maker:
    // //updateorder(amount, morderstable, mot, mrate);
    // if(quantity_maker.amount == mot.base.amount) 
    // {
    //     morderstable.erase(mot);
    // } else {
    //     morderstable.modify(mot, _self, [&](auto &o) {
    //     o.base.amount -= quantity_maker.amount;
    //     o.counter.amount -= deduct_maker.amount;
    // });
    // }
    // //Taker:
    // //updateorder(tamount, torderstable, tot, trate);
    // if(quantity_taker.amount == tot.base.amount) 
    // {
    //     torderstable.erase(tot);
    // } else {
    //     torderstable.modify(tot, _self, [&](auto &o) {
    //     o.base.amount -= quantity_taker.amount;
    //     o.counter.amount -= deduct_taker.amount;
    // });
    // }
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
    //Only the user can modify his orders
    require_auth(user);

    //Check that the order exists and get it
    orders orderstable(_self, key);
    auto element = orderstable.find(key);
    eosio_assert(element != orderstable.end(), "Order not found!");
    const auto &ot = *element;

    auto basic = "ibclcontract"_n;

    //Checks on the base amount: check that it is valid
    auto symbase = base.symbol;
    eosio_assert(symbase.is_valid(), "invalid symbol name");
    eosio_assert(base.is_valid(), "invalid quantity of base token");
    eosio_assert(base.amount > 0, "Base amount must be positive");
    stats basestatstable(basic, symbase.code().raw()); 
    const auto &bst = basestatstable.get(symbase.code().raw());
    eosio_assert(symbase == bst.supply.symbol, "symbol precision mismatch");
    eosio_assert(symbase == ot.base.symbol, "Cannot change the base asset type");

    // Checks on the allowed table of the user for the base amount
    allowed allowedtable(basic, user.value);
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
    stats counterstatstable(basic, symcounter.code().raw());
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
      "ibclcontract"_n,
      "transferfrom"_n,
      std::make_tuple(from, to, get_self(), amount, memo));

    transferfrom.send();
}

} // namespace eosio

EOSIO_DISPATCH(eosio::IBCLExchange, (createorder)(settleorders)(editorder)(cancelorder))
