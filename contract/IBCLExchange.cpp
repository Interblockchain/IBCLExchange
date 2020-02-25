/**
 *  @file
 *  @copyright (c) 2019 Copyright Transledger inc. All rights reserved 
 * 
**/

#include "IBCLExchange.hpp"

namespace eosio
{

/**
 * Create Order 
 * @details This fonctions adds an order to the orders table.
 * @param user - the account that creates the order
 * @param sender - the account of the relayer that will receive the fees
 * @param key - a uint64 that is used to index the order in the table
 * @param base - amount of tokens that are offered
 * @param counter - amount of tokens asked for the offer
 * @param fees - amount of fees paid for the order (must be in INTER)
 * @param memo - a simple memo
 * @param timestamp - time the order is created
 * @param expires - date at which the order expires  
**/
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

    //Only the account on which the contract is deployed can make operations
    require_auth(user);

    //BasicContract address and our token
    auto basic = "ibclcontract"_n;
    uint64_t token = 1145853012;

    //Checks on the base amount: check that it is valid
    auto symbase = base.symbol;
    check(symbase.is_valid(), "invalid symbol name");
    check(base.is_valid(), "invalid quantity of base token");
    check(base.amount > 0, "Base amount must be positive");
    stats basestatstable(basic, symbase.code().raw());
    const auto &bst = basestatstable.get(symbase.code().raw());
    check(symbase == bst.supply.symbol, "symbol precision mismatch");

    // Checks on the allowed table of the user for the base amount
    allowed allowedtable(basic, user.value);
    auto existing = allowedtable.find(get_self().value + symbase.code().raw()); //Find returns an iterator pointing to the found object
    check(existing != allowedtable.end(), "IBCLExchange not allowed for base");
    const auto &at = *existing;
    check(at.quantity.is_valid(), "invalid allowed quantity");
    check(at.quantity.amount > 0, "allowed must be a positive quantity");
    check(at.quantity.symbol == bst.supply.symbol, "symbol precision mismatch");
    check(at.quantity.amount >= base.amount, "Allowed quantity < Order Quantity");

    //Checks on the counter amount: check if it is valid
    auto symcounter = counter.symbol;
    check(symcounter.is_valid(), "invalid symbol name");
    check(counter.is_valid(), "invalid quantity of base token");
    check(counter.amount > 0, "Base amount must be positive");
    stats counterstatstable(basic, symcounter.code().raw());
    const auto &cst = counterstatstable.get(symcounter.code().raw());
    check(symcounter == cst.supply.symbol, "symbol precision mismatch");

    //Should we check the fees?
    auto symfees = fees.symbol;
    check(symfees.is_valid(), "invalid symbol name");
    check(fees.is_valid(), "invalid quantity of base token");
    check(fees.amount >= 0, "Base amount must be positive or zero");
    check(symfees.code().raw() == token, "Fees must be in TTLD"); //Fees must be paid in TTLD       <===============
    stats feestatstable(basic, symfees.code().raw());
    const auto &fst = feestatstable.get(symfees.code().raw());
    check(symfees == fst.supply.symbol, "symbol precision mismatch");

    auto feesexist = allowedtable.find(get_self().value + symfees.code().raw()); //Find returns an iterator pointing to the found object
    check(feesexist != allowedtable.end(), "IBCLExchange not allowed for fees");
    const auto &ft = *feesexist;
    check(ft.quantity.is_valid(), "invalid fees allowed quantity");
    check(ft.quantity.amount > 0, "fees allowed quantity must be a positive quantity");
    check(ft.quantity.symbol == fst.supply.symbol, "fees symbol precision mismatch");
    check(ft.quantity.amount >= fees.amount, "fees allowed quantity < Fees order Quantity");

    //Check memo size
    check(memo.size() <= 256, "memo has more than 256 bytes");

    //Check if the order already exists
    orders orderstable(get_self(), get_self().value);
    auto existo = orderstable.find(key);
    check(existo == orderstable.end(), "Order with same key already exists");

    //Since everything is ok, we can proceed with the paiement of the fees.
    //We can only use the allowed token that we checked.
    //Do this by calling the transferFrom action from Basic Contract
    //Should to address be other then get_self()?
    //sendtransfer(user, get_self(), fees, memo);

    //Now, emplace the order.
    orderstable.emplace(user, [&](auto &o) { //who pays the RAM, get_self() or user?
        o.key = key;
        o.user = user;
        o.sender = sender;
        o.base = base;
        o.counter = counter;
        o.fees = fees;
        o.timestamp = timestamp;
        o.expires = expires;
    });
}

/**
 * Settle Orders
 * @details This function is called after two orders are paired. It proceeds to transfer the specified funds.
 * @details We explicitely specify th fubds to allow fractional fulfillement.
 * @details Then it deletes orders (if they were both completely fulfilled) or just modifies their amounts.
 * @param maker - index (key) of the maker order
 * @param taker - index (key) of the taker order
 * @param quantity_maker - amount of tokens sent by the maker to the taker
 * @param deduct_maker - amount of tokens to deduct from the counter in the sell order
 * @param quantity_taker - amount of tokens sent by the taker to the maker
 * @param deduct_taker - amount of tokens to deduct from the counter in the buy order
 * @param memo - a simple memo
**/
void IBCLExchange::settleorders(uint64_t maker,
                                uint64_t taker,
                                asset quantity_maker,
                                asset deduct_maker,
                                asset quantity_taker,
                                asset deduct_taker,
                                string memo)
{
    const double tol = 1.0e-10;

    // Get both maker and taker orders
    orders morderstable(get_self(), get_self().value);
    auto makerorder = morderstable.find(maker);
    check(makerorder != morderstable.end(), "Maker order not found");
    const auto &mot = *makerorder;

    orders torderstable(get_self(), get_self().value);
    auto takerorder = torderstable.find(taker);
    check(takerorder != torderstable.end(), "Taker order not found");
    const auto &tot = *takerorder;

    //Calculating the different required prices for validation
    const double price = quantity_maker.amount / quantity_taker.amount;
    const double ask_price = mot.base.amount / mot.counter.amount;
    const double taker_price = tot.base.amount / tot.counter.amount;
    const double new_ask_price = (mot.base.amount - quantity_maker.amount) / (mot.counter.amount - deduct_maker.amount);
    const double new_taker_price = (tot.base.amount - quantity_taker.amount) / (tot.counter.amount - deduct_taker.amount);

    //Some sanity checks (some tests are performed in sendtransfer so don't do them twice)
    check(mot.base.amount >= quantity_maker.amount, "Amount bigger than specified in maker order");
    check(mot.base.symbol.code().raw() == quantity_maker.symbol.code().raw(), "Maker base currency mismatch");
    check(mot.counter.symbol.code().raw() == deduct_maker.symbol.code().raw(), "Maker counter currency mismatch");
    check(tot.base.amount >= quantity_taker.amount, "Amount bigger than specified in taker order");
    check(tot.base.symbol.code().raw() == quantity_taker.symbol.code().raw(), "Taker base currency mismatch");
    check(tot.counter.symbol.code().raw() == deduct_taker.symbol.code().raw(), "Taker counter currency mismatch");
    check(price >= ask_price, "Buying price is smaller than asked price");
    check((new_ask_price - ask_price) <= tol, "Updated maker ask price is not the same (deduct_maker not valid)");
    check((new_taker_price - taker_price) <= tol, "Updated taker ask price is not the same (deduct_taker not valid)");

    //Now make the transfers between both parties
    //Don't need to redo all the checks since they are done in transferfrom
    sendtransfer(mot.user, tot.user, quantity_maker, memo);
    sendtransfer(tot.user, mot.user, quantity_taker, memo);

    // Pay the fees to the respective senders
    sendtransfer(mot.user, mot.sender, mot.fees, memo);
    sendtransfer(tot.user, tot.sender, tot.fees, memo);

    //Now modify/delete the orders accordingly
    // Maker:
    // //updateorder(amount, morderstable, mot, mrate);
    if (quantity_maker.amount == mot.base.amount)
    {
        morderstable.erase(mot);
    }
    else
    {
        morderstable.modify(mot, get_self(), [&](auto &o) {
            o.base.amount -= quantity_maker.amount;
            o.counter.amount -= deduct_maker.amount;
        });
    }
    //Taker:
    //updateorder(tamount, torderstable, tot, trate);
    if (quantity_taker.amount == tot.base.amount)
    {
        torderstable.erase(tot);
    }
    else
    {
        torderstable.modify(tot, get_self(), [&](auto &o) {
            o.base.amount -= quantity_taker.amount;
            o.counter.amount -= deduct_taker.amount;
        });
    }
}

/**
 * Edit order
 * @details Allows to update an order. 
 * @details For now, you can modify only the base.amount, counter and expire time.
 * @param key - a uint64 that is used to index the order in the table
 * @param user - the account that creates the order
 * @param base - amount of tokens that are offered
 * @param counter - amount of tokens asked for the offer
 * @param expires - date at which the order expires
**/
void IBCLExchange::editorder(uint64_t key,
                             asset base,
                             asset counter,
                             uint64_t expires)
{

    //Check that the order exists and get it
    orders orderstable(get_self(), get_self().value);
    auto element = orderstable.find(key);
    check(element != orderstable.end(), "Order not found!");
    const auto &ot = *element;

    auto basic = "ibclcontract"_n;

    //Only the user can modify his orders
    require_auth(ot.user);

    //Checks on the base amount: check that it is valid
    auto symbase = base.symbol;
    check(symbase.is_valid(), "invalid symbol name");
    check(base.is_valid(), "invalid quantity of base token");
    check(base.amount > 0, "Base amount must be positive");
    stats basestatstable(basic, symbase.code().raw());
    const auto &bst = basestatstable.get(symbase.code().raw());
    check(symbase == bst.supply.symbol, "symbol precision mismatch");
    check(symbase == ot.base.symbol, "Cannot change the base asset type");

    // Checks on the allowed table of the user for the base amount
    allowed allowedtable(basic, ot.user.value);
    auto existing = allowedtable.find(get_self().value + symbase.code().raw()); //Find returns an iterator pointing to the found object
    check(existing != allowedtable.end(), "IBCLExchange not allowed");
    const auto &at = *existing;
    check(at.quantity.is_valid(), "invalid allowed quantity");
    check(at.quantity.amount > 0, "allowed must be a positive quantity");
    check(at.quantity.symbol == bst.supply.symbol, "symbol precision mismatch");
    check(at.quantity.amount >= base.amount, "Allowed quantity < Order Quantity");

    //Checks on the counter amount: check if it is valid
    auto symcounter = counter.symbol;
    check(symcounter.is_valid(), "invalid symbol name");
    check(counter.is_valid(), "invalid quantity of base token");
    check(counter.amount > 0, "Base amount must be positive");
    stats counterstatstable(basic, symcounter.code().raw());
    const auto &cst = counterstatstable.get(symcounter.code().raw());
    check(symcounter == cst.supply.symbol, "symbol precision mismatch");

    orderstable.modify(ot, get_self(), [&](auto &o) {
        o.base.amount = base.amount;
        o.counter = counter;
        o.expires = expires;
    });
}

/**
 * Cancel order
 * @details This functions deletes an order from the orders table.
 * @param key - a uint64 that is used to index the order in the table
**/
void IBCLExchange::cancelorder(uint64_t key)
{
    orders orderstable(get_self(), get_self().value);
    auto order = orderstable.find(key);
    check(order != orderstable.end(), "Order not found!");
    const auto &ot = *order;
    require_auth(ot.user);
    orderstable.erase(ot);
}

/**
 * Retire order
 * @details This function is called to retire orders which are expired
 * @param key - a uint64 that is used to index the order in the table
**/
void IBCLExchange::retireorder(uint64_t key)
{
    orders orderstable(get_self(), get_self().value);
    auto order = orderstable.find(key);
    check(order != orderstable.end(), "Order not found!");
    const auto &ot = *order;
    check(ot.expires > now(), "Order has not expired");
    orderstable.erase(ot);
}

/**
 * Send transfer
 * @details Helper function to send the transferfrom action to the basic contract.
 * NOTE: the account in which the basic contract is deployed is explicitely stated here (CHANGE THIS AS NEEDED).
 * @param from - account sending the tokens
 * @param to - account that will receive the tokens
 * @param amount - amount of tokens transferred
 * @param memo - a simple memo
**/
void IBCLExchange::sendtransfer(name from, name to, asset amount, string memo)
{
    action transferfrom = action(
        permission_level{get_self(), "active"_n},
        "ibclcontract"_n,
        "transferfrom"_n,
        std::make_tuple(from, to, get_self(), amount, memo));

    transferfrom.send();
}

/**
 * Now
 * @details Helper function to get the current UTC time 
**/
uint32_t IBCLExchange::now()
{
    return current_time_point().sec_since_epoch();
}

} // namespace eosio

EOSIO_DISPATCH(eosio::IBCLExchange, (createorder)(settleorders)(editorder)(cancelorder))
