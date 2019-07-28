/***************************************************************************
 *     Copyright (c) 2005-2006, Cryptic Studios
 *     All Rights Reserved
 *     Confidential Property of Cryptic Studios
 ***************************************************************************/
#include "earray.h"
#include "mathutil.h"

#include "gfxwindow.h"

#include "input.h"
#include "cmdgame.h"         // for game_state
#include "uiBaseInput.h"

#include "bases.h"
#include "baseclientsend.h"
#include "basedata.h"
#include "baseinv.h"
#include "baseedit.h"
#include "basesend.h"

#include "entity.h"
#include "player.h"
#include "uiNet.h"
#include "timing.h"
#include "uiDialog.h"
#include "sprite_text.h"
#include "ArrayOld.h"
#include "MessageStore.h"
#include "language/langClientUtil.h"
#include "MessageStoreUtil.h"

// Used to restore the original rotation of base items.
const Vec3 resetvec3 = {0, 0, 1};

/**********************************************************************func*
 * baseedit_Mode
 *
 */
int baseedit_Mode(void)
{
	return game_state.edit_base;
}

/**********************************************************************func*
 * baseedit_SetCurDetail
 *
 */
void baseedit_SetCurDetail(BaseRoom *pRoom, RoomDetail *pDetail)
{
	baseedit_ClearDetailAlpha(pDetail);

	g_refCurDetail.refRoom.p = pRoom;
 	g_refCurDetail.refRoom.id = (pRoom ? pRoom->id : 0);

	g_refCurDetail.p = pDetail;
	g_refCurDetail.id = (pDetail ? pDetail->id : 0);

	if( pRoom )
		baseedit_SetCurRoom( pRoom, NULL );
}

/**********************************************************************func*
 * baseedit_SetHoverDetail
 *
 */
void baseedit_SetHoverDetail(BaseRoom *pRoom, RoomDetail *pDetail)
{
	g_refHoverDetail.refRoom.p = pRoom;
	g_refHoverDetail.refRoom.id = (pRoom ? pRoom->id : 0);

	g_refHoverDetail.p = pDetail;
	g_refHoverDetail.id = (pDetail ? pDetail->id : 0);
}

/**********************************************************************func*
 * baseedit_SyncUIState
 *
 */
void baseedit_SyncUIState(void)
{
	if(g_refCurRoom.p)
	{
		g_refCurRoom.p = baseGetRoom(&g_base, g_refCurRoom.id);
		if(!g_refCurRoom.p)
		{
			memset(&g_refCurRoom, 0, sizeof(BaseRoomRef));
			memset(&g_refCurBlock, 0, sizeof(BaseBlockRef));
		}
		else
			g_refCurBlock.p = roomGetBlock(g_refCurRoom.p, g_refCurBlock.block);
	}
	else
	{
		memset(&g_refCurDetail, 0, sizeof(RoomDetailRef));
		memset(&g_refCurBlock, 0, sizeof(BaseBlockRef));
	}

	if(g_refCurDetail.p)
	{
		g_refCurDetail.p = NULL;

		g_refCurDetail.refRoom.p = baseGetRoom(&g_base, g_refCurDetail.refRoom.id);
		if(g_refCurDetail.refRoom.p)
		{
			g_refCurDetail.p = roomGetDetail(g_refCurDetail.refRoom.p, g_refCurDetail.id);
		}

		if(!g_refCurDetail.p)
			memset(&g_refCurDetail, 0, sizeof(RoomDetailRef));
	}

	if(g_refHoverDetail.p)
	{
		g_refHoverDetail.p = NULL;

		g_refHoverDetail.refRoom.p = baseGetRoom(&g_base, g_refHoverDetail.refRoom.id);
		if(g_refHoverDetail.refRoom.p)
		{
			g_refHoverDetail.p = roomGetDetail(g_refHoverDetail.refRoom.p, g_refHoverDetail.id);
		}

		if(!g_refHoverDetail.p)
			memset(&g_refHoverDetail, 0, sizeof(RoomDetailRef));
	}
}



/**********************************************************************func*
 * baseedit_CopyMatFromBlock
 *
 */
void baseedit_CopyMatFromBlock(BaseRoom *pRoom, int block[2], int iSurface, Mat4 mat)
{
	BaseBlock *pBlock;
	F32 height[2];

	if(!pRoom)
		return;
	pBlock = roomGetBlock(pRoom, block);
	if(!pBlock)
	{
		roomIsDoorway(pRoom, block, height);
	}
	else
	{
		height[iSurface] = pBlock->height[iSurface];
	}

	copyMat4(unitmat, mat);

	mat[3][0] = pRoom->pos[0] + block[0] * pRoom->blockSize + pRoom->blockSize/2;
	mat[3][1] = pRoom->pos[1] + height[iSurface];
	mat[3][2] = pRoom->pos[2] + block[1] * pRoom->blockSize + pRoom->blockSize/2;

	if(iSurface)
	{
		rollMat3(RAD(180), mat);
	}
}

/**********************************************************************func*
 * baseedit_SetRoomTexture
 *
 */
void baseedit_SetRoomTexture(BaseRoom *pRoom, int partIdx, char *pchTexture)
{
	baseClientSendTexSwap(pRoom, partIdx, pchTexture);
}

/**********************************************************************func*
 * baseedit_SetRoomGeometry
 *
 */
void baseedit_SetRoomGeometry(BaseRoom *pRoom, int partIdx, char *pchGeometry)
{
	baseClientSendGeoSwap(pRoom, partIdx, pchGeometry);
}

void baseedit_SetRoomTint( BaseRoom *pRoom, int partIdx, int c1, int c2 )
{
	Color colors[2];

	colors[0].integer = c1;
	colors[1].integer = c2;

	baseClientSendTint( pRoom, partIdx, colors );
}

/**********************************************************************func*
 * baseedit_SetRoomLight
 *
 */
void baseedit_SetRoomLight(BaseRoom *pRoom, int which, Vec3 color)
{
	Color byte_color;
	int i,c;

	for(i=0;i<3;i++)
	{
		c = 63 * color[i];
		if(c > 255)
			c = 255;
		if(c < 0)
			c = 0;
		byte_color.rgba[i] = c;
	}
	baseClientSendLight(pRoom, which, byte_color);
}

/**********************************************************************func*
 * baseedit_CreateDetail
 *
 */
void baseedit_CreateDetail(BaseRoom *pRoom, const Detail *pDetail, Mat4 mat, Mat4 editor_mat, Mat4 surface_mat, int invID, AttachType attach)
{
	RoomDetail detail = {0};

	detail.info = pDetail;
	detail.invID = invID;
	detail.eAttach = attach;
	copyMat4(mat, detail.mat);
	copyMat4(editor_mat, detail.editor_mat);
	copyMat4(surface_mat, detail.surface_mat);
	
	baseClientSendDetailCreate(pRoom, &detail);
}

/**********************************************************************func*
 * baseedit_DeleteDetail
 *
 */
static RoomDetail * pPendingDetailDelete;
static BaseRoom * pPendingDetailDeleteRoom;

void baseDetailDelete(void *data)
{
 	if( baseedit_Mode() != kBaseEdit_Architect )
		return;

	if(!pPendingDetailDelete||!pPendingDetailDeleteRoom)
		return;

	if(g_refCurDetail.p == pPendingDetailDelete)
		memset(&g_refCurDetail, 0, sizeof(g_refCurDetail));

	if( !pPendingDetailDelete->id ) // tried to delete while placing item
	{
		baseedit_CancelDrags();
		baseedit_CancelSelects();
		return;
	}

	baseClientSendDetailDelete(pPendingDetailDeleteRoom, pPendingDetailDelete);
}

void baseedit_DeleteDetail(BaseRoom *pRoom, RoomDetail *pDetail)
{
	if( stricmp("Entrance", pDetail->info->pchCategory)==0 )
		return;

	if( dialogInQueue( NULL, baseDetailDelete, NULL ) )
		return;

	if( eaSize(&pDetail->storedEnhancement) || 
		eaSize(&pDetail->storedInspiration) ||
		eaSize(&pDetail->storedSalvage) )
	{
		dialogStd( DIALOG_OK, textStd("CantDeleteStoredItemsName", pDetail->info->pchDisplayName), NULL, NULL, NULL, NULL, 0 );
		return;
	}

	pPendingDetailDelete = pDetail;
	pPendingDetailDeleteRoom = pRoom;



	if( pDetail->info->bObsolete )
		dialogStd( DIALOG_ACCEPT_CANCEL, textStd("ObsoleteItemDeleteWarning"), NULL, NULL, baseDetailDelete, NULL, 0 );
	else if( pDetail->creator_name[0] )
		dialogStd( DIALOG_ACCEPT_CANCEL, textStd("PersonalItemDeleteWarning"), NULL, NULL, baseDetailDelete, NULL, 0 );
	else if( pDetail->info->bWarnDelete)
		dialogStd( DIALOG_ACCEPT_CANCEL, textStd("GenericItemDeleteWarning"), NULL, NULL, baseDetailDelete, NULL, 0 );
	else
		baseDetailDelete(0);
}

/**********************************************************************func*
* baseedit_DeleteDetail
*
*/
void baseedit_DeleteDoorway( BaseRoom * pRoom, int block[2])
{
	if( block[0] < 0 || block[0] >= pRoom->size[0] ||
		block[1] < 0 || block[1] >= pRoom->size[1] )
	{
		baseClientSendDoorwayDelete(pRoom, g_refCurBlock.block);
		if( block[0] == g_refCurBlock.block[0] &&
			block[1] == g_refCurBlock.block[1] )
		{
			memset(&g_refCurBlock, 0, sizeof(BaseBlockRef));
		}
	}
}

/**********************************************************************func*
 * baseedit_MoveDetail
 *
 */
void baseedit_MoveDetail(BaseRoom *pRoom, RoomDetail *pDetail, Mat4 mat, Mat4 editor_mat, Mat4 surface_mat)
{
	RoomDetail detail = *pDetail;

	copyMat4(mat, detail.mat);
	copyMat4(editor_mat, detail.editor_mat);
	copyMat4(surface_mat, detail.surface_mat);

	baseClientSendDetailUpdate(pRoom, &detail, NULL);
}

void baseedit_MoveDetailToRoom(BaseRoom *pRoom, RoomDetail *pDetail, BaseRoom *pNewRoom, Mat4 mat, Mat4 editor_mat, Mat4 surface_mat)
{
	RoomDetail detail = *pDetail;

	copyMat4(mat, detail.mat);
	copyMat4(editor_mat, detail.editor_mat);
	copyMat4(surface_mat, detail.surface_mat);

	baseClientSendDetailUpdate(pRoom, &detail,pNewRoom);
}

/**********************************************************************func*
* baseedit_RotateDetail
*
*/
void baseedit_RotateDetail(BaseRoom *pRoom, RoomDetail *pDetail, int clockwise)
{
	if(baseEdit_state.detail_dragging && g_refCurDetail.id == pDetail->id)
	{
		baseedit_RotateDraggedDetail(clockwise);
	}
	else if(pDetail->id)
	{
		RoomDetail detail = *pDetail;
		if (ctrlKeyState) {
			// Reset rotation on Ctrl+RightClick.
			orientMat3(detail.editor_mat, resetvec3);
		} else {
			if( clockwise ) {
				yawMat3(PI/2, detail.editor_mat);
			} else {
				yawMat3(-PI/2, detail.editor_mat);
			}
		}
		if( !detailCanFitEditor(g_refCurRoom.p, &detail, detail.editor_mat, detail.surface_mat, baseEdit_state.gridsnap, baseEdit_state.roomclip && !DetailHasFlag(detail.info, NoClip)) )
		{
			FindReasonableSpot(g_refCurRoom.p, &detail, detail.editor_mat, detail.surface_mat, NULL, detail.editor_mat[3], false);
		}
		baseDetailApplyEditorTransform(detail.mat, &detail, detail.editor_mat, detail.surface_mat);
		baseClientSendDetailUpdate(pRoom, &detail,0);
	}
}

/**********************************************************************func*
* baseedit_SetDetailTint
*
*/
void baseedit_SetDetailTint( BaseRoom *pRoom, RoomDetail *pDetail, int c1, int c2 )
{
	Color colors[2];

	colors[0].integer = c1;
	colors[1].integer = c2;

	baseClientSendDetailTint( pRoom, pDetail, colors );
}

/**********************************************************************func*
 * SetPermissions
 *
 */
void baseedit_SetDetailPermissions(BaseRoom *pRoom, RoomDetail *pDetail)
{
	RoomDetail detail = *pDetail;

	if (pDetail && pDetail->info && pDetail->info->bIsStorage)
	{
		baseClientSendPermissionUpdate(pRoom, &detail);
	}
}

/**********************************************************************func*
 * BlockHeights
 *
 */
void baseedit_SetBlockHeights(BaseRoom *pRoom, int block[2], int height[2])
{
	float float_ht[2];
	Entity * e = playerPtr();
	float_ht[0] = height[0];
	float_ht[1] = height[1];

	if( roomIsDoorway(pRoom, block, NULL ) )
	{
		baseedit_CreateDoorway(pRoom, block, float_ht);
	}
	else
	{
		if(height[1]<height[0] )
			baseClientSendHeight(pRoom, kBaseSend_Single, block[0] + block[1]*pRoom->size[0], 0.0f, 0.0f);
		else
			baseClientSendHeight(pRoom, kBaseSend_Single, block[0] + block[1]*pRoom->size[0], height[0], height[1]);
	}
}

void baseedit_SetRoomHeights(BaseRoom *pRoom,int height[2])
{
	if(height[1]>height[0] )
		baseClientSendHeight(pRoom, kBaseSend_Room, 0, height[0], height[1]);
}

void baseedit_SetBaseHeights(int height[2])
{
	if(height[1]>height[0] )
		baseClientSendHeight(NULL, kBaseSend_Base, 0, height[0], height[1]);
}

/**********************************************************************func*
 * baseedit_InDragMode
 *
 */
bool baseedit_InDragMode(void)
{
	if(baseEdit_state.dragging
		|| baseEdit_state.detail_dragging
		|| baseEdit_state.surface_dragging
		|| baseEdit_state.room_dragging
		|| baseEdit_state.door_dragging
		|| baseEdit_state.plot_dragging)
	{
		return true;
	}

	return false;
}


/**********************************************************************func*
 * baseedit_CancelSelects
 *
 */
bool baseedit_CancelSelects(void)
{
	bool bRet;
	F32 elapsed = timerElapsed(g_iDecorFadeTimer);

	baseedit_ClearDetailAlpha(NULL);

	if(g_refCurDetail.p || elapsed < DECOR_FADE_TOTAL);
		bRet = true;

	timerAdd(g_iDecorFadeTimer, DECOR_FADE_TOTAL);
	memset(&g_refCurDetail, 0, sizeof(g_refCurDetail));

	return bRet;
}

/**********************************************************************func*
 * baseedit_DeleteRoom
 *
 */
void baseedit_DeleteRoom(BaseRoom *pRoom)
{
	int block[2];
	BaseRoom *pPosRoom = baseRoomLocFromPos(&g_base, ENTPOS(playerPtr()), block);

	if(!pRoom)
		return;

	if(pRoom == g_refCurRoom.p)
	{
		if(eaSize(&g_base.rooms)>1)
			baseedit_SetCurRoom(g_base.rooms[0], NULL);
	}

	baseClientSendRoomDelete(pRoom->id);
}

/**********************************************************************func*
 * baseedit_CreateRoom
 *
 */
void baseedit_CreateRoom(const RoomTemplate *pinfo, int size[2], Vec3 pos, F32 afHeights[2], RoomDoor * doors, int door_count )
{
	baseClientSendRoomCreate(size, pos, afHeights, pinfo, doors, door_count);
}

/**********************************************************************func*
* baseedit_MoveRoom
*
*/
void baseedit_MoveRoom(BaseRoom *room, int rotation, Vec3 pos, RoomDoor * doors, int door_count )
{
	baseClientSendRoomMove(room, rotation, pos, doors, door_count);
}

/**********************************************************************func*
 * baseClientSendDoorway
 *
 */
void baseedit_CreateDoorway(BaseRoom *pRoom, int block[2], F32 afHeights[2])
{
	baseClientSendDoorway(pRoom, block, afHeights);
}


static int printHelper(	MessageStore* store,
						char* outputBuffer,
						int bufferLength,
						const char* messageID,
						Array* messageTypeDef, ...) 
{
	int l;
	va_list va;
	va_start(va, messageTypeDef);
	l = msvaPrintfInternal(store, outputBuffer, bufferLength, messageID, messageTypeDef, NULL, va);
	va_end(va);
	return l;
}

// Parse and replace templates in a detail description string
char *detailGetDescription(const Detail *detail,int longDesc) {
	static char result[2048]; //Warning! watch for overflow
	const char *source;
	static Array *Message = NULL;
	int messageLength;
	int percent = 0;
	const char *param = "";

	if (longDesc && detail->pchDisplayHelp)
		source = detail->pchDisplayHelp;
	else if (!longDesc && detail->pchDisplayShortHelp)
		source = detail->pchDisplayShortHelp;
	else
		return NULL;

	if (!Message)
		Message = msCompileMessageType("{Control, %d}, {Energy, %d}, {Lifetime, %d}, {RepairChance, %d}, {FunctionParam, %s}, {FunctionPercent, %d");

	if (detail->ppchFunctionParams && detail->ppchFunctionParams[0])
	{
		param = detail->ppchFunctionParams[0];
		percent = (int)(atof(detail->ppchFunctionParams[0])*100);
	}

	messageLength = printHelper(menuMessages,
								result,
								ARRAY_SIZE(result),
								source,
								Message,
								detail->iControlProduce+detail->iControlConsume,
								detail->iEnergyProduce+detail->iEnergyConsume,
								detail->iLifetime, (int)(detail->fRepairChance*100),
								param,
								percent);

	if(!messageLength)
		return NULL;
	return result;
}

/* End of File */
