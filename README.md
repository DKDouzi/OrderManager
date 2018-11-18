# OrderManager
Create a solution for maintaining information about orders that are being placed on an exchange by an order router. This should derive from IOrderManager (provided in challenge.h).

The in-house trading system tracks orders using a unique internal identifier (provided in challenge.h). The exchange tracks orders using a unique text identifier.

Each order has a price and a quantity. Both are uint32_t. Prices and quantities must be positive numbers.
When an order request operation is received from a trader, the order manager must be able to store and retrieve information using the
OrderIdentifier.

When a response is received from the exchange the order manager must be able to retrieve the information using the text identifier.

The trader can perform the following requests. Each of these includes the OrderIdentifier but not the exchange's identifier:
    Enter order. Includes OrderIdentifier, price and quantity but not exchange's identifier (this is provided when the exchange confirms the order).
    Cancel order. Only the OrderIdentifier is provided. The order is considered as still in the market until the exchange confirms the
cancellation.

The exchange can provide the following updates:
    New order confirmed. Contains both the OrderIdentifier and the exchange's text identifier.
    Order traded. Includes quantity traded and exchange's identifier.
    Order cancelled. This can be due to the trader cancelling it, or the exchange cancelling the order. Contains only exchange's identifier.

If all the order quantity is traded then the order should be considered to be removed from the market and stops being active. An order can be traded multiple times until all its quantity is used up.

The trader must not reuse a previously used order identifier, and they are only allowed cancel an order which is active in the market and which they haven't already sent a cancel request for.

An order becomes active when the exchange confirms it, and stops being active when the exchange provides a cancel update on the order or a trade update which uses all of the remaining quantity of the order.

Requests and updates have restrictions in the sequence in which they can occur for a specific order. Here A->B means that B can only occur after A, if it occurs. There are no restrictions on sequencing between different orders.
    Enter order -> New order confirmed
    New order confirmed -> Order traded
    New order confirmed -> Order cancelled
    Order traded -> Order cancelled

You can assume the exchange never sends incorrect information, for instance a trade greater than the order quantity or a cancellation for an order which doesn't exist.

Your order manager solution should handle requests from the trader and responses from the exchange in the form of methods being called in the class you derive from IOrderManager. You are free to create other classes as part of the solution.
