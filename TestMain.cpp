
#include "OrderManager.h"

#include <iostream>
#include <thread>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <unistd.h>
#endif


OrderManager& g_orderManager = OrderManager::GetInstance();
IOrderManager* g_IOrderManager = &g_orderManager;

const int MAX_NUMBER_OF_THREAD_OF_PER_CLASS = 100; // Must <= const int MAX_NUMBER_OF_MARKET = 1000;

const char * ID_STR_FORMAT = "Test_Market_%d_Dest_%d_Trader_%d_Sequence_%d";

enum OperationType
{
	OP_TRADER_SUBMIT,
	OP_EXCHANGE_ACTIVATE,
	OP_EXCHANGE_TRADE,
	OP_TRADER_CANCEL,
	OP_EXCHANGE_CANCEL,
	OP_GET_ORDER_QUANTITY,
	OP_UNDEF,
};

struct OperationInfo {
	uint16_t		numberOfElement;
	uint16_t		operationThreadIndex[MAX_NUMBER_OF_THREAD_OF_PER_CLASS];
	uint16_t		beginIndexOfOrder[MAX_NUMBER_OF_THREAD_OF_PER_CLASS];
	uint16_t		endIndexOfOrder[MAX_NUMBER_OF_THREAD_OF_PER_CLASS];
};

const uint16_t DEFAULT_UNDEF = 0;

struct TestCfg{
	uint16_t		threadIndex;	// As the value of _market
	OperationType	operationType;
	uint16_t		numberOfOrder;  // The result of the increase is taken as the value of _sequence
	uint16_t		intervalPerSubmit;
	OrderIdentifier defaultIdentifier;
	OrderAttribute	defaultAttribut;
	uint16_t		pendingSecondsBeforeRun; // Simply start the thread with a delay to achieve the execution order of the control operations.
	uint16_t		quantityForSubmit;
	uint16_t		quantityPerTradeForTrade;
	OperationInfo	operationInfo;
};

const TestCfg test_case_cfg[][10] = {
	{
		//test_submit_activate_trade
		{ 0, OP_TRADER_SUBMIT,		20,  1,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },
		{ 1, OP_TRADER_SUBMIT,		10,  2,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },
		{ 2, OP_TRADER_SUBMIT,		 5,  4,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },

		{ 3, OP_EXCHANGE_ACTIVATE,	10, DEFAULT_UNDEF, { 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 0,{ 2, { 0, 1 },{ 0, 0 },{ 5, 5 } } },
		{ 4, OP_EXCHANGE_ACTIVATE,	 9, DEFAULT_UNDEF, { 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 0,{ 2, { 1, 2 },{ 5, 0 },{ 10, 4 } } },

		{ 5, OP_EXCHANGE_TRADE,		4, DEFAULT_UNDEF, { 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 3,{ 2, { 0, 1 },{ 0, 0 },{ 2, 2 } } },
		{ 6, OP_EXCHANGE_TRADE,		11, DEFAULT_UNDEF, { 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 4,{ 3, { 0, 1, 2 },{ 2, 2, 0 },{ 4, 8, 3} } },
		{ 7, OP_UNDEF },
		{ 8, OP_UNDEF },
		{ 9, OP_UNDEF }
	},
	{
		// test_submit_activate_cancel
		{ 0, OP_TRADER_SUBMIT,		20,  1,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },
		{ 1, OP_TRADER_SUBMIT,		10,  2,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },
		{ 2, OP_TRADER_SUBMIT,		 5,  4,{ 0, 0, 0, 0 },{ 10, 10 }, 1, 12, DEFAULT_UNDEF },

		{ 3, OP_EXCHANGE_ACTIVATE,	10, DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 0,{ 2,{ 0, 1 },{ 0, 0 },{ 5, 5 } } },
		{ 4, OP_EXCHANGE_ACTIVATE,	 9, DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 }, 1,  0, 0,{ 2,{ 1, 2 },{ 5, 0 },{ 10, 4 } } },

		//{ 5, OP_UNDEF },
		{ 5, OP_TRADER_CANCEL,		 5,  DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 },  10,  0, 0,{ 2,{ 0, 1 },{ 0, 0 },{ 2, 3 } } },

		{ 6, OP_GET_ORDER_QUANTITY,	35,  DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 },  20,  0, 0,{ 3,{ 0, 1, 2 },{ 0, 0, 0 },{ 20, 10, 5 } } }, // After the Trader request is cancelled, the order quantity can still be obtained before it is confirmed by EXCHANGE.
		{ 7, OP_EXCHANGE_CANCEL,	 6,  DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 },  30,  0, 0,{ 3,{ 0, 1, 2 },{ 0, 0, 0 },{ 1, 2, 3 } } }, // EXCHANGE execution cancellation.
		{ 8, OP_GET_ORDER_QUANTITY,	35,  DEFAULT_UNDEF,{ 0, 0, 0, 0 },{ 10, 10 },  50,  0, 0,{ 3,{ 0, 1, 2 },{ 0, 0, 0 },{ 20, 10, 5 } } },
		//{ 6, OP_UNDEF },
		//{ 7, OP_UNDEF },
		//{ 8, OP_UNDEF },
		{ 9, OP_UNDEF }
	}
};

void localSleep(uint16_t milliseconds)
{
#ifdef _WIN32
	Sleep(milliseconds);
#elif __linux__
	usleep(milliseconds);
#endif
}

void traderSubmit(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID		= testCfg[threadIndex].defaultIdentifier;
	OrderAttribute  submitOrderAttr		= testCfg[threadIndex].defaultAttribut;
	uint16_t		pendingSeconds		= testCfg[threadIndex].pendingSecondsBeforeRun;
	uint16_t		numberOfOrder		= testCfg[threadIndex].numberOfOrder;
	uint16_t		intervalPerSubmit	= testCfg[threadIndex].intervalPerSubmit;
	uint16_t		quantity		    = testCfg[threadIndex].quantityForSubmit;

	sumbmitOrderID._market = threadIndex;
	sumbmitOrderID._trader = threadIndex;

	submitOrderAttr._quantity = quantity;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < numberOfOrder; ++i)
	{
		localSleep(intervalPerSubmit * 1000);

		sumbmitOrderID._sequence = i;

		while(!g_IOrderManager->OnTraderEnter(sumbmitOrderID,
			submitOrderAttr._price, submitOrderAttr._quantity))
		{
			std::cout << "Try to submit " << sumbmitOrderID << " FAILED! Try again!" << std::endl;
			
			localSleep(intervalPerSubmit * 1000);
		}

		std::cout << "Try to submit " << sumbmitOrderID << " OK!" << std::endl;
	}
}

void exchangeActivate(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID = testCfg[threadIndex].defaultIdentifier;
	uint16_t		pendingSeconds = testCfg[threadIndex].pendingSecondsBeforeRun;
	OperationInfo   operationInfo  = testCfg[threadIndex].operationInfo;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < operationInfo.numberOfElement; ++i)
	{
		uint16_t threadIndex		= operationInfo.operationThreadIndex[i];
		sumbmitOrderID._market		= threadIndex;
		sumbmitOrderID._trader		= threadIndex;
		uint16_t beginOrderIndex	= operationInfo.beginIndexOfOrder[i];
		uint16_t endOrderIndex		= operationInfo.endIndexOfOrder[i];

		for (auto index = beginOrderIndex; index < endOrderIndex; ++index)
		{
			sumbmitOrderID._sequence = index;

			std::string idStr("");
			const int n = 128;
			char buf[n];
			int ret = snprintf(buf, n, ID_STR_FORMAT, sumbmitOrderID._market, 
				sumbmitOrderID._desk, sumbmitOrderID._trader, sumbmitOrderID._sequence);

			idStr = std::string(buf);

			while (true)
			{
				/* */
				//if(g_orderManager.IsOrderActive(sumbmitOrderID)) 
				/* */
				if (g_IOrderManager->IsOrderActive(idStr))
				{
					break;
				}
				else
				{
					if (g_orderManager.OnExchangeNew(sumbmitOrderID, idStr))
					{
						std::cout << "Activate " << sumbmitOrderID << " OK!" << std::endl;
						break;
					};

					//g_orderManager.DumpOrder();

					localSleep(50);
				}
			}
		}
	}
}

void exchangeTrade(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID = testCfg[threadIndex].defaultIdentifier;
	uint16_t		pendingSeconds = testCfg[threadIndex].pendingSecondsBeforeRun;
	OperationInfo   operationInfo = testCfg[threadIndex].operationInfo;
	uint16_t		numberPerTrade = testCfg[threadIndex].quantityPerTradeForTrade;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < operationInfo.numberOfElement; ++i)
	{
		uint16_t localThreadIndex = operationInfo.operationThreadIndex[i];

		int times = testCfg[localThreadIndex].quantityForSubmit / numberPerTrade;
		for(int k = 0; k < times; k++)
		{
		sumbmitOrderID._market = localThreadIndex;
		sumbmitOrderID._trader = localThreadIndex;
		uint16_t beginOrderIndex = operationInfo.beginIndexOfOrder[i];
		uint16_t endOrderIndex = operationInfo.endIndexOfOrder[i];

		for (auto index = beginOrderIndex; index < endOrderIndex; ++index)
		{
			sumbmitOrderID._sequence = index;

			std::string idStr("");
			const int n = 128;
			char buf[n];
			int ret = snprintf(buf, n, ID_STR_FORMAT, sumbmitOrderID._market,
				sumbmitOrderID._desk, sumbmitOrderID._trader, sumbmitOrderID._sequence);

			idStr = std::string(buf);

			while (true)
			{
				/* */
				//if(g_orderManager.IsOrderActive(sumbmitOrderID)) 
				/* */
				if (!g_IOrderManager->IsOrderActive(idStr))
				{
					localSleep(50);
				}
				else
				{
					if (g_IOrderManager->OnExchangeTrade(idStr, numberPerTrade))
					{
						break;
					}
				}
			}
		}
		}
	}
}

void traderCancel(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID = testCfg[threadIndex].defaultIdentifier;
	uint16_t		pendingSeconds = testCfg[threadIndex].pendingSecondsBeforeRun;
	OperationInfo   operationInfo = testCfg[threadIndex].operationInfo;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < operationInfo.numberOfElement; ++i)
	{
		uint16_t localThreadIndex = operationInfo.operationThreadIndex[i];
		sumbmitOrderID._market = localThreadIndex;
		sumbmitOrderID._trader = localThreadIndex;
		uint16_t beginOrderIndex = operationInfo.beginIndexOfOrder[i];
		uint16_t endOrderIndex = operationInfo.endIndexOfOrder[i];

		for (auto index = beginOrderIndex; index < endOrderIndex; ++index)
		{
			sumbmitOrderID._sequence = index;

			std::string idStr("");
			const int n = 128;
			char buf[n];
			int ret = snprintf(buf, n, ID_STR_FORMAT, sumbmitOrderID._market,
				sumbmitOrderID._desk, sumbmitOrderID._trader, sumbmitOrderID._sequence);

			idStr = std::string(buf);

			while (true)
			{
				/* */
				//if(g_orderManager.IsOrderActive(sumbmitOrderID)) 
				/* */
				if (!g_IOrderManager->IsOrderActive(idStr))
				{
					localSleep(50);
				}
				else
				{
					if (g_IOrderManager->OnTraderCancel(sumbmitOrderID))
					{
						std::cout << "Trader cancel " << sumbmitOrderID << " OK!" << std::endl;
						break;
					}
				}
			}
		}
	}
}

void getActiveOrderQuantity(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID = testCfg[threadIndex].defaultIdentifier;
	uint16_t		pendingSeconds = testCfg[threadIndex].pendingSecondsBeforeRun;
	OperationInfo   operationInfo = testCfg[threadIndex].operationInfo;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < operationInfo.numberOfElement; ++i)
	{
		uint16_t threadIndex = operationInfo.operationThreadIndex[i];
		sumbmitOrderID._market = threadIndex;
		sumbmitOrderID._trader = threadIndex;
		uint16_t beginOrderIndex = operationInfo.beginIndexOfOrder[i];
		uint16_t endOrderIndex = operationInfo.endIndexOfOrder[i];

		for (auto index = beginOrderIndex; index < endOrderIndex; ++index)
		{
			sumbmitOrderID._sequence = index;

			std::cout << sumbmitOrderID << "=>" << g_IOrderManager->GetActiveOrderQuantity(sumbmitOrderID) << std::endl;
		}
	}
}

void exchangeCancel(uint16_t threadIndex, const TestCfg testCfg[])
{
	OrderIdentifier sumbmitOrderID = testCfg[threadIndex].defaultIdentifier;
	uint16_t		pendingSeconds = testCfg[threadIndex].pendingSecondsBeforeRun;
	OperationInfo   operationInfo = testCfg[threadIndex].operationInfo;

	localSleep(pendingSeconds * 1000);

	for (int i = 0; i < operationInfo.numberOfElement; ++i)
	{
		uint16_t threadIndex = operationInfo.operationThreadIndex[i];
		sumbmitOrderID._market = threadIndex;
		sumbmitOrderID._trader = threadIndex;
		uint16_t beginOrderIndex = operationInfo.beginIndexOfOrder[i];
		uint16_t endOrderIndex = operationInfo.endIndexOfOrder[i];

		for (auto index = beginOrderIndex; index < endOrderIndex; ++index)
		{
			sumbmitOrderID._sequence = index;

			std::string idStr("");
			const int n = 128;
			char buf[n];
			int ret = snprintf(buf, n, ID_STR_FORMAT, sumbmitOrderID._market,
				sumbmitOrderID._desk, sumbmitOrderID._trader, sumbmitOrderID._sequence);

			idStr = std::string(buf);

			while (true)
			{
				/* */
				//if(g_orderManager.IsOrderActive(sumbmitOrderID)) 
				/* */
				if (!g_IOrderManager->IsOrderActive(idStr))
				{
					localSleep(50);
				}
				else
				{
					if (g_IOrderManager->OnExchangeCancel(idStr))
					{
						std::cout << "Cancel " << sumbmitOrderID << " OK!" << std::endl;
						break;
					}
				}
			}
		}
	}
}

int main()
{
	int numberOfCase = sizeof(test_case_cfg) / sizeof(TestCfg[10]);

	for(int indexOfCase = 0; indexOfCase < numberOfCase; indexOfCase++)
	{
		int numberOfThread = sizeof(test_case_cfg[indexOfCase]) / sizeof(TestCfg);

		std::thread submit[1000];
		std::thread activate[1000];
		std::thread trade[1000];
		std::thread tCancel[1000];
		std::thread eCancel[1000];
		std::thread getOrderQuantity[1000];

		int countSubmitThread = 0;
		int countActivate = 0;
		int countTrade = 0;
		int countTraderCancel = 0;
		int countExchangeCancel = 0;
		int countGetQuantity = 0;

		for (auto i = 0; i < numberOfThread; ++i)
		{
			if (OP_TRADER_SUBMIT == test_case_cfg[indexOfCase][i].operationType)
			{
				submit[countSubmitThread] = std::thread(traderSubmit, i, test_case_cfg[indexOfCase]);
				countSubmitThread++;
			}
			else if (OP_EXCHANGE_ACTIVATE == test_case_cfg[indexOfCase][i].operationType)
			{
				activate[countActivate] = std::thread(exchangeActivate, i, test_case_cfg[indexOfCase]);
				countActivate++;
			}
			else if (OP_EXCHANGE_TRADE == test_case_cfg[indexOfCase][i].operationType)
			{
				trade[countTrade] = std::thread(exchangeTrade, i, test_case_cfg[indexOfCase]);
				countTrade++;
			}

			else if (OP_TRADER_CANCEL == test_case_cfg[indexOfCase][i].operationType)
			{
				tCancel[countTraderCancel] = std::thread(traderCancel, i, test_case_cfg[indexOfCase]);
				countTraderCancel++;
			}
			else if (OP_EXCHANGE_CANCEL == test_case_cfg[indexOfCase][i].operationType)
			{
				eCancel[countExchangeCancel] = std::thread(exchangeCancel, i, test_case_cfg[indexOfCase]);
				countExchangeCancel++;

			}
			else if (OP_GET_ORDER_QUANTITY == test_case_cfg[indexOfCase][i].operationType)
			{
				getOrderQuantity[countGetQuantity] = std::thread(getActiveOrderQuantity, i, test_case_cfg[indexOfCase]);
				countGetQuantity++;
			}

		}

		// case 01 02
		for (auto i = 0; i < countSubmitThread; ++i)
		{
			submit[i].join();
		}
		for (auto i = 0; i < countActivate; ++i)
		{
			activate[i].join();
		}
		for (auto i = 0; i < countTrade; ++i)
		{
			trade[i].join();
		}

		// case 02
		for (auto i = 0; i < countTraderCancel; ++i)
		{
			tCancel[i].join();
		}
		for (auto i = 0; i < countExchangeCancel; ++i)
		{
			eCancel[i].join();
		}
		for (auto i = 0; i < countGetQuantity; ++i)
		{
			getOrderQuantity[i].join();
		}

		g_orderManager.Dump();
        
        g_orderManager.Reset();
	}

	return 0;
}
