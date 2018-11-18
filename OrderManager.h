
#ifndef __ORDER_MANAGER_H
#define __ORDER_MANAGER_H

#include "challenge.h"
#include "WriteFirstRWLock.h"

#include <unordered_map>
#include <string>
#include <ostream>
#include <iostream>
#include <cstring>

struct OrderAttribute {
	OrderAttribute() = default;
	OrderAttribute(uint32_t p, uint32_t q)
		: _price(p)
		, _quantity(q)
	{}

	uint32_t _price{ 0 };
	uint32_t _quantity{ 0 };
};


// Assuming the maximum number of markets, the data will be stored in different
// containers by different market Numbers, increasing concurrent access.
const int MAX_NUMBER_OF_MARKET  = 1000;
const int DEFAULT_PLACEHOLDER   = 0;
const uint16_t UINT16_UNDEF     = -1;


namespace std
{
	inline std::ostream & operator<<(std::ostream &ostream, const OrderAttribute &attr)
	{
		ostream  << "(p:" << attr._price << ", q:" << attr._quantity << ")";
		return ostream;
	}

	inline std::ostream & operator<<(std::ostream &ostream, const OrderIdentifier &identifier)
	{
		ostream << "(M:" << identifier._market << ","
				<< "D:" << identifier._desk << ","
				<< "T:" << identifier._trader << ","
				<< "I:" << identifier._sequence <<")";
		return ostream;
	}
}


class OrderManager :
	public IOrderManager
{
public:
	OrderManager();
	~OrderManager();
    
    static OrderManager & GetInstance()
    {
        static OrderManager m_instance;
        return m_instance;
    }
	
	// trader operations - these return false if there is a problem
	virtual bool OnTraderEnter(const OrderIdentifier& aInternal, uint32_t
		aPrice, uint32_t aQuantity);
	virtual bool OnTraderCancel(const OrderIdentifier& aInternal);

	// exchange operations - these return false if there is a problem
	virtual bool OnExchangeNew(const OrderIdentifier& aInternal, const
		std::string& aExternal);
	virtual bool OnExchangeTrade(const std::string& aExternal, uint32_t
		aQuantity);
	virtual bool OnExchangeCancel(const std::string& aExternal);

	virtual bool IsOrderActive(const OrderIdentifier& aInternal) const;
	virtual bool IsOrderActive(const std::string& aExternal) const;

	// returns the quantity of the order that is active in the market, 
	// or zero if the order isn't recognised or is not active
	virtual uint32_t GetActiveOrderQuantity(const OrderIdentifier&
		aInternal) const;

	// Displays the current value of the member variable of interest
	void Dump();
    
	// Reset the member variable for the Host test
    void Reset();
    

private:
	std::string OrderIdentifierToStr(const OrderIdentifier& aInternal) const;

	uint16_t GetIndexOfMarket(const std::string& aExternal) const;

private:
    
    WfirstRWLock m_rwLockOfSubmittedMap;
    mutable WfirstRWLock m_rwLockOfActivatedMap[MAX_NUMBER_OF_MARKET];
    WfirstRWLock m_rwLockOfCancelledMap;
    WfirstRWLock m_rwLockOfCancelPending;
    
    mutable WfirstRWLock m_rwLockOfIdExtStrCorresponding;
    WfirstRWLock m_rwLockOfIdIntStrCorresponding;
    
	std::unordered_map<std::string, OrderAttribute> m_order_submitted;
	std::unordered_map<std::string, OrderAttribute> m_order_activated[MAX_NUMBER_OF_MARKET];
	std::unordered_map<std::string, OrderAttribute> m_order_cancelled;
    std::unordered_map<std::string, int /* placeholder */> m_order_cancel_pending;
    
    std::unordered_map<std::string/* aExternal */, std::string/* aInternal */>    m_id_ext_str_corresponding;
    std::unordered_map<std::string/* aInternal */, std::string/* aExternal */>    m_id_int_str_corresponding;
    
};

#endif // __ORDER_MANAGER_H
