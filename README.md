# Summary

Building from the functionality of our [Basic Contract](https://github.com/Interblockchain/BasicContract), we build a non-custodian exchange smart contract. Contrary to custodian exchanges, we do not require users to have accounts which are handled by our contract. As such, we also have no need of a deposit or withdrawal feature. Of course, this limits our exchange to assets generated with the [Basic Contract](https://github.com/Interblockchain/BasicContract)

In order to pool the liquidity between all partnered exchanges applications, we have elected to store all orders in one central location, i.e. directly in the scope of the contract. All partners can add orders to the pool. There orders are identified with there account names and there required fees. When orders are settled, the partners that contributed the orders are paid the specified fees, guaranteeing a fair revenue stream. Fees must be charged in our custom token.  

In order to simplify and minimize the database, we have chosen to work within a symmetric framework, abstracting the notion of buy or sell from the orders. Each order will instead correspond to a desired transaction between two amount of assets. Orders will specify the amount and asset that are offered by the user (in the base structure) and the amount and asset the user is willing to accept as trade for the full order (in the counter structure). This way of doing things is more transparent, less error prone and as the added benefit of only handling asset amounts (conversions and other rounding prone calculations are left off-chain).

# User Experience

Users that want to trade on our exchange will have to :

1) Allow the exchange to transfer a specified amount of assets in there name using the Approve action from the [Basic Contract](https://github.com/Interblockchain/BasicContract). Since all fees must be paid in our own custom token, they must also hold and approve the exchange for this token.

2) The user then trades on an affiliated exchange app which issues the createOrder action on the exchange contract. The user must sign the transaction with is own key and the RAM cost of storing this order is paid by the user. The affiliated partner sets the sender and fees properties of the order. These should be made visible to the user in the UI (they are visible in the transaction and state on the blockchain). 
   
3) If a compatible counter offer already exists on the exchange, then both offers are matched and the transaction is issued by a matching engine (which issues a settleorders action with is own authority). The exchange contract directly transfers the related funds (using the allowance table and the transferFrom action of the [Basic Contract](https://github.com/Interblockchain/BasicContract)) in one transaction, garanteeing that that both parties pay the agreed upon price or the transaction fails. Also included in the transaction is the paiement of fees: both the maker and taker pay their respective affiliated exchange.

# Table of Content
We provide two repositories:
* [contract](./contract/README.md): Contains the exchange smart contract which is deployed on the eosio chain 
* [javascript](./javascript/README.md): Contains a small module to easily interface our exchange smart contract with eos-transit

