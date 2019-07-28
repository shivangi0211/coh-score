/***************************************************************************
 *     Copyright (c) 2000-2006, Cryptic Studios
 *     All Rights Reserved
 *     Confidential Property of Cryptic Studios
 ***************************************************************************/
#include "net_packet.h"
#include "entVarUpdate.h"
#include "wdwbase.h"
#include <assert.h>
#include "error.h"
#include "earray.h"

#if CLIENT
	#include "clientcomm.h"
#endif

Wdw winDefs[MAX_WINDOW_COUNT+MAX_CUSTOM_WINDOW_COUNT] = {0};


void sendWindow(Packet *pak, WdwBase *pwdw, int idx)
{
	pktSendBitsPack(pak, 1, idx);
	pktSendBitsPack(pak, 1, pwdw->xp);
	pktSendBitsPack(pak, 1, pwdw->yp);

	// special windows like the quit window should not be saved open
	if( pwdw->start_shrunk == ALWAYS_CLOSED )
		pktSendBitsPack(pak, 1, WINDOW_DOCKED );
	else 
		pktSendBitsPack(pak, 1, pwdw->mode);

	pktSendBitsPack(pak, 1, pwdw->locked);
	pktSendBitsPack(pak, 1, pwdw->color);
	pktSendBitsPack(pak, 1, pwdw->back_color);
	pktSendBits(pak, 1, pwdw->maximized);

	if( pwdw->draggable_frame || idx == WDW_CHAT_BOX)	// minimized primary chat window is undraggable but needs its height/width info
	{
		pktSendBits( pak, 1, 1 );
		pktSendBitsPack(pak, 1, pwdw->wd);
		pktSendBitsPack(pak, 1, pwdw->ht);
	}
	else
		pktSendBits( pak, 1, 0 );

	pktSendIfSetF32( pak, pwdw->sc );
}

#if CLIENT
void sendWindowToServer(WdwBase *pwdw, int idx, int save_window)
{
	START_INPUT_PACKET(pak, CLIENTINP_WINDOW);
	if( save_window )
	{
		pktSendBits(pak,1,0);
		sendWindow(pak, pwdw, idx);
	}
	else
	{
		pktSendBits(pak,1,1);
		pktSendBitsPack(pak, 1, idx);
	}
	END_INPUT_PACKET
}
#endif

//
//
//

void receiveWindowIdx(Packet *pak, int *idx)
{
	*idx = pktGetBitsPack(pak,1);
}

void receiveWindow(Packet *pak, WdwBase *pwdw)
{
	float sc;

	pwdw->xp			= pktGetBitsPack(pak, 1);
	pwdw->yp			= pktGetBitsPack(pak, 1);
	pwdw->mode			= pktGetBitsPack(pak, 1);
	pwdw->locked		= pktGetBitsPack(pak, 1);
	pwdw->color			= pktGetBitsPack(pak, 1);
	pwdw->back_color	= pktGetBitsPack(pak, 1);
	pwdw->maximized		= pktGetBits(pak,1);

	if( pktGetBits( pak, 1 ) )
	{
		pwdw->draggable_frame = TRUE;
		pwdw->wd   = pktGetBitsPack(pak, 1);
		pwdw->ht   = pktGetBitsPack(pak, 1);
	}
	else
		pwdw->draggable_frame = FALSE;

	if( (sc=pktGetIfSetF32(pak)) != 0 ) 
		pwdw->sc = sc;
	else
		pwdw->sc = 1.f;
}

Wdw* wdwCreate(WindowName id, int custom)
{
	if( id >= MAX_WINDOW_COUNT && !custom )
		FatalErrorf( "Tried to create window with invalid ID: %i (max = %i)", id, MAX_WINDOW_COUNT );

	winDefs[id].id = id;
	return &winDefs[id];
}

Wdw* wdwGetWindow(WindowName id)
{
	return &winDefs[id];
}
/* End of File */
