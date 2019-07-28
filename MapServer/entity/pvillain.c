/***************************************************************************
 *     Copyright (c) 2003-2006, Cryptic Studios
 *     All Rights Reserved
 *     Confidential Property of Cryptic Studios
 ***************************************************************************/
#include "earray.h"

#include "Entity.h"
#include "VillainDef.h"
#include "character_base.h"
#include "powers.h"
#include "PowerInfo.h"
#include "entserver.h"
#include "entPlayer.h"

/**********************************************************************func*
 * BecomeVillain
 *
 */
void BecomeVillain(Entity *e, char *pchVillain, int iLevel)
{
	// OK, This is a hack function right now. It just nukes whatever
	// entity it's on and makes it a villain.


	const VillainDef* def = villainFindByName(pchVillain);

	// OK, Do that voodoo.
	if(def)
	{
		char pchName[MAX_NAME_LEN];

		pchName[0]=0;
		if(e->name && strlen(e->name)>0)
		{
			strcpy(pchName, e->name);
		}

		if(villainCreateInPlace(e, def, iLevel, NULL, false, NULL, NULL, 0, NULL))
		{
			// Resend all the powers.
			eaPush(&e->powerDefChange, (void *)-1);
			
			// set ent flag
			if( e->pl )
			{
				e->pl->isBeingCritter = true;
			}
		}

		if(pchName[0])
		{
			strcpy(e->name, pchName);
		}
	}
}

/* End of File */
