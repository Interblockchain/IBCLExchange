# Summary

Building from the functionality of our [Basic Contract](https://github.com/Interblockchain/BasicContract), we build a non-custodian exchange smart contract. Contrary to custodian exchanges, we do not require users to have accounts that are handled by our contract. As such, we also do not need a deposit or withdrawal feature. Of course, this limits our exchange to assets generated with the [Basic Contract](https://github.com/Interblockchain/BasicContract)

To pool the liquidity between all partnered exchanges applications, we have elected to store all orders in one central location, i.e. directly in the scope of the contract. All partners can add orders to the pool. Their orders are identified with there account names and there required fees. When orders are settled, the partners that contributed the orders are paid the specified fees, guaranteeing a fair revenue stream. Fees must be charged in our custom token.  

To simplify and minimize the database, we have chosen to work within a symmetric framework, abstracting the notion of "buy" or "sell" from the orders. Each order will instead correspond to a desired transaction between two amount of assets. Orders will specify the amount and asset that are offered by the user (in the base structure) and the amount and asset the user is willing to accept as trade for the full order (in the counter structure). This way of doing things is more transparent, less error-prone and as the added benefit of only handling asset amounts (conversions and other rounding prone calculations are left off-chain).

#### Example:
```
Alice wants to trade 10.0000 iBTC on our exchange. Firstly, she approves the DEX account (ibclexchange) to spend 10.0000 iBTC
in her name. She then logs in to a partnered app and checks the different iBTC buying orders on the exchange.
She chooses to sell her iBTC for some iXRP at an acceptable (fictional) rate of 100 iXRP per iBTC. To do so, she
issues an Order on the app by filling out a form. Behind the scenes, the app issues an createOrder action
on the DEX contract specifying the 10.0000 iBTC in the base structure. Instead of only specifying the rate, it
calculates the price of the full order (which is 1000.0000 iXRP) and specifies that as the counter. It also fills
the fees and sender properties which allow the partner to receive his fees when the Order settles. 
```

<img src="./exchange_smart-contract.png" width="700">

# User Experience

Users that want to trade on our exchange will have to :

1) Allow the exchange to transfer a specified amount of assets in there name using the Approve action from the [Basic Contract](https://github.com/Interblockchain/BasicContract). Since all fees must be paid in our custom token, they must also hold and approve the exchange for this token.

2) The user then trades on an affiliated exchange app which issues the createOrder action on the exchange contract. The user must sign the transaction with is own key and the RAM cost of storing this order is paid by the user. The affiliated partner sets the sender and fees properties of the order. These should be made visible to the user in the UI (they are visible in the transaction and state on the blockchain). 
   
3) If a compatible counter offer already exists on the exchange, then both offers are matched and the transaction is issued by a matching engine (which issues a settleorders action with is own authority). The exchange contract directly transfers the related funds (using the allowance table and the transferFrom action of the [Basic Contract](https://github.com/Interblockchain/BasicContract)) in one transaction, guaranteeing that both parties pay the agreed-upon price or the transaction fails. Also included in the transaction is the payment of fees: both the maker and taker pay their respective affiliated exchange.

Some additional user interactions are:

a) A user should be able to query the orders present on the exchange. For now, this is done by using the standard [cleos](https://developers.eos.io/eosio-cleos/docs) command `get table`, which is possible because the exchange state is housed in the scope of the account on which the exchange contract is deployed.
This can be done using [eosjs](https://github.com/EOSIO/eosjs) or our [EOSPlus library](https://github.com/Interblockchain/EOSPlus). Alternatively, or concurrently, we can define a Query action in the exchange contract completing the CRUD interface (this is not done).

b) A user can edit is orders, to increase or decrease selling/buying prices. The EditOrder action allows users to change the base amount and the full counter properties of an existing order. This means a user cannot change the base asset of his orders, instead, he must issue a new one in the new asset. Since this is only manipulating already existing orders, this action is free of charge, but must still be signed by the original user to prevent tampering.

c) A user could want to extend the lifetime of is order. I propose an "ExtendOrder" action which allows to put a greater expire time. This is separate from the EditOrder action since, in principle, we could charge an extra fee to do so (this is not implemented yet). 

d) Furthermore, there are two different delete functions for Orders. The first, cancelOrder is used by the original user (using is signature) if he wants to cancel is Order and regain the RAM cost. The second, retireOrder is used to delete Orders which have expired (as check is done on the current date). This is done by a garbage collection service. 

# Security

The users typically interact with the DEX via some partnered app which constructs the Orders off-chain. As such, some extra care must be taken in the DEX to protect the users. 

One possible route to steal/distribute assets would be to impersonate users on the DEX, issuing or editing orders with another user's account name and with unfavorable rates. Since the victim as approved the DEX to spend assets in its name, such orders will be fulfilled. To block this route, the DEX verifies the ownership or permission of its users before any action is done. Specifically, the creation, editing, and deletion of Orders require the user to sign the transactions with there private keys.

As such, the partnered app should allow users to sign said transactions directly on their computers without communicating the actual keys. This can be done with eos-transit and, for convenience, our [EOSPlus library](https://github.com/Interblockchain/EOSPlus).

Furthermore, as can be seen in the contract, the Settle Orders action explicitly checks that matched orders are compatible and that all prices and amounts required by the parties are met. Hence, there is no danger in using this action. 

# Structures

The state of the exchange is stored in the Orders table, which is an ordered table of Order structures. This is queryable with cleos:

```bash
cleos get table ibclexchange ibclexchange orders
```

Each of the Order structures is defined as follows: 
```cplusplus
order_struct
    {
        uint64_t key;
        account_name user;
        account_name sender;
        asset base;
        asset counter;
        asset fees;
        uint64_t timestamp;
        uint64_t expires; 
        uint64_t primary_key() const {return key;}
    };

asset 
    {
        uint64_t amount;
        char[] symbol; 
    }
```

# Actions

## Create Order
``` c++
void createorder(name user, name sender, uint64_t key, asset base, asset counter, asset fees, string memo, uint64_t timestamp, uint64_t expires)
```
The createOrder method allows to store an Order in our exchange state. This method must be called with the authority of the user.
Before adding the order into the table, the contract checks that the user as sufficient balance and allowance for the base and the required permissions. Since a user can always change his allowance table, he can lock the exchange out after issuing an Order. This could be used for malicious order placement (DOS or something). This is mitigated by the fact that the RAM for storing Orders is paid by the user. Hence, a malicious agent would have to use is own resources. 

### Parameters:
* user: EOS account name of the user issuing the Order
* sender: EOS account name of the particular relayer that broadcasted the Order
* key: an integer index to store the Order, must be unique (see the EOSPlus repository for an algorithm to generate this)
* base: The amount and currency (ex: "1.0000 TEOS") that is offered in the Order
* counter: The amount and currency (ex: "0.0100 TBTC") that is asked for the whole offering of the Order
* fees: The amount and currency (ex: "0.10000000 INTER") that is charged by the sender
* memo: A simple string memo
* timestamp: integer timestamp corresponding to the creation date of the Order
* expires: integer corresponding to the expiration date of the Order


## Settle Orders
``` c++
void settleorders(uint64_t maker, uint64_t taker, asset quantity_maker, asset deduct_maker, asset quantity_taker, asset deduct_taker, string memo)
```
This method executes the transactions when two orders are matched by the DEX (they are specified by their key properties).
Both payments are executed in one transaction, such that if one fails, both fail.
Of course, before issuing the transaction all checks are done (balance, allowance, permissions, etc.).
We allow fractional fulfillment of Orders, hence we must make additional checks on the asked prices. 
After all this is done, we modify or delete the transaction.
Anybody can call this action, which is why everything is checked. Nevertheless, this function could be used to flood our account with requests.
To mitigate this, malicious users can be reported to the Block producers and removed. 

### Parameters:
* maker: Index of the first Order (which we call the maker)
* taker: Index of the second Order (which we call the taker)
* quantity_maker: Amount and currency which is transferred from the maker to the taker
* deduct_maker: Amount and currency which is deducted from the counter of the maker to keep his price the same 
* quantity_taker: Amount and currency which is transferred from the taker to the maker
* deduct_maker: Amount and currency which is deducted from the counter of the taker to keep his price the same 
* memo: a simple string memo


## Cancel Order
``` c++
void cancelorder(uint64_t key)
```
This method deletes an order from the DEX contract scope. This must be executed by the original user that created the Order.

### Parameters:
* key: index of the Order

 ## Retire Order
``` c++
void retireorder(uint64_t key)
```
This method deletes an order from the DEX contract scope if its expiration date is passed. This can be executed by anybody and
is used to make a garbage collection service.

### Parameters:
* key: index of the Order

## Edit Order
``` c++
void editOrder(uint64_t key, asset base, asset counter, uint64_t expires)
```
This method is used to change some properties of an Order. This action can only be executed by the original user that created the Order.

### Parameters:
* key: index of the Order
* base: New amount ( includes also the currency which cannot be changed) with which to change the offer of the Order
* counter: New amount ( includes also the currency which cannot be changed) with which to change the asking of the Order
* expires: New expiration date for the Order
