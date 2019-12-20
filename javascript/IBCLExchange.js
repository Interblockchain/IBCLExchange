/* ==================================================
(c) 2019 Copyright Transledger inc. All rights reserved 
================================================== */

const BigNumber = require('bignumber.js');
const { UInt64 } = require('int64_t');
const axios = require('axios');

class transeos {
    constructor(params) {
        this.exchangeAddress = params.exchangeAddress;
        this.network = params.network;
    }

    /*
    This method is used to sign and send actions to the EOS network.
    Arguments:
        actions: array of actions
    An action is an object of the form:
        {   account: account of the contract,
            name: name of the function to run on the contract,
            authorization: [ { actor: account taking the action, permission: permission level (usually "active") } ],
            data: { the arguments of the function call of the contract}
        }
    Returns:
        result: the result from the blockchain for the action
    */
    async sendAction(wallet, actions) {
        try {
            let result = await wallet.eosApi.transact({ actions: actions }, { broadcast: true, blocksBehind: 3, expireSeconds: 60 });
            console.log('Transaction success!', result);
            return result;
        } catch (error) {
            console.error('Transaction error :(', error);
            throw error;
        }
    }

    /*=========================================================================================
      EXCHANGE CONTRACT ACTIONS
      =========================================================================================
    */

    /*
    This method creates an order in the specified network.
    The arguments are:
        wallet: transit-eos wallet object which provide keys for the action
        user: account of the user, possessing the tokens, that will be used in the order (authorization for this account must be provided in wallet)
        sender: account of the relayer (exchange) which will get the commission (fees) for this order 
        baseAmount: amount of tokens offered by 'user'
        baseDecimals: number of decimals used for the currency offered by 'user'
        baseSymbol: symbol of the currency offered by 'user'
        counterAmount: amount of tokens wanted for is offer
        counterDecimals: number of decimals used for the wanted currency
        counterSymbol: symbol of the wanted currency
        feesAmount: amount of fees (in GIZMO) the relayer is charging
        memo: a memo
        expires: Expiration date of the offer [should be in milliseconds since 1970, like Date.now()]
    Returns:
        result: the result from the blockchain for the action
    */
    async createOrder(wallet, user, sender, baseAmount, baseDecimals, baseSymbol, counterAmount, counterDecimals, counterSymbol, feesAmount, memo, expires) {
        //Validation
        if (!wallet) { throw { name: "No wallet has been passed", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!wallet.auth) { throw { name: "No auth information has been passed with wallet", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!user || typeof (user) != "string") { throw { name: "No user has been passed or it is not of type string", statusCode: "400", message: "Please provide a user for the order." } }
        if (!sender || typeof (sender) != "string") { throw { name: "No sender has been passed or it is not of type string", statusCode: "400", message: "Please provide a sender for the order." } }
        if (!baseAmount) { throw { name: "No offer quantity has been passed", statusCode: "400", message: "Please provide an offer quantity for the order." } }
        if (!baseDecimals) { throw { name: "No offer decimals has been passed", statusCode: "400", message: "Please provide a number of decimal for this currency." } }
        if (!baseSymbol || typeof (baseSymbol) != "string") { throw { name: "No offer symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the offer." } }
        if (!counterAmount) { throw { name: "No counter quantity has been passed", statusCode: "400", message: "Please provide a quantity for the order." } }
        if (!counterDecimals) { throw { name: "No counter decimals has been passed", statusCode: "400", message: "Please provide a number of counter decimal for this currency." } }
        if (!counterSymbol || typeof (counterSymbol) != "string") { throw { name: "No counter symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the counter." } }
        if (!expires) { throw { name: "No expire date has been passed", statusCode: "400", message: "Please provide an expire date (in miliseconds like Date.now()) for the order." } }

        let timestamp = Date.now();
        let key = this.getKeyForOrder(user, baseSymbol, timestamp);
        let _memo = (memo) ? memo : `Issue order ${key}`;

        BigNumber.set({ DECIMAL_PLACES: baseDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        let base = new BigNumber(baseAmount, 10);
        let baseStr = base.toString();
        BigNumber.set({ DECIMAL_PLACES: counterDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        let counter = new BigNumber(counterAmount, 10);
        let counterStr = counter.toString();
        BigNumber.set({ DECIMAL_PLACES: 8, ROUNDING_MODE: BigNumber.ROUND_DOWN });  //HERE CHECK NUMBER OF DECIMALS
        let fees = new BigNumber(feesAmount, 10);
        let feesStr = fees.toString();

        let result = await this.sendAction(wallet, [{
            account: this.exchangeAddress,
            name: 'createorder',
            authorization: [{ actor: user, permission: wallet.auth.permission }],
            data: {
                user: user,
                sender: sender,
                key: key,
                base: `${baseStr} ${baseSymbol}`,
                counter: `${counterStr} ${counterSymbol}`,
                fees: `${feesStr} GIZMO`,   //HERE CHECK TOKEN Symbol
                memo: _memo,
                timestamp: timestamp,
                expires: expires
            }
        }]);
        return result;
    }

    /*
     This method allows to change an order. The only variables that can be changed are: the expiration date and the amounts offered and required by the order. 
     The arguments are:
        wallet: transit-eos wallet object which provide keys for the action
        user: account of the user, possessing the tokens, that will be used in the order (authorization for this account must be provided in wallet)
        key: key identifying the order
        baseAmount: amount of tokens offered by 'user'
        baseDecimals: number of decimals used for the currency offered by 'user'
        baseSymbol: symbol of the currency offered by 'user'
        counterAmount: amount of tokens wanted for is offer
        counterDecimals: number of decimals used for the wanted currency
        counterSymbol: symbol of the wanted currency
        expires: Expiration date of the offer [should be in milliseconds since 1970, like Date.now()]
     Returns:
         result: the result from the blockchain for the action
     */
    async editOrder(wallet, user, key, baseAmount, baseDecimals, baseSymbol, counterAmount, counterDecimals, counterSymbol, expires) {
        //Validation
        if (!wallet) { throw { name: "No wallet has been passed", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!wallet.auth) { throw { name: "No auth information has been passed with wallet", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!user || typeof (user) != "string") { throw { name: "No user has been passed or it is not of type string", statusCode: "400", message: "Please provide a user for the order." } }
        if (!key) { throw { name: "No key has been passed", statusCode: "400", message: "Please provide a key identifying the order to modify." } }
        if (!baseAmount) { throw { name: "No offer quantity has been passed", statusCode: "400", message: "Please provide an offer quantity for the order." } }
        if (!baseDecimals) { throw { name: "No offer decimals has been passed", statusCode: "400", message: "Please provide a number of decimal for this currency." } }
        if (!baseSymbol || typeof (baseSymbol) != "string") { throw { name: "No offer symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the offer." } }
        if (!counterAmount) { throw { name: "No counter quantity has been passed", statusCode: "400", message: "Please provide a quantity for the order." } }
        if (!counterDecimals) { throw { name: "No counter decimals has been passed", statusCode: "400", message: "Please provide a number of counter decimal for this currency." } }
        if (!counterSymbol || typeof (counterSymbol) != "string") { throw { name: "No counter symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the counter." } }
        if (!expires) { throw { name: "No expire date has been passed", statusCode: "400", message: "Please provide an expire date (in miliseconds like Date.now()) for the order." } }

        BigNumber.set({ DECIMAL_PLACES: baseDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        let base = new BigNumber(baseAmount, 10);
        let baseStr = base.toString();
        BigNumber.set({ DECIMAL_PLACES: counterDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        let counter = new BigNumber(counterAmount, 10);
        let counterStr = counter.toString();

        let result = await this.sendAction(wallet, [{
            account: this.exchangeAddress,
            name: 'editorder',
            authorization: [{ actor: user, permission: wallet.auth.permission }],
            data: {
                key: key,
                base: `${baseStr} ${baseSymbol}`,
                counter: `${counterStr} ${counterSymbol}`,
                expires: expires
            }
        }]);
        return result;
    }

    /*
     This method allows to delete an existing order.
     Must be called by the owner of the order.
     The arguments are:
        wallet: transit-eos wallet object which provide keys for the action
        user: account of the user possessing the order (authorization for this account must be provided in wallet)
        key: key identifying the order
     Returns:
         result: the result from the blockchain for the action
     */
    async cancelOrder(wallet, user, key) {
        //Validation
        if (!wallet) { throw { name: "No wallet has been passed", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!wallet.auth) { throw { name: "No auth information has been passed with wallet", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!user || typeof (user) != "string") { throw { name: "No user has been passed or it is not of type string", statusCode: "400", message: "Please provide a user for the order." } }
        if (!key) { throw { name: "No key has been passed", statusCode: "400", message: "Please provide a key identifying the order to delete." } }

        let result = await this.sendAction(wallet, [{
            account: this.exchangeAddress,
            name: 'cancelorder',
            authorization: [{ actor: user, permission: wallet.auth.permission }],
            data: {
                key: key
            }
        }]);
        return result;
    }

    /*
         This method allows to delete an existing order when the expiration is reached.
         Can be called by anybody on an expired order (used for garbage collection).
         The arguments are:
            wallet: transit-eos wallet object which provide keys for the action
            sender: account of the sender retiring the order (authorization for this account must be provided in wallet)
            key: key identifying the order
         Returns:
             result: the result from the blockchain for the action
    */
    async retireOrder(wallet, sender, key) {
        //Validation
        if (!wallet) { throw { name: "No wallet has been passed", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!wallet.auth) { throw { name: "No auth information has been passed with wallet", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!sender || typeof (sender) != "string") { throw { name: "No sender has been passed or it is not of type string", statusCode: "400", message: "Please provide a sender for the order." } }
        if (!key) { throw { name: "No key has been passed", statusCode: "400", message: "Please provide a key identifying the order to retire." } }

        let result = await this.sendAction(wallet, [{
            account: this.exchangeAddress,
            name: 'retireorder',
            authorization: [{ actor: sender, permission: wallet.auth.permission }],
            data: {
                key: key
            }
        }]);
        return result;
    }

    /*
     This method allows to settle to matching orders.
     Can be called by anybody, checks are made to guarantee the matching.
     The arguments are:
        wallet: transit-eos wallet object which provide keys for the action
        sender: account of the user issuing the settlement (authorization for this account must be provided in wallet)
        makerKey: Key index of the maker order
        takerKey: Key index of the taker order 
        makerBaseAmount: Amount payed by the maker to the taker (deduct this from the maker base amount)
        makerBaseDecimals: number of decimal of the currency offered by maker
        makerBaseSymbol: symbol of the currency offered by maker
        makerCounterAmount: Amount to deduct from the maker counter amount to keep asked price constant
        makerCounterDecimals: number of decimal of the currency asked by maker
        makerCounterSymbol: symbol of the currency asked by maker
        takerBaseAmount: Amount payed by the taker to the maker (deduct this from the taker base amount)
        takerBaseDecimals: number of decimal of the currency offered by taker
        takerBaseSymbol: symbol of the currency offered by taker
        takerCounterAmount: Amount to deduct from the taker counter amount to keep price constant
        takerCounterDecimals: number of decimal of the currency asked by taker
        takerCounterSymbol: symbol of the currency asked by taker
        memo: a memo
     Returns:
         rows: the result from the blockchain for the action
     */
    async settleOrders(wallet, sender, makerKey, takerKey, makerBaseAmount, makerBaseDecimals, makerBaseSymbol, makerCounterAmount, makerCounterDecimals, makerCounterSymbol, takerBaseAmount, takerBaseDecimals, takerBaseSymbol, takerCounterAmount, takerCounterDecimals, takerCounterSymbol , memo) {
        //Validation
        if (!wallet) { throw { name: "No wallet has been passed", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!wallet.auth) { throw { name: "No auth information has been passed with wallet", statusCode: "400", message: "Please provide an authenticated wallet." } }
        if (!sender || typeof (sender) != "string") { throw { name: "No sender has been passed or it is not of type string", statusCode: "400", message: "Please provide a sender for the action." } }
        if (!makerKey) { throw { name: "No maker key has been passed", statusCode: "400", message: "Please provide a maker key identifying the order to settle." } }
        if (!takerKey) { throw { name: "No taker key has been passed", statusCode: "400", message: "Please provide a taker key identifying the order to settle." } }
        if (!makerBaseAmount) { throw { name: "No  maker offer quantity has been passed", statusCode: "400", message: "Please provide a maker offer quantity." } }
        if (!makerBaseDecimals) { throw { name: "No maker offer decimals has been passed", statusCode: "400", message: "Please provide a number of decimal for this currency." } }
        if (!makerBaseSymbol || typeof (makerBaseSymbol) != "string") { throw { name: "No  maker offer symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the maker offer." } }
        if (!makerCounterAmount) { throw { name: "No maker counter quantity has been passed", statusCode: "400", message: "Please provide maker counter quantity." } }
        if (!makerCounterDecimals) { throw { name: "No maker counter decimals has been passed", statusCode: "400", message: "Please provide a number of maker counter decimal for this currency." } }
        if (!makerCounterSymbol || typeof (makerCounterSymbol) != "string") { throw { name: "No maker counter symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the maker counter." } }
        if (!takerBaseAmount) { throw { name: "No taker offer quantity has been passed", statusCode: "400", message: "Please provide a taker offer quantity." } }
        if (!takerBaseDecimals) { throw { name: "No taker offer decimals has been passed", statusCode: "400", message: "Please provide a number of decimal for this currency." } }
        if (!takerBaseSymbol || typeof (takerBaseSymbol) != "string") { throw { name: "No taker offer symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the taker offer." } }
        if (!takerCounterAmount) { throw { name: "No taker counter quantity has been passed", statusCode: "400", message: "Please provide a taker counter quantity." } }
        if (!takerCounterDecimals) { throw { name: "No taker counter decimals has been passed", statusCode: "400", message: "Please provide a number of taker counter decimal for this currency." } }
        if (!takerCounterSymbol || typeof (takerCounterSymbol) != "string") { throw { name: "No taker counter symbol has been passed or it is not of type string", statusCode: "400", message: "Please provide a token symbol for the taker counter." } }

        // Amount to deduct from the maker amounts
        BigNumber.set({ DECIMAL_PLACES: makerBaseDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        base = new BigNumber(makerBaseAmount, 10);
        let makerBaseStr = base.toString();
        BigNumber.set({ DECIMAL_PLACES: makerCounterDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        let counter = new BigNumber(makerCounterAmount, 10);
        let makerCounterStr = counter.toString();
        // Amount to deduct from the taker amounts
        BigNumber.set({ DECIMAL_PLACES: takerBaseDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        base = new BigNumber(takerBaseAmount, 10);
        let takerBaseStr = base.toString();
        BigNumber.set({ DECIMAL_PLACES: takerCounterDecimals, ROUNDING_MODE: BigNumber.ROUND_DOWN });
        counter = new BigNumber(takerCounterAmount, 10);
        let takerCounterStr = counter.toString();
        
        let result = await this.sendAction(wallet, [{
            account: this.exchangeAddress,
            name: 'settleorders',
            authorization: [{ actor: sender, permission: wallet.auth.permission }],
            data: {
                maker: makerKey,                                              // Key of the maker order
                taker: takerKey,                                              // Key of the taker order
                quantity_maker: makerBaseStr + " " + makerBaseSymbol,         // Quantity payed by the maker
                deduct_maker: makerCounterStr + " " + makerCounterSymbol,     // Quantity to deduct from maker.counter
                quantity_taker: takerBaseStr + " " + takerBaseSymbol,         // Quantity payed by the taker
                deduct_taker: takerCounterStr + " " + takerCounterSymbol,     // Quantity to deduct from taker.counter
                memo: memo
            }
        }]);
        return result;
    }

    /*
         This method allows to query the full order list directly from the blockchain state.
         Can be called by anybody as this information is public.
         The arguments are optional and contained in an object:
            params.user: Filter the results to keep only the orders from user (OPTIONAL)
            params.sender: Filter the results to keep only the orders originating from sender (OPTIONAL)
            params.baseSymbol: Filter the results to keep only orders offering baseSymbol currency (OPTIONAL)
            params.counterSymbol: Filter the results to keep only orders asking for counterSymbol currency (OPTIONAL)
         Returns:
             result: the paginated list of orders on the blockchain
    */
   async getOrders(params) {
    try {
        let result = await axios({
            method: 'POST',
            url: (this.network.port) ? `${this.network.protocol}://${this.network.host}:${this.network.port}/v1/chain/get_table_rows` : `${this.network.protocol}://${this.network.host}/v1/chain/get_table_rows`,
            headers: {
                'content-type': 'application/x-www-form-urlencoded; charset=UTF-8'
            },
            data: {
                code: this.exchangeAddress,
                scope: this.exchangeAddress,
                table: "orders",
                json: true
            }
        });
        let rows = result.data.rows;

        if (params && params.user && Array.isArray(rows)) rows = rows.filter(element => element.user == params.user);
        if (params && params.sender && Array.isArray(rows)) rows = rows.filter(element => element.sender == params.sender);
        if (params && params.baseSymbol && Array.isArray(rows)) rows = rows.filter(element => element.base.split(" ")[1] == params.baseSymbol);
        if (params && params.counterSymbol && Array.isArray(rows)) rows = rows.filter(element => element.counter.split(" ")[1] == params.counterSymbol);
        let total = rows.length;
        if (params && params.page && params.limit && Array.isArray(rows)) rows = this.paginateArray(rows, page, limit);

        return {
            docs: rows,
            total: total,
            limit: (params.limit) ? parseInt(params.limit) : total,
            page:  (params.page) ? parseInt(params.page) : 1,
            pages: (params.limit) ? Math.ceil(total / parseInt(params.limit)) : 1
        };
    } catch (error) {
        throw { name: error.name, statusCode: "500", message: error.message }
    }
   }

    /*=========================================================================================
     PRIVATE METHODS
     =========================================================================================*/
    charToValue(c) {
        let value = c.charCodeAt(0);
        //console.log(c + "  " + c.charCodeAt(0));
        if (value == 46) { return 0; }
        else if (value >= 49 && value <= 53) { return value - 48; }
        else if (value >= 97 && value <= 122) { return value - 91; }
        else {
            throw { message: "character is not allowed in character set for names" };
        }
    }

    strToName(str) {
        let value = new UInt64(0x0);
        if (str.length > 13) {
            throw { message: "string is too long to be a valid name" };
        }
        if (str.length == 0) {
            return;
        }
        let n = Math.min(str.length, 12);
        for (var i = 0; i < n; ++i) {
            value = value.shiftLeft(5);
            value = value.or(new UInt64(this.charToValue(str.charAt(i))));
        }
        value = value.shiftLeft(4 + 5 * (12 - n));
        if (str.length == 13) {
            let v = this.charToValue(str.charAt(12));
            if (v > 15) {
                throw { message: "thirteenth character in name cannot be a letter that comes after j" };
            }
            value = value.or(new UINT64(v));
        }
        return value;
    }

    strToSymbol(str) {
        if (str.length > 7) {
            throw { message: "string is too long to be a valid symbol_code" };
        }
        let value = new UInt64(0x0);
        for (let i = str.length - 1; i >= 0; i--) {
            let cv = str.charCodeAt(i);
            if (cv < 65 || cv > 90) { throw { message: "only uppercase letters allowed in symbol_code string" } }
            value = value.shiftLeft(8);
            value = value.or(new UInt64(cv));
        }
        return value;
    }

    getKeyForOrder(user, baseSymbol, timestamp) {
        let account = this.strToName(user);
        // console.log("account: " + account.toString());
        let asset = this.strToSymbol(baseSymbol);
        // console.log("asset: " + asset.toString());
        let time = new UInt64(Number(timestamp));
        // console.log("time: " + time.toString());
        return account.add(asset).add(time).toString();
    }

    paginateArray(array, page_number, page_size) {
        --page_number; // because pages logically start with 1, but technically with 0
        return array.slice(page_number * page_size, (page_number + 1) * page_size);
    }
}

module.exports = transeos;