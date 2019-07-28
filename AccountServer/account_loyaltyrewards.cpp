#include "account_loyaltyrewards.h"
#include "account_inventory.h"
#include "AccountCatalog.h"
#include "AccountDb.hpp"
#include "AccountServer.hpp"
#include "transaction.h"
#include "account/AccountData.h"
#include "components/earray.h"
#include "network/netio.h"
#include "utils/error.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/mathutil.h"

static bool accountLoyaltyMaybePurchase(Account *pAccount, SkuId sku_id, int quantity, bool csr_did_it)
{
	assert(AccountDb_IsDataLoaded(pAccount));

	const AccountProduct *product = accountCatalogGetProduct(sku_id);
	if (!devassert(product))
		return false;

	AccountInventory *inv = AccountInventorySet_Find(&pAccount->invSet, sku_id);
	int granted_total = inv ? inv->granted_total : 0;

	if (product->grantLimit)
	{
		quantity = MIN(product->grantLimit - granted_total, quantity);
	}
	else
	{
		if (isDevelopmentMode())
			Errorf("No grant limit set for loyalty reward grant on '%.8s', undefined results will occur.", sku_id.c);
	}

	if (!quantity)
		return false;

	if (!devassert(granted_total + quantity >= 0))
		return false;

	return !orderIdIsNull(Transaction_GamePurchaseBySkuId(pAccount, sku_id, 0, 0, quantity, csr_did_it));
}

static void accountLoyaltyAdjustLevels(Account *pAccount, int previousEarned, bool csr_did_it)
{
	int i, j, size;

	for (i = 0; i < eaSize(&g_accountLoyaltyRewardTree.levels); i++)
	{
		if (!accountLoyaltyRewardHasThisLevel(previousEarned, g_accountLoyaltyRewardTree.levels[i]) &&
			accountLoyaltyRewardHasThisLevel(pAccount->loyaltyPointsEarned, g_accountLoyaltyRewardTree.levels[i]))
		{
			// newly unlocked
			LoyaltyRewardLevel *level = g_accountLoyaltyRewardTree.levels[i];

			if (level && level->storeProduct != NULL)
			{
				// apply any product codes 
				size = eaSize(&level->storeProduct);
				if (size % 2 == 0)
				{
					for (j = 0; j < size / 2; j++)
					{
						SkuId sku_id = skuIdFromString(level->storeProduct[j*2]);
						int delta = atoi(level->storeProduct[(j*2)+1]);
						if (!accountLoyaltyMaybePurchase(pAccount, sku_id, delta, csr_did_it))
						{
							LOG(LOG_ACCOUNT, LOG_LEVEL_ALERT, LOG_CONSOLE_ALWAYS,
								"{\"reason\":\"recovering from loyalty level grant failure\", \"auth_id\":%d, \"sku_id\":\"%.8s\", \"quantity\":%d}",
								pAccount->auth_id, sku_id.c, delta);
						}
					}
				}
			}
		}
	}
}

/**
* Grant a new character slot every 12 loyalty points earned since Hybrid launch
*/
static void accountLoyaltyGrantCharSlots(Account *pAccount, int previousEarned, int legacyPoints)
{
	int slotsNow = (pAccount->loyaltyPointsEarned - legacyPoints)/12;
	int slotsBefore = (MAX(previousEarned - legacyPoints, 0))/12;

	assert(slotsNow >= 0);
	
	// If the user has just received a new slot
	if (slotsNow > slotsBefore)
		devassert(!orderIdIsNull(Transaction_GamePurchaseBySkuId(pAccount, SKU("svcslots"), 0, 0, slotsNow-slotsBefore, false)));
}

void accountLoyaltyChangeNode(Account *pAccount, int index, bool set, bool csr_did_it)
{
	U8 * loyaltyBits = AccountDb_GetLoyaltyBits(pAccount);

	LoyaltyRewardNode *node = accountLoyaltyRewardFindNodeByIndex(index);
	if (!devassert(node))
		return;

	int size = eaSize(&node->storeProduct);
	
	int arrayIdx = index / 8;
	int bit = index - (arrayIdx * 8);

	// change the loyalty bit
	if (set)
	{
		loyaltyBits[arrayIdx] |= (1 << bit);
		pAccount->loyaltyPointsUnspent--;
	}
	else
	{
		loyaltyBits[arrayIdx] &= ~(1 << bit);
		pAccount->loyaltyPointsUnspent++;
	}

	// apply any product codes
	if (size % 2 == 0)
	{
		int i;
		int countDir = set ? 1 : -1;

		for (i = 0; i < size / 2; i++)
		{
			SkuId sku_id = skuIdFromString(node->storeProduct[i*2]);
			int delta = countDir*atoi(node->storeProduct[(i*2)+1]);
			if (!accountLoyaltyMaybePurchase(pAccount, sku_id, delta, csr_did_it))
			{
				LOG(LOG_ACCOUNT, LOG_LEVEL_ALERT, LOG_CONSOLE_ALWAYS,
					"{\"reason\":\"recovering from loyalty node grant failure\", \"auth_id\":%d, \"sku_id\":\"%.8s\", \"quantity\":%d}",
					pAccount->auth_id, sku_id.c, delta);
			}
		}
	}

	AccountDb_MarkUpdate(pAccount, ACCOUNTDB_UPDATE_SQL_ACCOUNT | ACCOUNTDB_UPDATE_NETWORK);
}

/**
* @param earnedDelta Reset all loyalty purchases when less than zero
* @param index Only modify earned points when less than zero
*/
bool accountLoyaltyValidateLoyaltyChange(Account *pAccount, int index, int set, int earnedDelta)
{
	if (pAccount)
	{
		// reset special case
		if (earnedDelta == INT_MIN)
			return true;

		if (earnedDelta < 0 && earnedDelta + pAccount->loyaltyPointsUnspent >= 0 )
			return true;

		if (earnedDelta + pAccount->loyaltyPointsUnspent < 0)
			return false;

		if (earnedDelta + pAccount->loyaltyPointsEarned < 0)
			return false;

		if (index < 0) 
		{
			if (earnedDelta > 0)
				return true;
			else 
				return true;
		}

		if (index < LOYALTY_BITS)
		{
			LoyaltyRewardNode *node = accountLoyaltyRewardFindNodeByIndex(index);
	
			if (!node)
				return false;

			if (set)
				return accountLoyaltyRewardCanBuyNode(&pAccount->invSet, ACCOUNT_STATUS_FLAG_IS_VIP, AccountDb_GetLoyaltyBits(pAccount), pAccount->loyaltyPointsUnspent, node);
			else
				return accountLoyaltyRewardHasNode(AccountDb_GetLoyaltyBits(pAccount), node->name);

		}
	}
	return false;
}

void accountLoyalityDecreaseLoyaltyPoints(Account *pAccount, int loyaltyEarned, bool csr_did_it)
{
	U8 * loyaltyBits = AccountDb_GetLoyaltyBits(pAccount);
	int i;
	
	for (i=0; i<LOYALTY_BITS; i++)
	{
		int arrayIdx = i / 8;
		int bit = i - (arrayIdx * 8);

		if (loyaltyBits[arrayIdx] & (1 << bit))
			accountLoyaltyChangeNode(pAccount, i, false, csr_did_it);
	}
}

void accountLoyalityIncreaseLoyaltyPoints(Account *pAccount, int loyaltyEarned, bool csr_did_it)
{
	int previousEarned = pAccount->loyaltyPointsEarned;
	assert(previousEarned >= 0);

	pAccount->loyaltyPointsEarned = loyaltyEarned;
	pAccount->loyaltyPointsUnspent += loyaltyEarned - previousEarned;

	// check for newly unlocked levels
	accountLoyaltyAdjustLevels(pAccount, previousEarned, csr_did_it);

	AccountDb_MarkUpdate(pAccount, ACCOUNTDB_UPDATE_SQL_ACCOUNT | ACCOUNTDB_UPDATE_NETWORK);
}

// ACCOUNT_CLIENT_AUTH_UPDATE
void handleAuthUpdateRequest(AccountServerShard *shard, Packet *packet_in)
{
	int auth_id = pktGetBitsAuto(packet_in);
	int payStat = pktGetBitsAuto(packet_in);
	int loyaltyEarned = pktGetBitsAuto(packet_in);
	int loyaltyLegacy = pktGetBitsAuto(packet_in);
	bool vip = pktGetBool(packet_in);

	// setting minimum for loyalty points
	if (loyaltyEarned < 1)
		loyaltyEarned = 1;
	if (loyaltyLegacy < 1)
		loyaltyLegacy = 1;

	Account *pAccount = AccountDb_GetAccount(auth_id);
	if (!devassert(pAccount))
		return;

	if (!AccountDb_IsDataLoaded(pAccount))
	{
		// try to queue this command for later
		if (AccountServer_DelayPacketUntilLater(shard, packet_in))
			return;
	}

	// update earned points - but don't roll back
	if (!devassert(loyaltyEarned >= loyaltyLegacy))
		loyaltyLegacy = loyaltyEarned;

	if (loyaltyEarned > pAccount->loyaltyPointsEarned)
	{
		int previousEarned = pAccount->loyaltyPointsEarned;
		assert(previousEarned >= 0);

		accountLoyalityIncreaseLoyaltyPoints(pAccount, loyaltyEarned, false);
		accountLoyaltyGrantCharSlots(pAccount, previousEarned, loyaltyLegacy);
	}

	// need to pre-spend appropriate legacy
	{
		int howMany = MIN(loyaltyLegacy - ((loyaltyLegacy - 2) / 5), 34);
		int actualSpent = accountLoyaltyRewardGetNodesBought(AccountDb_GetLoyaltyBits(pAccount));
		U8	*loyaltyBits = AccountDb_GetLoyaltyBits(pAccount);

		for (int i = 0; i < howMany; i++)
		{
			if (!accountLoyaltyRewardHasThisNode(loyaltyBits, accountLoyaltyRewardFindNodeByIndex(i)))
				accountLoyaltyChangeNode(pAccount, i, true, false);
		}
	}

	// provide free monthly server transfer for VIPs
	if (vip)
	{
		SYSTEMTIME time;
		GetSystemTime(&time);
		U32 monthsSinceZero = (time.wYear * 12U) + (time.wMonth - 1);
		if (monthsSinceZero > pAccount->freeXferMonths)
		{
			devassert(!orderIdIsNull(Transaction_GamePurchaseBySkuId(pAccount, SKU("svsvtran"), 0, 0, 1, false)));

			pAccount->freeXferMonths = monthsSinceZero;
			AccountDb_MarkUpdate(pAccount, ACCOUNTDB_UPDATE_SQL_ACCOUNT);
		}
	}
}

// ACCOUNT_CLIENT_LOYALTY_CHANGE
void handleLoyaltyChangeRequest(AccountServerShard *shard, Packet *packet_in)
{
	int auth_id;
	int index, earnedDelta;
	bool set;
	Account *pAccount;

	auth_id = pktGetBitsAuto(packet_in);
	index = pktGetBitsAuto(packet_in);
	set = pktGetBool(packet_in);
	earnedDelta = pktGetBitsAuto(packet_in);

	pAccount = AccountDb_GetAccount(auth_id);
	if (!devassert(pAccount))
		return;
	if (!devassert(pAccount->shard == shard))
		return;

	if (accountLoyaltyValidateLoyaltyChange(pAccount, index, set, earnedDelta))
	{
		// change the loyalty points
		if (earnedDelta < 0)
			accountLoyalityDecreaseLoyaltyPoints(pAccount, pAccount->loyaltyPointsEarned + earnedDelta, false);
		if (earnedDelta > 0)
			accountLoyalityIncreaseLoyaltyPoints(pAccount, pAccount->loyaltyPointsEarned + earnedDelta, false);
		if (earnedDelta != 0)
			AccountDb_MarkUpdate(pAccount, ACCOUNTDB_UPDATE_SQL_ACCOUNT | ACCOUNTDB_UPDATE_NETWORK);

		// change the loyalty bit
		if (index >= 0)
			accountLoyaltyChangeNode(pAccount, index, set, false);
	}
}
