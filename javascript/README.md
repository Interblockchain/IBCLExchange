# Summary
A small library that interfaces the actions of the Transledger C++ exchange contract deployed on various EOS chains.
In order to do so, we must first log in to a wallet using the [eos-transit package](https://github.com/eosnewyork/eos-transit/tree/master/packages/eos-transit#basic-usage-example) and gain the required authorities for the accounts performing the actions. 
They are all bunched here for easy access, easy modification and to give a uniform convention for the different recurrent quantities.

# Usage

Simply import the module and construct a new instance by passing the connection parameters to the constructor.

```javascript
const exchangeAPI = require("./IBCLExchange.js");

let params = {
    exchangeAddress: "ibclexchange",     //Account where the Transledger contract is deployed (usually: ibclexchange)
    network: {
        host: "jungleapi.eossweden.se",   // RPC API Endpoint for the EOS chain
        port: null,                       // Port of the API Endpoint
        protocol: 'https',                // HTTP Connection protocol to the API Endpoint
        chainId: "e70aaab8997e1dfce58fbfac80cbbb8fecec7b99cf982a9444273cbc64c41473"  // ChainId of the EOS chain (this example is for JUNGLE)
    }
};
const api = new exchangeAPI(params);
```

Then you can invoke the methods, like for example:

```javascript
let response = api.createOrder(wallet, user, sender, baseAmount, baseDecimals, baseSymbol, counterAmount, counterDecimals, counterSymbol, feesAmount, memo, expires);
```

All methods require a unlocked [eos-transit](https://github.com/eosnewyork/eos-transit/tree/master/packages/eos-transit#basic-usage-example) Wallet with the required authorizations as first argument. See there documentation for more information on this.

# Methods

## Create Order
```javascript
let response = createOrder(wallet, user, sender, baseAmount, baseDecimals, baseSymbol, counterAmount, counterDecimals, counterSymbol, feesAmount, memo, expires)
```
This method is used for creating a new order on the blockchain. It must be called with the authority of the specified user. 

### Parameters:
* wallet: transit-eos wallet object which provide keys for the account performing the action (in this case: user)
* user: account of the user creating the order (authorization for this account must be provided in `wallet`)
* sender: account of the relayer (exchange) which will get the commission (fees) for this order 
* baseAmount: amount of tokens offered by 'user'
* baseDecimals: number of decimals used for the currency offered by 'user'
* baseSymbol: symbol of the currency offered by 'user'
* counterAmount: amount of tokens wanted for his offer
* counterDecimals: number of decimals used for the wanted currency
* counterSymbol: symbol of the wanted currency
* feesAmount: amount of fees (in GIZMO) the relayer is charging
* memo: a memo
* expires: Expiration date of the offer [should be in milliseconds since 1970, like Date.now()]

## Edit Order
```javascript
let response = editOrder(wallet, user, key, baseAmount, baseDecimals, baseSymbol, counterAmount, counterDecimals, counterSymbol, expires)
```
This method is used to change an order. The only variables that can be changed are: the expiration date and the amounts offered and required by the order. It must be called with the authority of the user which created the order.

### Parameters:
* wallet: transit-eos wallet object which provide keys for the account performing the action (in this case: user)
* user: account of the user which created the order (authorization for this account must be provided in `wallet`)
* key: key identifying the order
* baseAmount: amount of tokens offered by 'user'
* baseDecimals: number of decimals used for the currency offered by 'user'
* baseSymbol: symbol of the currency offered by 'user'
* counterAmount: amount of tokens wanted for his offer
* counterDecimals: number of decimals used for the wanted currency
* counterSymbol: symbol of the wanted currency
* expires: Expiration date of the offer [should be in milliseconds since 1970, like Date.now()]

## Cancel Order
```javascript
let response = cancelOrder(wallet, user, key)
```
This method is used to delete an existing order. Must be called by the owner of the order.

### Parameters:
* wallet: transit-eos wallet object which provide keys for the account performing the action (in this case: user)
* user: account of the user possessing the order (authorization for this account must be provided in `wallet`)
* key: key identifying the order

## Retire Order
```javascript
let response = retireOrder(wallet, sender, key)
```
This method is used to delete an expired order. It can be called by anybody, a time check is done to prevent deletion of on expired orders.
This is used to do garbage collection.

### Parameters:
* wallet: transit-eos wallet object which provide keys for the account performing the action (in this case: sender)
* sender: account of the user possessing the order (authorization for this account must be provided in `wallet`)
* key: key identifying the order

## Settle Orders
```javascript
let response = settleOrders(wallet, sender, makerKey, takerKey, makerBaseAmount, makerBaseDecimals, makerBaseSymbol, makerCounterAmount, makerCounterDecimals, makerCounterSymbol, takerBaseAmount, takerBaseDecimals, takerBaseSymbol, takerCounterAmount, takerCounterDecimals, takerCounterSymbol , memo)
```
This method is used to settle two matching orders. It can be called by anybody, checks are made to guarantee the matching.

### Parameters:
* wallet: transit-eos wallet object which provide keys for the account performing the action (in this case: sender)
* sender: account of the user issuing the settlement (authorization for this account must be provided in `wallet`)
* makerKey: Key index of the maker order
* takerKey: Key index of the taker order 
* makerBaseAmount: Amount paid by the maker to the taker (deduct this from the maker base amount)
* makerBaseDecimals: number of decimal of the currency offered by maker
* makerBaseSymbol: symbol of the currency offered by maker
* makerCounterAmount: Amount to deduct from the maker counter amount to keep asked price constant
* makerCounterDecimals: number of decimal of the currency asked by maker
* makerCounterSymbol: symbol of the currency asked by maker
* takerBaseAmount: Amount paid by the taker to the maker (deduct this from the taker base amount)
* takerBaseDecimals: number of decimal of the currency offered by taker
* takerBaseSymbol: symbol of the currency offered by taker
* takerCounterAmount: Amount to deduct from the taker counter amount to keep price constant
* takerCounterDecimals: number of decimal of the currency asked by taker
* takerCounterSymbol: symbol of the currency asked by taker
* memo: a memo

## Get Orders
```javascript
let response = getOrders(params)
```
This method is used to query the full order list directly from the blockchain state.
It can be called by anybody as this information is public.
For convenience, we paginate the results (with the optional page and limit parameters). 

### Parameters (Optional):
The user can supply optional filters to refine the query.
* params.user: Filter the results to keep only the orders from user (OPTIONAL)
* params.sender: Filter the results to keep only the orders originating from sender (OPTIONAL)
* params.baseSymbol: Filter the results to keep only orders offering baseSymbol currency (OPTIONAL)
* params.counterSymbol: Filter the results to keep only orders asking for counterSymbol currency (OPTIONAL)
* params.page: Page number to return (OPTIONAL, requires params.limit)
* params.limit: Number of orders to include per page (OPTIONAL, requires params.page)