#include "OrderManager.h"

#include <iostream>

OrderManager::OrderManager()
{
}

OrderManager::~OrderManager()
{
}

bool OrderManager::OnTraderEnter(const OrderIdentifier& aInternal, uint32_t
	aPrice, uint32_t aQuantity)
{
    // When multiple traders send the same ID, it always returns: true 
    // as long as the ID has not been confirmed by Exchange.
    
    std::string idStr = OrderIdentifierToStr(aInternal);
    uint16_t marketIndex = aInternal._market;
    
    // For test
    std::cout << idStr << "," << marketIndex <<  std::endl;
    
    // So we only need to detect the activated map and canceled map 
    // (depending on the situation, we can create another map, 
    // to be equal to the contents of these two maps)
    // returns False when it exists
    
    unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
    
    if (m_order_activated[marketIndex].end() != m_order_activated[marketIndex].find(idStr))
    {
        return false;
    }
    
    unique_readguard<WfirstRWLock> readLockCancelled(m_rwLockOfCancelledMap);
    
    if (m_order_cancelled.end() != m_order_cancelled.find(idStr))
    {
        return false;
    }
    
    // Don't care the sumitted map.
    
    unique_writeguard<WfirstRWLock> writeLockSubmitted(m_rwLockOfSubmittedMap);
    
    // The last time the quantity and price are set as valid values.
	m_order_submitted[idStr] = OrderAttribute(aPrice, aQuantity);

	return true;
}

bool OrderManager::OnTraderCancel(const OrderIdentifier& aInternal)
{
    std::string extIdStr("");
    std::string intIdStr = OrderIdentifierToStr(aInternal);
    
    {
        unique_readguard<WfirstRWLock> readLockCancelPending(m_rwLockOfCancelPending);
        
        if (m_order_cancel_pending.end() != m_order_cancel_pending.find(intIdStr))
        {
            return false;
        }
    }
    
    {
        unique_writeguard<WfirstRWLock> writeLockCancelPending(m_rwLockOfCancelPending);
        m_order_cancel_pending[intIdStr] = DEFAULT_PLACEHOLDER;
    }
    
    /* For test! */
    //{
    //    unique_readguard<WfirstRWLock> readLockIntCorresponding(m_rwLockOfIdIntStrCorresponding);
    //    extIdStr = m_id_int_str_corresponding[intIdStr];
    //}
    
    //OnExchangeCancel(extIdStr);
    /* End, For test! */
    
	return true;
}

bool OrderManager::OnExchangeNew(const OrderIdentifier& aInternal, const
	std::string& aExternal)
{
    std::string idStr = OrderIdentifierToStr(aInternal);
    uint16_t marketIndex = aInternal._market;
    
    // If it exists in the submitted map.
    {
        unique_readguard<WfirstRWLock> readLockSubmitted(m_rwLockOfSubmittedMap);
        if(m_order_submitted.end() == m_order_submitted.find(idStr))
        {
            std::cout << "OnExchangeNew(), [" << idStr << "] not in order_submited!" <<  std::endl;
            return false;
        }
  
        unique_readguard<WfirstRWLock> readLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
        if(m_id_ext_str_corresponding.end() != m_id_ext_str_corresponding.find(aExternal))
        {
            std::cout << "OnExchangeNew(), [" << aExternal << "] in the ext_str_corresponding!" << std::endl;
            return false;
        }
    }
    
    {
        unique_writeguard<WfirstRWLock> writeLockActivated(m_rwLockOfActivatedMap[marketIndex]);
        m_order_activated[marketIndex][idStr] = m_order_submitted[idStr];
    }
    
    {
        unique_writeguard<WfirstRWLock> writeLockSubmitted(m_rwLockOfSubmittedMap);
        // remove the identifier from the submitted map
        m_order_submitted.erase(idStr);
    }
    
    {
        unique_writeguard<WfirstRWLock> writeLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
        m_id_ext_str_corresponding[aExternal] = idStr;
    }
    
    {
        unique_writeguard<WfirstRWLock> writeLockIntCorresponding(m_rwLockOfIdIntStrCorresponding);
        m_id_int_str_corresponding[idStr] = aExternal;
    }
    
	return true;
}

bool OrderManager::OnExchangeTrade(const std::string& aExternal, uint32_t
	aQuantity)
{
    std::string idStr("");
    
    {
        unique_readguard<WfirstRWLock> readLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
        if(m_id_ext_str_corresponding.end() == m_id_ext_str_corresponding.find(aExternal))
        {
            std::cout << "OnExchangeTrade(), [" << aExternal << "] not in ext_str_corresponding!" << std::endl;
            return false;
        }
        
        idStr = m_id_ext_str_corresponding[aExternal];
    }
    
    uint16_t marketIndex = GetIndexOfMarket(idStr);
    
    {
        unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
        if(m_order_activated[marketIndex].end() == m_order_activated[marketIndex].find(idStr))
        {
            std::cout << "OnExchangeTrade(), [" << idStr << " not in order_activated!" << std::endl;
            return false;
        }
    }
    
    OrderAttribute tempAttribut(-1,-1);
    {
        unique_writeguard<WfirstRWLock> writeLockActivated(m_rwLockOfActivatedMap[marketIndex]);
        
        if(m_order_activated[marketIndex][idStr]._quantity >= aQuantity)
        {
            m_order_activated[marketIndex][idStr]._quantity -= aQuantity;
        }
        else
        {
            //m_order_activated[marketIndex][idStr]._quantity = 0;
			return false;
        }

		if (0 == m_order_activated[marketIndex][idStr]._quantity)
		{
			tempAttribut = m_order_activated[marketIndex][idStr];

			m_order_activated[marketIndex].erase(idStr);
		}
    }

	if (tempAttribut._quantity == 0)
	{
		unique_writeguard<WfirstRWLock> writeLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
		m_order_cancelled[idStr] = tempAttribut; /* quantity = 0 */
		
		// Update the cancel pending map.
		unique_writeguard<WfirstRWLock> writeLockCancelPending(m_rwLockOfCancelPending);
		if (m_order_cancel_pending.end() != m_order_cancel_pending.find(idStr))
		{
			m_order_cancel_pending.erase(idStr);
		}
	}
    
	return true;
}

bool OrderManager::OnExchangeCancel(const std::string& aExternal)
{
    std::string idStr("");
    
	{
        unique_readguard<WfirstRWLock> readLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
        if(m_id_ext_str_corresponding.end() == m_id_ext_str_corresponding.find(aExternal))
        {
            std::cout << "OnExchangeCancel(), [" <<  aExternal << "] not in ext_str_corresponding!" << std::endl;
            return false;
        }
        
        
        idStr = m_id_ext_str_corresponding[aExternal];
    }
    
    uint16_t marketIndex = GetIndexOfMarket(idStr);
    
    {
        unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
        if(m_order_activated[marketIndex].end() == m_order_activated[marketIndex].find(idStr))
        {
            std::cout << "OnExchangeCancel(), [" << idStr << " not in order_activated!" << std::endl;
            return false;
        }
    }
    
    OrderAttribute tempAttribut;
    {
        unique_writeguard<WfirstRWLock> writeLockActivated(m_rwLockOfActivatedMap[marketIndex]);

        tempAttribut = m_order_activated[marketIndex][idStr];
        m_order_activated[marketIndex].erase(idStr);
    }
    
    unique_writeguard<WfirstRWLock> writeLockCorresponding(m_rwLockOfIdExtStrCorresponding);

    m_order_cancelled[idStr] = tempAttribut; /* quantity != 0 */
    
    // Update the cancel pending map.
    {
        unique_writeguard<WfirstRWLock> writeLockCancelPending(m_rwLockOfCancelPending);
        if(m_order_cancel_pending.end() != m_order_cancel_pending.find(idStr))
        {
            m_order_cancel_pending.erase(idStr);
        }
    }
   
	return true;
}

bool OrderManager::IsOrderActive(const OrderIdentifier& aInternal) const
{    
    std::string idStr = OrderIdentifierToStr(aInternal);
    uint16_t marketIndex = aInternal._market;    
    
    unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
    
    // Or check the canceled map.
    if(m_order_activated[marketIndex].end() == m_order_activated[marketIndex].find(idStr))
    {
        //std::cout << "IsOrderActive(Internal), [" <<  idStr << " not in order_activated!" << std::endl;
        return false;
    }
    
	return true;
}

bool OrderManager::IsOrderActive(const std::string& aExternal) const
{
    std::string idStr("");

	{
        unique_readguard<WfirstRWLock> readLockExtCorresponding(m_rwLockOfIdExtStrCorresponding);
        if(m_id_ext_str_corresponding.end() == m_id_ext_str_corresponding.find(aExternal))
        {
            //std::cout << "IsOrderActive(External), [" << aExternal << " not in ext_str_corresponding!" << std::endl;
            return false;
        }
        else
        {
            idStr = m_id_ext_str_corresponding.find(aExternal)->second;
        }
    }
    
    uint16_t marketIndex = GetIndexOfMarket(idStr);
    
    //if(UINT16_UNDEF == marketIndex)
    //{
    //    return false;
    //}
    
    
    unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
    if(m_order_activated[marketIndex].end() == m_order_activated[marketIndex].find(idStr))
    {
        //std::cout << "IsOrderActive(External), [" <<  idStr << " not in order_activated!" << std::endl;
        return false;
    }
    
	return true;
}

uint32_t OrderManager::GetActiveOrderQuantity(const OrderIdentifier&
	aInternal) const
{
    
	std::string idStr = OrderIdentifierToStr(aInternal);
    uint16_t marketIndex = aInternal._market;
    
    unique_readguard<WfirstRWLock> readLockActivated(m_rwLockOfActivatedMap[marketIndex]);
    
    if (m_order_activated[marketIndex].end() == m_order_activated[marketIndex].find(idStr))
    {
        return 0;
    }
    else
    {
        return m_order_activated[marketIndex].find(idStr)->second._quantity;
    }
}

void OrderManager::Dump()
{
    std::cout << "==============================================" << std::endl;
    std::cout << "order_submitted" << std::endl;
    for (auto x : m_order_submitted)
        std::cout << x.first << " => " << x.second << std::endl;
    
    std::cout << "==============================================" << std::endl;
    std::cout << "order_activated" << std::endl;
    for (auto i = 0; i < MAX_NUMBER_OF_MARKET; i++)
        for (auto x : m_order_activated[i])
            std::cout << x.first << " => " << x.second << std::endl;

    std::cout << "==============================================" << std::endl;
    std::cout << "order_cancelled" << std::endl;
    for (auto x : m_order_cancelled)
        std::cout << x.first << " => " << x.second << std::endl;
    
    std::cout << "==============================================" << std::endl;
	std::cout << "id_ext_str_corresponding" << std::endl;
    for (auto x : m_id_ext_str_corresponding)
        std::cout << x.first << " => " << x.second << std::endl;
}
    
void OrderManager::Reset()
{
    std::cout << "==============================================" << std::endl;
    std::cout << "Reset order_submitted" << std::endl;
    m_order_submitted.clear();
    
    
    std::cout << "==============================================" << std::endl;
    std::cout << "Reset order_activated" << std::endl;
    for (auto i = 0; i < MAX_NUMBER_OF_MARKET; i++)
        m_order_activated[i].clear();
    
    
    std::cout << "==============================================" << std::endl;
    std::cout << "Reset order_cancelled" << std::endl;
    m_order_cancelled.clear();
    
    
    std::cout << "==============================================" << std::endl;
    std::cout << "Reset order_cancel_pending" << std::endl;
    m_order_cancel_pending.clear();

    std::cout << "==============================================" << std::endl;
    std::cout << "Reset id_ext_str_corresponding" << std::endl;
    m_id_ext_str_corresponding.clear();
    
    std::cout << "==============================================" << std::endl;
    std::cout << "Reset id_int_str_corresponding" << std::endl;
    m_id_int_str_corresponding.clear();
}

std::string OrderManager::OrderIdentifierToStr(const OrderIdentifier& aInternal) const
{
	const int n = 128;
	char buf[n];
	int ret = snprintf(buf, n, "_market%d_desk%d_trader%d_sequence%d",
		aInternal._market, aInternal._desk,
		aInternal._trader, aInternal._sequence
	);

	return std::string(buf);
}

uint16_t OrderManager::GetIndexOfMarket(const std::string& aExternal) const
{
	char marketPrefix[8] = "_market";
	std::size_t found = aExternal.find(marketPrefix);

	//if(aExternal.empty())
	//{
	//    return UINT16_UNDEF;
	//}

	if (std::string::npos == found)
	{
		// It must exist in the activated map, 
		// and this statement will not be executed.
		std::cout << marketPrefix << " is not in \"" << aExternal << "\"" << std::endl;
		std::cout << "Error: marketIndex is invalid default(" << UINT16_UNDEF << ")" << std::endl;
		return UINT16_UNDEF;
	}

	return std::stoi(aExternal.substr(found + strlen(marketPrefix)));
}