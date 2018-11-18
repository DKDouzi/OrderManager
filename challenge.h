#pragma once

#include <stdint.h>
#include <string>

// company's order identifier
struct OrderIdentifier {
	OrderIdentifier() = default;
	OrderIdentifier(uint16_t m, uint16_t d, uint16_t t, uint32_t s)
		: _market(m)
		, _desk(d)
		, _trader(t)
		, _sequence(s)
	{}

	uint16_t _market{ 0 };
	uint16_t _desk{ 0 };
	uint16_t _trader{ 0 };
	uint32_t _sequence{ 0 }; // increments with each order from a particular trader
};

//
// The order router will call these methods as it receives order operations from the
// trader and exchange.
// Note:
// 1. Thread safety needs to be considered for your implementation of this class: e.g.
// - OnTraderXXX may be called from different threads
// - You can assume OnExchangeXXX will only be called from a single thread.
// - But OnTraderXXX and OnExchangeXXX maybe called from different threads.
// 2. Performance, in terms of latency, of all the interface functions are important.
//
class IOrderManager
{
public:
	// trader operations - these return false if there is a problem
	virtual bool OnTraderEnter(const OrderIdentifier& aInternal, uint32_t
		aPrice, uint32_t aQuantity) = 0;
	virtual bool OnTraderCancel(const OrderIdentifier& aInternal) = 0;

	// exchange operations - these return false if there is a problem
	virtual bool OnExchangeNew(const OrderIdentifier& aInternal, const
		std::string& aExternal) = 0;
	virtual bool OnExchangeTrade(const std::string& aExternal, uint32_t
		aQuantity) = 0;
	virtual bool OnExchangeCancel(const std::string& aExternal) = 0;

	virtual bool IsOrderActive(const OrderIdentifier& aInternal) const = 0;
	virtual bool IsOrderActive(const std::string& aExternal) const = 0;

	// returns the quantity of the order that is active in the market, 
	// or zero if the order isn't recognised or is not active
	virtual uint32_t GetActiveOrderQuantity(const OrderIdentifier&
		aInternal) const = 0;
};
