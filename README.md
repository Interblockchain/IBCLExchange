#Summary

Building from the functionality of our [Basic Contract](https://github.com/Interblockchain/BasicContract), we build a non-custodian exchange smart contract. Our exchange would only accept transactions between assets generated with the [Basic Contract](https://github.com/Interblockchain/BasicContract). User would only need grant the exchange smart contract to power to spend a specified amount of funds (using the approve action) and issue an order on the exchange portal which is an external [firebase](https://firebase.google.com/) server.

In order to simplify and minimize the database, we have chosen to work within a symmetric framework, abstracting the notion of buy or sell from the orders. Each order will instead correspond to a desired transaction between to amount of assets. Orders will specify the amount and asset that are offered by the user (in the base structure). They will also specify the asset the user is willing to accept as trade and the total amount accepted for the full order (in the counter structure). This way of doing things is more transparent, less error prone and as the added benefit of only handling asset amounts (conversions and other rounding prone calculations are left off-chain). 

####Example:
```
Alice wants to trade 10.0000 iBTC on our exchange. Firstly, see approves the DEX account to spend 10.0000 iBTC in her name. She then logs in to our [firebase app]() and checks the different iBTC buying orders on the exchange. She chooses to sell her iBTC for some iXRP at an acceptable (fictional) rate of 100 iXRP per iBTC. To do so, she issues an Order on the firebase app by filling out a form. Behind the scenes, the app issues an createOrder action on the DEX contract specifying the 10.0000 iBTC in the base structure. Instead of only specifying the rate, it calculates the price of the full order (which is 1000.0000 iXRP) and specifies that as the counter. 
```

# User experience

Contrary to custodian exchanges, we do not require users to have accounts which are handled by our contract. As such, we also have no need of a deposit or withdrawal feature. Users that want to trade on our exchange will have to :

1) Allow the exchange to transfer a specified amount of assets in there name using the Approve action from the Basic Contract.

2) The user then trades on the exchange app which issues the createOrder action on the exchange contract. Each time this action is called, a fee is collected (in base asset) in order to pay for the RAM allocation and use of the exchange. If a compatible counter offer already exists on the exchange, then both offers are matched and the transaction issued by the matching engine. The exchange contract directly transfers the related funds (using the allowance table and the transferFrom action of the BasicContract) in one transaction, garanteeing that that both parties or the transaction fails.   

Some additional user interactions are:

a) A user should be able to query the orders present on the exchange. For now, this is done by using the standard [cleos](https://developers.eos.io/eosio-cleos/docs) command `get table`, which is possible because the exchange state is housed in the scope of the account on which the exchange contract is deployed. Alternatively, or concurrently, we can define a Query action in the exchange contract completing the CRUD interface (this is not done).

b) A user is able to edit is orders, to increase or decrease selling/buying prices. The EditOrder action allows users to change the base amount and the full counter properties of an existing order. This means a user cannot change the base asset of his orders, instead he must issue a new one in the new asset. Since this is only manipulating already existing orders, this action is free of charge.

c) A user could want to extend the lifetime of is order. I propose an "ExtendOrder" action which allows to put a greater expire time. This is seperate from the EditOrder action since, in principle, we could charge an extra fee to do so (this is not implemented yet). 

d) Furthermore, I propose the DeleteOrder action which is self explanatory. 

# Security

Since only the DEX account can issue actions on this contract, the users interact with the DEX off-chain, some extra care must be taken in the DEX to protect the users.

One possible route to steal/distribute assets would be to impersonate users on the DEX, issuing orders with another user's account name and with defavorable rates. Since the victim as approved the DEX to spend assets in it's name, such orders will be fulfiled. In order to block this route, the off-chain DEX must find a way to verify the ownership or permission of it's users before any action is done.

# Structures

The state of the exchange is stored in the Orders table, which is an ordered table of Order structures.
Each of the Order structures is defined as follows: 
```cplusplus
order_struct
    {
        uint64_t key;
        account_name user;
        asset base;
        asset counter;
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

#### createorder(account_name user, asset base, asset counter, asset fees, string memo, uint64_t timestamp, uint64_t expires)
The createOrder method allows to store an Order in our exchange state.
Before adding the order into the table, we need to:
1) check that the user as sufficient balance and allowance for the base and the required permissions. 

2) Since a user can always change his allowance table, he can lock the exchange out after issuing an Order. This could be used for malicious order placement (DOS or something). To mitigate this, we apply fees directly on Order submission, even before adding it to the table. If the paiement of the fees fails, nothing is done.

#### settleOrders(uint64_t sell, uint64_t buy)
This method executes the transactions when two orders are matched by the DEX (they are specified by their key properties).
Both paiements are executed in one transaction, such that if one fails, both fail.
Of course, before issuing the transaction all checks are done (balance, allowance, permissions, etc.).
After all is done, we modify or delete the transaction.

#### cancelOrder(uint64_t key)
This method deletes an order from the DEX contract scope. 

#### editOrder(uint64_t key, account_name user, asset base, asset counter, uint64_t timestamp, uint64_t expires)
This method is used to change some properties of an order.



