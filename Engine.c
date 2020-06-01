
/* Amiga 500 Game Engine with direct hardware access by Balrog Soft */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <exec/memory.h>  
#include <exec/execbase.h>
#include <hardware/cia.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>

#include <devices/input.h>

#include "custom_defines.h"

#include "ptplayer.h"

#define GETLONG(var) (*(volatile ULONG*)&var)

#define MOD_SIZE    8340
#define PLAYER_SIZE 3072 
#define TILES_SIZE  368648
#define MAP_SIZE    320

#define BPL0 (25 * 2) + 1
#define BPL1 (27 * 2) + 1
#define BPL2 (29 * 2) + 1
#define BPL3 (31 * 2) + 1

#define TILES   144
#define TILE_BITS 4

#define LINEBLOCKS 16
#define WBLOCK 16
#define BITMAPLINEBYTES 40
#define BITMAPLINEBYTESI 160
#define BITMAPLINEBYTESB 38

#define WIDTH 320
#define HEIGHT 256

#define UP     1 
#define DOWN   2
#define LEFT   4
#define RIGHT  8
#define FIRE1 16
#define FIRE2 32

#define WLK_TILES 16
#define OVER_TILES 5

#define SPRITES_NB 11

#define POTGOR *(UWORD *)0xDFF016    

#define INLINE_FUNCTIONS 1

extern struct ExecBase *SysBase;
struct CIA *cia = (struct CIA *) 0xBFE001;
struct Custom *custom = (struct Custom *)0xDFF000;

struct GfxBase *GfxBase;

struct MsgPort *inputMP;
struct IOStdReq *inputReq;
struct Interrupt inputHandler;
BYTE inputDevice = -1;

struct Task *task = NULL;

UBYTE NameString[]="InputHandlerGame";

static struct Window *winPtr;
static struct Process *process;

static ULONG vbCounter = 0;
struct Interrupt *vbInt = NULL;

UWORD GetVBR[] = {0x4e7a, 0x0801, 0x4e73}; // MOVEC.L VBR,D0 RTE
    
UWORD wbytes[HEIGHT];

typedef struct {
    UBYTE *data;
    WORD  width;
    WORD  height;
    WORD  wbytes;
    UBYTE depth;
} Bitmap;

typedef struct {
    UBYTE *rest_bm;

    UWORD wbytes[HEIGHT];
    UWORD bltsize;
    
    WORD dx;
    WORD dy;
    
    WORD x;
    WORD y;
    
    WORD fx[2];
    WORD fy[2];
    WORD width;
    WORD height;
    
    UBYTE dirty;
    UBYTE dir;
    UBYTE move;
    UBYTE anim;
    UBYTE frame;
    
    UWORD origTile;
    UWORD destTile;
    
    ULONG rest_bpl[2];
    
    Bitmap *bm;
    
    void *next;
} Sprite;


typedef struct {
    Bitmap* map;
    UBYTE *tilemap;
    UBYTE twidth;
    UBYTE theight;
    UBYTE tdepth;
    UWORD mwidth;
    UWORD mheight;
    UBYTE *offset[TILES];
    UBYTE dirty[MAP_SIZE];
} TileMap;

Sprite *spriteListHead;

UWORD chip cop1[] =
{
    FMODE, 0,
    
    BPLCON0, 0x4200,
    BPLCON1, 0,

    DIWSTRT, 0x2C81,
    DIWSTOP, 0x2CC1,
 
    DDFSTRT, 0x0038, 
    DDFSTOP, 0x00d0,

    BPL1MOD, 0x78,
    BPL2MOD, 0x78,

    SPR0PTH, 0,
    SPR0PTL, 0,
        
    SPR1PTH, 0,
    SPR1PTL, 0,
    
    SPR2PTH, 0,
    SPR2PTL, 0,
        
    SPR3PTH, 0,
    SPR3PTL, 0,
        
    SPR4PTH, 0,
    SPR4PTL, 0,
        
    SPR5PTH, 0,
    SPR5PTL, 0,
        
    SPR6PTH, 0,
    SPR6PTL, 0,
        
    SPR7PTH, 0,
    SPR7PTL, 0,

    BPL1PTH, 0,
    BPL1PTL, 0,

    BPL2PTH, 0,
    BPL2PTL, 0,

    BPL3PTH, 0,
    BPL3PTL, 0,

    BPL4PTH, 0,
    BPL4PTL, 0,

    COLOR00, 0x0000,
    COLOR01, 0x0323,
    COLOR02, 0x0354,
    COLOR03, 0x0834,
    COLOR04, 0x0374,
    COLOR05, 0x0457,
    COLOR06, 0x0859,
    COLOR07, 0x05a4,
    COLOR08, 0x0b54,
    COLOR09, 0x048b,
    COLOR10, 0x0698,
    COLOR11, 0x09b9,
    COLOR12, 0x0d85,
    COLOR13, 0x06bb,
    COLOR14, 0x0efe,
    COLOR15, 0x0ed5,

    0xffff, 0xfffe,
    0xffff, 0xfffe
};

UWORD chip cop2[] =
{
    FMODE, 0,
    
    BPLCON0, 0x4200,
    BPLCON1, 0,

    DIWSTRT, 0x2C81,
    DIWSTOP, 0x2CC1,
 
    DDFSTRT, 0x0038, 
    DDFSTOP, 0x00d0,

    BPL1MOD, 0x78,
    BPL2MOD, 0x78,

    SPR0PTH, 0,
    SPR0PTL, 0,
        
    SPR1PTH, 0,
    SPR1PTL, 0,
        
    SPR2PTH, 0,
    SPR2PTL, 0,
        
    SPR3PTH, 0,
    SPR3PTL, 0,
        
    SPR4PTH, 0,
    SPR4PTL, 0,
        
    SPR5PTH, 0,
    SPR5PTL, 0,
        
    SPR6PTH, 0,
    SPR6PTL, 0,
        
    SPR7PTH, 0,
    SPR7PTL, 0,

    BPL1PTH, 0,
    BPL1PTL, 0,

    BPL2PTH, 0,
    BPL2PTL, 0,

    BPL3PTH, 0,
    BPL3PTL, 0,

    BPL4PTH, 0,
    BPL4PTL, 0,

    COLOR00, 0x0000,
    COLOR01, 0x0323,
    COLOR02, 0x0354,
    COLOR03, 0x0834,
    COLOR04, 0x0374,
    COLOR05, 0x0457,
    COLOR06, 0x0859,
    COLOR07, 0x05a4,
    COLOR08, 0x0b54,
    COLOR09, 0x048b,
    COLOR10, 0x0698,
    COLOR11, 0x09b9,
    COLOR12, 0x0d85,
    COLOR13, 0x06bb,
    COLOR14, 0x0efe,
    COLOR15, 0x0ed5,

    0xffff, 0xfffe,
    0xffff, 0xfffe
};
    
ULONG random_v = 0x9c33fe02;

#define WaitBlitter() while (GETLONG(custom->dmaconr) & DMAF_BLTDONE){}

static LONG __interrupt __saveds NullInputHandler(void)
{
	return 0;
}

ULONG random(void)
{
    random_v = (random_v * (ULONG)2654435761);
    return random_v>>16;
}

#ifdef INLINE_FUNCTIONS

#define waitVBlank() \
  do { \
    vposr = GETLONG(custom->vposr); \
  } while ((vposr & 0x1FF00) >= 0x12F00); \
  do { \
    vposr = GETLONG(custom->vposr); \
  } while ((vposr & 0x1FF00) < 0x12F00); \
  
#define tm_drawTile(tm, dst, tx, ty, tl) \
    WaitBlitter() \
    custom->bltcon0 = 0xFCA; \
    custom->bltcon1 = 0; \
    custom->bltafwm = 0xFFFF; \
    custom->bltalwm = 0xFFFF; \
    custom->bltamod = 62; \
    custom->bltbmod = 62; \
    custom->bltcmod = BITMAPLINEBYTESB; \
    custom->bltdmod = BITMAPLINEBYTESB; \
    custom->bltapt  = tm->offset[tl] + 32; \
    custom->bltbpt  = tm->offset[tl]; \
    custom->bltcpt  = dst + ((tx >> 3) & 0xFFFE) + wbytes[ty]; \
    custom->bltdpt  = custom->bltcpt; \
    custom->bltsize = 4097; 

#define sp_initSpritePos(spr, sx, sy) \
    spr->x = spr->fx[0] = spr->fx[1] = sx; \
    spr->y = spr->fy[0] = spr->fy[1] = sy; \
    spr->dirty = 2; \
    spr->move = FALSE;
    
#define sp_updateSpritePos(spr, tm, anm, frm) \
    if ((spr->x & 15) == 0 && (spr->y & 15) == 0) { \
        if (spr->move == FALSE && (spr->dx != 0 || spr->dy != 0)) { \
            tx1 = spr->x >> 4; \
            ty1 = spr->y >> 4; \
            spr->origTile = modu[tx1&31] + mapy[ty1]; \
            tx2 = tx1 + spr->dx; \
            ty2 = ty1 + spr->dy; \
            spr->destTile = modu[tx2&31] + mapy[ty2]; \
            for (j = 0; j < WLK_TILES; j++) { \
                if (walk_tiles[j] == tm->tilemap[spr->destTile]) { \
                    spr->move = TRUE; \
                    break; \
                } \
            } \
            spr_chk = spriteListHead; \
            while (spr_chk != NULL) { \
            if (spr != spr_chk && spr_chk->dirty > 0) { \
                if (spr_chk->origTile == spr->origTile || \
                    spr_chk->destTile == spr->origTile) { \
                    spr->dirty = 2; \
                    spr_chk->dirty = 2; \
                    for (j = 0; j < OVER_TILES; j++) {  \
                        if (tileMap->tilemap[spr->origTile] == overlay_tiles[j]) {  \
                            tileMap->dirty[spr->origTile] = 2;  \
                        }  \
                    }  \
                } \
            } \
            spr_chk = spr_chk->next; \
        } \
            if (spr->move == FALSE) { \
                spr->dx = 0; \
                spr->dy = 0; \
            } \
        } \
    } \
    if (spr->move == TRUE || spr->anim != anm || spr->frame != frm) { \
        spr->x += spr->dx; \
        spr->y += spr->dy; \
        for (j = 0; j < OVER_TILES; j++) { \
            if (tm->tilemap[spr->origTile] == overlay_tiles[j]) { \
                tm->dirty[spr->origTile] = 2; \
            } \
            if (tm->tilemap[spr->destTile] == overlay_tiles[j]) { \
                tm->dirty[spr->destTile] = 2; \
            } \
        } \
        spr->dirty = 2; \
        if ((spr->x & 15) == 0 && (spr->y & 15) == 0) { \
            spr->move = FALSE; \
            spr->dx = 0; \
            spr->dy = 0; \
        } \
    } \
    if (spr->anim != anm) { spr->anim = anm; } \
    if (spr->frame != frm) { spr->frame = frm; }

#define sp_updateSpriteDirty(spr) \
	if (spr->move == FALSE) { \
        tx1 = spr->x >> 4; \
        ty1 = spr->y >> 4; \
        tile1 = modu[tx1&31] + mapy[ty1]; \
        spr_chk = spriteListHead; \
        while (spr_chk != NULL) { \
            if (spr != spr_chk && spr_chk->dirty > 0) { \
                if (spr_chk->origTile == tile1 || \
                    spr_chk->destTile == tile1 || \
                    spr_chk->origTile+1 == tile1 || \
                    spr_chk->destTile+1 == tile1) { \
                    spr->dirty = 2; \
                    spr_chk->dirty = 2; \
                    for (j = 0; j < OVER_TILES; j++) {  \
                        if (tileMap->tilemap[tile1] == overlay_tiles[j]) {  \
                            tileMap->dirty[tile1] = 2;  \
                        }  \
                    }  \
                } \
            } \
            spr_chk = spr_chk->next; \
        } \
    }

#define sp_saveSpriteBack(spr, dst)  \
    spr->fx[frame] = spr->x; \
    spr->fy[frame] = spr->y; \
    scr_offset = ((spr->fx[frame] >> 3) & 0xFFFE) + wbytes[spr->fy[frame]]; \
    WaitBlitter(); \
    custom->bltcon0 = 0x9F0; \
    custom->bltcon1 = 0; \
    custom->bltafwm = 0xFFFF; \
    custom->bltalwm = 0xFFFF; \
    custom->bltamod = BITMAPLINEBYTES - 4; \
    custom->bltdmod = 0; \
    custom->bltapt  = dst + scr_offset; \
    custom->bltdpt  = spr->rest_bm + spr->rest_bpl[frame]; \
    custom->bltsize = spr->bltsize;

#define sp_restoreSpriteBack(spr, dst) \
    scr_offset = ((spr->fx[frame] >> 3) & 0xFFFE) + wbytes[spr->fy[frame]]; \
    WaitBlitter(); \
    custom->bltcon0 = 0x9F0; \
    custom->bltcon1 = 0; \
    custom->bltafwm = 0xFFFF; \
    custom->bltalwm = 0xFFFF; \
    custom->bltamod = 0; \
    custom->bltdmod = BITMAPLINEBYTES - 4; \
    custom->bltapt  = spr->rest_bm + spr->rest_bpl[frame]; \
    custom->bltdpt  = dst + scr_offset; \
    custom->bltsize = spr->bltsize; 
   
#define sp_drawSprite(spr, dst) \
    scr_offset = ((spr->fx[frame] >> 3) & 0xFFFE) + wbytes[spr->fy[frame]]; \
    bpl_offset = (((spr->frame << 4) >> 3) & 0xFFFE) + spr->wbytes[spr->anim<<4]; \
    wrd_offset = ((spr->fx[frame] & 0xf) << 12); \
    WaitBlitter(); \
    custom->bltcon0   = 0xFCA + wrd_offset; \
    custom->bltcon1   = wrd_offset; \
    custom->bltafwm   = 0xFFFF; \
    custom->bltalwm   = 0x0000; \
    custom->bltamod   = (spr->bm->wbytes << 1) - 4; \
    custom->bltbmod   = (spr->bm->wbytes << 1) - 4; \
    custom->bltcmod   = BITMAPLINEBYTES - 4; \
    custom->bltdmod   = custom->bltcmod; \
    custom->bltapt    = spr->bm->data + bpl_offset + spr->bm->wbytes; \
    custom->bltbpt    = spr->bm->data + bpl_offset; \
    custom->bltcpt    = dst + scr_offset; \
    custom->bltdpt    = dst + scr_offset; \
    custom->bltsize   = spr->bltsize; \
    if (spr->dirty > 0) \
        spr->dirty--;

#else

void waitVBlank(void) {
  ULONG vposr;

  do {
    vposr = GETLONG(custom->vposr);
  } while ((vposr & 0x1FF00) >= 0x12F00);

  do {
    vposr = GETLONG(custom->vposr);
  } while ((vposr & 0x1FF00) < 0x12F00);
}

void tm_drawTile(TileMap *tm, UBYTE *dst, WORD x, WORD y, WORD tile)
{
    WaitBlitter();
    
    custom->bltcon0 = 0xFCA;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltamod = 62;
    custom->bltbmod = 62;
    custom->bltcmod = BITMAPLINEBYTESB;
    custom->bltdmod = BITMAPLINEBYTESB;

    custom->bltapt  =  tm->offset[tile] + 32;
    custom->bltbpt  =  tm->offset[tile];
    custom->bltcpt  =  dst + ((x >> 3) & 0xFFFE) + wbytes[y];
    custom->bltdpt  =  custom->bltcpt;

    custom->bltsize = 4097;
}

void sp_initSpritePos(Sprite *spr, sx, sy) {
    spr->x = spr->fx[0] = spr->fx[1] = sx;
    spr->y = spr->fy[0] = spr->fy[1] = sy;
    spr->dirty = 2;
    spr->move = FALSE;
}

void sp_updateSpritePos(Sprite *sprListHead, Sprite *spr, TileMap *tm, UBYTE anm, UBYTE frm, UBYTE *wlk_tiles, UBYTE *over_tiles, UWORD *mapy, UWORD *modu) {
    UBYTE j;
    UWORD tx1, ty1, tx2, ty2;
    Sprite *spr_chk;
    if ((spr->x & 15) == 0 && (spr->y & 15) == 0) {
        if (spr->move == FALSE && (spr->dx != 0 || spr->dy != 0)) { 
            tx1 = spr->x >> 4;
            ty1 = spr->y >> 4;
            spr->origTile = modu[tx1&31] + mapy[ty1];
            tx2 = tx1 + spr->dx;
            ty2 = ty1 + spr->dy;
            spr->destTile = modu[tx2&31] + mapy[ty2];
            for (j = 0; j < WLK_TILES; j++) {
                if (wlk_tiles[j] == tm->tilemap[spr->destTile]) {
                    spr->move = TRUE;
                    break;
                }
            }
            spr_chk = sprListHead;
            while (spr_chk != NULL) {
                if (spr != spr_chk && spr_chk->dirty > 0) {
                    if (spr_chk->origTile == spr->origTile ||
                        spr_chk->destTile == spr->origTile) {
                        spr->dirty = 2;
                        spr_chk->dirty = 2;
                        for (j = 0; j < OVER_TILES; j++) {
                            if (tm->tilemap[spr->origTile] == over_tiles[j]) {
                                tm->dirty[spr->origTile] = 2;
                            }
                        }
                    }
                }
                spr_chk = spr_chk->next;
            }
            if (spr->move == FALSE) {
                spr->dx = 0;
                spr->dy = 0;
            }
        }
    }
    if (spr->move == TRUE || spr->anim != anm || spr->frame != frm) {
        spr->x += spr->dx;
        spr->y += spr->dy;
        for (j = 0; j < OVER_TILES; j++) {
            if (tm->tilemap[spr->origTile] == over_tiles[j]) {
                tm->dirty[spr->origTile] = 2;
            }
            if (tm->tilemap[spr->destTile] == over_tiles[j]) {
                tm->dirty[spr->destTile] = 2;
            }
        }
        spr->dirty = 2;
        if ((spr->x & 15) == 0 && (spr->y & 15) == 0) {
            spr->move = FALSE;
            spr->dx = 0;
            spr->dy = 0;
        }
    }
    if (spr->anim != anm) { spr->anim = anm; }
    if (spr->frame != frm) { spr->frame = frm; }
}

void sp_updateSpriteDirty(Sprite *sprListHead, Sprite *spr, TileMap *tm, UBYTE *over_tiles, UWORD *mapy, UWORD *modu) {
    if (spr->move == FALSE) {
        UWORD tx1, ty1, tile1, j;
        Sprite *spr_chk;
        tx1 = spr->x >> 4;
        ty1 = spr->y >> 4;
        tile1 = modu[tx1&31] + mapy[ty1];
        spr_chk = sprListHead;
        while (spr_chk != NULL) {
            if (spr != spr_chk && spr_chk->dirty > 0) {
                if (spr_chk->origTile == tile1 ||
                    spr_chk->destTile == tile1 ||
                    spr_chk->origTile+1 == tile1 ||
                    spr_chk->destTile+1 == tile1) {
                    spr->dirty = 2;
                    spr_chk->dirty = 2;
                    for (j = 0; j < OVER_TILES; j++) {
                        if (tm->tilemap[tile1] == over_tiles[j]) {
                            tm->dirty[tile1] = 2;
                        }
                    }
                }
            }
            spr_chk = spr_chk->next;
        }
    }
}

void sp_saveSpriteBack(Sprite* spr, UBYTE *dest) {
    ULONG scr_offset, map_offset;
    
    scr_offset = ((spr->x[frame] >> 3) & 0xFFFE) + wbytes[spr->y[frame]];
    map_offset = frame * ((spr->bm_wbytes * spr->height) << 2);
    
    WaitBlitter();

    custom->bltcon0 = 0x9F0;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltamod = BITMAPLINEBYTES - 4;
    custom->bltdmod = 0;
    
    custom->bltapt  = dest + scr_offset;
    custom->bltdpt  = spr->rest_bm + map_offset;
    custom->bltsize = spr->bltsize;
}

void sp_restoreSpriteBack(Sprite *spr, UBYTE *dest) {
    ULONG scr_offset, map_offset;
    
    scr_offset = ((spr->x[frame] >> 3) & 0xFFFE) + wbytes[spr->y[frame]];
    map_offset = frame * ((spr->bm_wbytes * spr->height) << 2);
    
    WaitBlitter();

    custom->bltcon0 = 0x9F0;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltamod = 0;
    custom->bltdmod = BITMAPLINEBYTES - 4;
    
    custom->bltapt  = spr->rest_bm + map_offset;
    custom->bltdpt  = dest + scr_offset;
    custom->bltsize = spr->bltsize;
}

void sp_drawSprite(Sprite *spr, UBYTE *dest, WORD sx, WORD sy) 
{
    ULONG scr_offset = ((spr->x[frame] >> 3) & 0xFFFE) + wbytes[spr->y[frame]];
    ULONG map_offset = ((sx >> 3) & 0xFFFE) + spr->wbytes[sy];

    WaitBlitter();

    custom->bltcon0 = 0xFCA + ((spr->x[frame] & 0xf) << 12);
    custom->bltcon1 = ((spr->x[frame] & 0xf) << 12);
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0x0000;
    custom->bltamod = 2 * spr->bm_wbytes - 4;
    custom->bltbmod = 2 * spr->bm_wbytes - 4;
    custom->bltcmod = BITMAPLINEBYTES - 4;
    custom->bltdmod = custom->bltcmod;//BITMAPLINEBYTES - 4;
    
    custom->bltapt  = spr->bm + map_offset + spr->bm_wbytes;
    custom->bltbpt  = spr->bm + map_offset;
    custom->bltcpt  = dest + scr_offset;
    custom->bltdpt  = dest + scr_offset;

    custom->bltsize = spr->bltsize;
}

#endif

TileMap* tm_createTileMap(Bitmap *bitmap, UBYTE *tilemap, UBYTE tile_width, UBYTE tile_height, UWORD map_width, UWORD map_height, UBYTE depth) {
    UWORD j;
    TileMap *tm = (TileMap*)AllocMem(sizeof(TileMap), MEMF_ANY);
    
    tm->map     = bitmap;
    tm->tilemap = tilemap;
    tm->twidth  = tile_width;
    tm->theight = tile_height;
    tm->tdepth  = depth;
    tm->mwidth  = map_width;
    tm->mheight = map_height;
    
    for (j = 0; j < TILES; j++) {
        tm->offset[j] = tm->map->data + ((j & 0xf) << 1) + ((j >> 4) << 12);
    }
    
    for (j = 0; j < MAP_SIZE; j++) {
        tm->dirty[j] = FALSE;
    }
    return tm; 
}

void tm_dealloc(TileMap* tm) 
{
    if (tm)
    {
        FreeMem(tm, sizeof(TileMap));
    }
}

// Sprite width must be word aligned
Sprite* sp_create(Bitmap *bitmap, WORD width, WORD height, WORD bm_width, WORD bm_height, WORD depth) 
{
    UBYTE y;
    Sprite *spr = (Sprite*)AllocMem(sizeof(Sprite), MEMF_ANY);
	
    spr->bm = bitmap;
    spr->rest_bm = NULL;
    
    spr->fx[0] = spr->fx[1] = spr->dx = 0;
    spr->fy[0] = spr->fy[1] = spr->dy = 0;
    spr->dir = spr->anim = spr->frame = 0;
    
    spr->width = width;
    spr->height = height;

    spr->move = spr->dirty = FALSE;
    
    spr->rest_bm = AllocMem((spr->width + 16) * (spr->height << 1) * spr->bm->depth, MEMF_CHIP);
    
    spr->rest_bpl[0] = 0;
    spr->rest_bpl[1] = (spr->bm->wbytes * spr->height) << 2;
    
    spr->bltsize = (height << 8) + (width >> 4) + 1;
    
    for (y = 0; y < spr->bm->height; y ++)
        spr->wbytes[y] = y * (spr->bm->depth * spr->bm->width >> 3) << 1;
    
    return spr;
}

void sp_dealloc(Sprite *spr) 
{
    if (spr) {
        if (spr->rest_bm)
            FreeMem(spr->rest_bm, (spr->width + 16) * (spr->height << 1) * spr->bm->depth);

        FreeMem(spr,sizeof(Sprite));
    }
}

Bitmap* bm_create(UBYTE *orig, WORD width, WORD height, BYTE depth) {
    Bitmap *bm = (Bitmap*)AllocMem(sizeof(Bitmap), 0L);
    bm->data = orig;
    bm->width = width;
    bm->height = height;
    bm->wbytes = width >> 3;
    bm->depth = depth;
    return bm;
}

void bm_dealloc(Bitmap *bm) {
    if (bm) {
        FreeMem(bm, sizeof(Bitmap));
    }
}

UBYTE joy_read(UWORD joynum)
{
    UBYTE ret = 0;
    UWORD joy;

    if(joynum == 0) 
        joy = custom->joy0dat;
    else
        joy = custom->joy1dat;

    ret += (joy >> 1 ^ joy) & 0x0100 ? UP : 0;  
    ret += (joy >> 1 ^ joy) & 0x0001 ? DOWN : 0;

    ret += joy & 0x0200 ? LEFT : 0;
    ret += joy & 0x0002 ? RIGHT : 0;

    if(joynum == 0) {
        ret += !(cia->ciapra & 0x0040) ? FIRE1 : 0; 
        ret += !(POTGOR & 0x0400) ? FIRE2 : 0;
    }
    else {
        ret += !(cia->ciapra & 0x0080) ? FIRE1 : 0;
        ret += !(POTGOR & 0x4000) ? FIRE2 : 0;
    }

    return ret;
}

void installInputHandler(void) {
    if ((inputMP = CreateMsgPort()))
    {
        if ((inputReq = CreateIORequest(inputMP, sizeof(*inputReq))))
        {
            inputDevice = OpenDevice("input.device", 0, (struct IORequest *)inputReq, 0);
            if (inputDevice == 0)
            {
                inputHandler.is_Node.ln_Type = NT_INTERRUPT;
                inputHandler.is_Node.ln_Pri = 127;
                inputHandler.is_Data = 0;
                inputHandler.is_Code = (APTR)NullInputHandler;
                inputHandler.is_Node.ln_Name=NameString;

                inputReq->io_Command = IND_ADDHANDLER;
                inputReq->io_Data = &inputHandler;

                DoIO((struct IORequest *)inputReq);
            }
        }
    }
}

void removeInputHandler(void) {
    if (inputDevice == 0)
    {
        inputReq->io_Data = &inputHandler;
        inputReq->io_Command = IND_REMHANDLER;
        DoIO((struct IORequest *)inputReq);
        CloseDevice((struct IORequest *)inputReq);
    }

    if (inputReq) 
        DeleteIORequest(inputReq);
    if (inputMP) 
        DeleteMsgPort(inputMP);
}

static LONG __interrupt __saveds VertBServer(void) {
    vbCounter++;
    return 0;
}

int installVBlankInt(void)
{
    if(vbInt = AllocMem(sizeof(struct Interrupt), MEMF_ANY|MEMF_CLEAR)) {
        vbInt->is_Node.ln_Type = NT_INTERRUPT;
        vbInt->is_Node.ln_Pri = -60;
        vbInt->is_Node.ln_Name = "VB_Engine";
        vbInt->is_Data = (APTR)&vbCounter;
        vbInt->is_Code = VertBServer;
        AddIntServer(INTB_VERTB, vbInt);
    }
    else {
        printf("Can't allocate memory for vblank interrupt\n");
        return 0;
    }
    return 1;
}

void removeVBlankInt(void)
{
    RemIntServer(INTB_VERTB, vbInt);
    FreeMem(vbInt, sizeof(struct Interrupt));
}

int main()
{
    UWORD oldInt, oldDMA;
    struct View *oldView;
    struct copinit *oldCopInit;
    BPTR file_ptr;
    UBYTE *mod = (UBYTE*)AllocMem(MOD_SIZE, MEMF_CHIP);
    UBYTE *tiles = (UBYTE*)AllocMem(TILES_SIZE, MEMF_CHIP);
    UBYTE* player = (UBYTE*)AllocMem(PLAYER_SIZE, MEMF_CHIP);
    UBYTE* map = (UBYTE*)AllocMem(MAP_SIZE, MEMF_ANY);
    
    Bitmap *tiles_bm;
    Bitmap *sprite_bm;
    
    Sprite *player_spr_srt[SPRITES_NB];
    Sprite *player_spr[SPRITES_NB];
    Sprite *sprite;
        
    Sprite *spr_chk;    
    TileMap *tileMap;

    UWORD tx1, ty1, tx2, ty2;
    UWORD tile1;
    UBYTE walk_tiles[WLK_TILES] = {0, 9, 10, 15, 16, 31, 32, 39, 43, 44, 45, 46, 47, 51, 55, 88};
    UBYTE overlay_tiles[OVER_TILES] = {9, 10, 51, 55, 88};

    UWORD mapy[16] = {0, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300};
    
    UWORD modu[32];
    
    UBYTE *bplptr[2];
    UBYTE *ptr;
    UBYTE *ptr10, *ptr11, *ptr12, *ptr13;
    UBYTE *ptr20, *ptr21, *ptr22, *ptr23;
    WORD i, j, x, y, fps = 0;
    
    ULONG clist[2];
  
    UBYTE spr_frames[4] = {0, 1, 0, 2};
    
    ULONG vposr;
    
    UBYTE joyData;
    
    UWORD map_offsetx[MAP_SIZE], map_offsety[MAP_SIZE];
    
    ULONG scr_offset, bpl_offset;
    UWORD wrd_offset;

    UBYTE frame = 0; 
    
    void *vbr;
    UBYTE isPal;

    // Do some precalcs
    
    for (i = 0; i < MAP_SIZE; i++) {
        map_offsetx[i] = (i << 4) % 320;
        map_offsety[i] = (i / 20) << 4;
    }

    for (i = 0; i < HEIGHT; i++) {
        wbytes[i] = i*BITMAPLINEBYTESI;
    }
    
    for (i = 0; i < 32; i++) {
        modu[i] = i%20;
    }
    
    // Load resources
    
    if (file_ptr = Open("data/intro1.mod", MODE_OLDFILE)) 
    {
            Read(file_ptr, mod, MOD_SIZE);
            Close(file_ptr);
    }
    
    if (file_ptr = Open("data/tiles3.dat", MODE_OLDFILE)) 
    {
            Read(file_ptr, tiles, TILES_SIZE);
            Close(file_ptr);
    }
    
    if (file_ptr = Open("data/nmap.dat", MODE_OLDFILE)) 
    {
            Read(file_ptr, map, MAP_SIZE);
            Close(file_ptr);
    }
    
    if (file_ptr = Open("data/character.dat", MODE_OLDFILE)) 
    {
            Read(file_ptr, player, PLAYER_SIZE);
            Close(file_ptr);
    }

    // open gfx lib and save original copperlist
    GfxBase = (struct GfxBase*)OldOpenLibrary("graphics.library");
    oldCopInit = GfxBase->copinit;
    oldView = GfxBase->ActiView;
    
    // Install input handler
    installInputHandler();
    
    task = FindTask(NULL);
    process = (struct Process *) task;
    winPtr = process->pr_WindowPtr;
    process->pr_WindowPtr = (APTR)-1;
    
    Forbid();
    
    // Save interrupts and DMA
    oldInt = custom->intenar;
    oldDMA = custom->dmaconr;

    // disable all interrupts and DMA
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff; 

    // set required bits of DMA
    custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG | DMAF_COPPER;

    // Allocate memory for bitplanes, interleaved format
    ptr10 = AllocMem(((WIDTH * HEIGHT) >> 3) << 2, MEMF_CHIP | MEMF_CLEAR);
    ptr11 = ptr10 + 40;
    ptr12 = ptr11 + 40;
    ptr13 = ptr12 + 40;
    
    ptr20 = AllocMem(((WIDTH * HEIGHT) >> 3) << 2, MEMF_CHIP | MEMF_CLEAR);
    ptr21 = ptr20 + 40;
    ptr22 = ptr21 + 40;
    ptr23 = ptr22 + 40;
    
    // Store pointers for double buffer bitplanes and copperlist 
    
    bplptr[0] = ptr20;
    bplptr[1] = ptr10;
    
    clist[0] = (ULONG)&cop1;
    clist[1] = (ULONG)&cop2;
    
    // Define bitplanes on copper list
    
    cop1[BPL0]     = (GETLONG(ptr10) >> 16) & 0xffff;
    cop1[BPL0 + 2] =  GETLONG(ptr10) & 0xffff;
    cop1[BPL1]     = (GETLONG(ptr11) >> 16) & 0xffff;
    cop1[BPL1 + 2] =  GETLONG(ptr11) & 0xffff;
    cop1[BPL2]     = (GETLONG(ptr12) >> 16) & 0xffff;
    cop1[BPL2 + 2] =  GETLONG(ptr12) & 0xffff;
    cop1[BPL3]     = (GETLONG(ptr13) >> 16) & 0xffff;
    cop1[BPL3 + 2] =  GETLONG(ptr13) & 0xffff;
    
    cop2[BPL0]     = (GETLONG(ptr20) >> 16) & 0xffff;
    cop2[BPL0 + 2] =  GETLONG(ptr20) & 0xffff;
    cop2[BPL1]     = (GETLONG(ptr21) >> 16) & 0xffff;
    cop2[BPL1 + 2] =  GETLONG(ptr21) & 0xffff;
    cop2[BPL2]     = (GETLONG(ptr22) >> 16) & 0xffff;
    cop2[BPL2 + 2] =  GETLONG(ptr22) & 0xffff;
    cop2[BPL3]     = (GETLONG(ptr23) >> 16) & 0xffff;
    cop2[BPL3 + 2] =  GETLONG(ptr23) & 0xffff;
    
    // Create tilemap and sprite bitmap
    
    tiles_bm = bm_create(tiles, 256, 144, 4);
    sprite_bm = bm_create(player, 48, 64, 4);
        
    // Create tilemap
    
    tileMap = tm_createTileMap(tiles_bm, map, 16, 16, 256, 144, 4);
    
    // Create sprites
    
    for (i = 0; i < SPRITES_NB; i ++) {
        player_spr[i] = sp_create(sprite_bm, 16, 16, 48, 64, 4);
        player_spr_srt[i] = player_spr[i];
    }

    for (i = 0; i < SPRITES_NB; i ++) {
        WORD mp = 0;
        while (tileMap->tilemap[mp] != 0) {
            mp = rand()%MAP_SIZE;
        }
        sp_initSpritePos(player_spr_srt[i], (mp%20)<<4, (mp/20)<<4);
    }
    
    spriteListHead = player_spr_srt[0];
    
    // Create sorted array of sprites
    
    player_spr_srt[0]->next = player_spr_srt[1];
    
    player_spr_srt[SPRITES_NB-1]->next = NULL;
    
    for (i = 1; i < SPRITES_NB-1; i ++) {
        player_spr_srt[i]->next = player_spr_srt[i+1];
    }
    
    // Paint tilemap on double buffer bitplanes
    
    ptr = bplptr[frame];
    for (j = 0; j < 2; j++) {
        i = 0;
        for (y = 0; y < HEIGHT - 1; y += WBLOCK) {
            for (x = 0; x < WIDTH - 1; x+= WBLOCK) {
                WORD tile = map[i] - 1;
                tm_drawTile(tileMap, ptr, x, y, 38);
                if (tile >= 0) {
                    tm_drawTile(tileMap, ptr, x, y, tile);
                }
                i++;
            }
        }
        for (i = 0; i < SPRITES_NB; i ++) {
            sp_saveSpriteBack(player_spr_srt[i], ptr);
        }
        frame ^= 1;
        ptr = bplptr[frame];
    }
    
    frame = 0;
    
    LoadView(NULL);
    WaitTOF();
    WaitTOF();
    
    custom->cop1lc = clist[frame];
    
    isPal = SysBase->PowerSupplyFrequency < 59;
  
    vbr = (SysBase->AttnFlags & AFF_68010) ?
          (void *)Supervisor((void *)GetVBR) : NULL;

    // Initialise mod player
          
    mt_install_cia(custom, vbr, isPal);
    mt_init(custom, mod, NULL, 0);
    mt_musicmask(custom, 0xf);
    mt_Enable = 1;
    
    vbCounter = 0;
    
    // Install VBlank interrupt
    
    installVBlankInt();
    
    // loop until mouse clicked;
    while(cia->ciapra & CIAF_GAMEPORT0) {
        
        // Read joystick
        joyData = joy_read(1);

        if (player_spr[SPRITES_NB-1]->move == FALSE) {
            if(joyData & DOWN) {
                player_spr[SPRITES_NB-1]->dy  =  1;
                player_spr[SPRITES_NB-1]->dx  =  0;
                player_spr[SPRITES_NB-1]->dir =  2;
            } 
            else if(joyData & LEFT) {
                player_spr[SPRITES_NB-1]->dx  = -1;
                player_spr[SPRITES_NB-1]->dy  =  0;
                player_spr[SPRITES_NB-1]->dir =  3;
            }
            else if(joyData & RIGHT) {
                player_spr[SPRITES_NB-1]->dx  =  1;
                player_spr[SPRITES_NB-1]->dy  =  0;
                player_spr[SPRITES_NB-1]->dir =  1;
            } 
            else if(joyData & UP) {
                player_spr[SPRITES_NB-1]->dy  = -1;
                player_spr[SPRITES_NB-1]->dx  =  0;
                player_spr[SPRITES_NB-1]->dir =  0;
            }
        }
        
        for (i = 0; i < SPRITES_NB-1; i++) {
            if (player_spr[i]->move == FALSE && (player_spr[i]->x&15) == 0 && (player_spr[i]->y&15) == 0) {
                UBYTE dir1 = random() & 1;
                UBYTE dir2 = random() & 1;
                if (dir1 == 0) {
                    if (dir2 == 0) {
                        player_spr[i]->dy  =  1;
                        player_spr[i]->dx  =  0;
                        player_spr[i]->dir =  2;
                    }
                    else {
                        player_spr[i]->dy  = -1;
                        player_spr[i]->dx  =  0;
                        player_spr[i]->dir =  0;
                    }
                }
                else {
                    if (dir2 == 0) {
                        player_spr[i]->dx  =  1;
                        player_spr[i]->dy  =  0;
                        player_spr[i]->dir =  1;
                    }
                    else {
                        player_spr[i]->dx  = -1;
                        player_spr[i]->dy  =  0;
                        player_spr[i]->dir =  3;
                    }
                }
            }
        }
        
        // Update sprites position
        sprite = spriteListHead;
        while (sprite != NULL) {
            UBYTE spr_frame = sprite->move==TRUE ? spr_frames[(fps >> 2) & 3] : 0;
            UBYTE spr_anim = sprite->dir;
#ifdef INLINE_FUNCTIONS
            sp_updateSpritePos(sprite, tileMap, spr_anim, spr_frame);
#else
            sp_updateSpritePos(spriteListHead, sprite, tileMap, spr_anim, spr_frame, walk_tiles, overlay_tiles, mapy, modu);
#endif
            sprite = sprite->next;
        }
        
        // Check dirty sprites against non dirty sprites and overlay tiles
        sprite = spriteListHead;
        while (sprite != NULL) {
#ifdef INLINE_FUNCTIONS
            sp_updateSpriteDirty(sprite);
#else
            sp_updateSpriteDirty(spriteListHead, sprite, tileMap, overlay_tiles, mapy, modu);
#endif
            sprite = sprite->next;
        }
        
        // Order sprites by vertical position
        
        for (i = 1; i < SPRITES_NB; i++) {
            sprite = player_spr_srt[i];
            j = i - 1;
            while (j >= 0 && player_spr_srt[j]->y > sprite->y)
            {
                player_spr_srt[j + 1] = player_spr_srt[j];  
                j--;
            }
            player_spr_srt[j + 1] = sprite;
        }
        
        spriteListHead = player_spr_srt[0];
        
        player_spr_srt[0]->next = player_spr_srt[1];
        
        player_spr_srt[SPRITES_NB-1]->next = NULL;
        
        for (i = 1; i < SPRITES_NB-1; i ++) {
            player_spr_srt[i]->next = player_spr_srt[i+1];
        }

        // Wait VBlank and swap buffers
        waitVBlank();
        
        custom->cop1lc = clist[frame];
        ptr = bplptr[frame];

        // Restore and save sprite background
        sprite = spriteListHead;
        while (sprite != NULL) {
            if (sprite->dirty > 0) {
                sp_restoreSpriteBack(sprite, ptr);
            }
            sprite = sprite->next;
        }
        
        sprite = spriteListHead;
        while (sprite != NULL) {
            if (sprite->dirty > 0) {
                sp_saveSpriteBack(sprite, ptr);
            }
            sprite = sprite->next;
        }
        
        // Paint sprites
        sprite = spriteListHead;
        while (sprite != NULL) {
            if (sprite->dirty > 0) {
                sp_drawSprite(sprite, ptr);
            }
            sprite = sprite->next;
        }
        
        // Repaint overlay tiles
        for (i = 40; i < MAP_SIZE; i++) {
            if (tileMap->dirty[i] > 0) {
                tm_drawTile(tileMap, ptr, map_offsetx[i], map_offsety[i], tileMap->tilemap[i] - 1);
                tileMap->dirty[i] --;
            }
        }
        
        fps++;
        frame ^= 1;
        
    }
    // Remove VBlank interrupt
    removeVBlankInt();
    
    // Remove mod player
    mt_Enable = 0;
    mt_end(custom);
    mt_remove_cia(custom);
    
    // restore DMA
    custom->dmacon = 0x7fff;
    custom->dmacon = oldDMA | DMAF_SETCLR | DMAF_MASTER;

    // restore original copper
    custom->cop1lc = (ULONG) oldCopInit;

    // restore interrupts
    custom->intena = oldInt | 0xc000;

    Permit();
    
    process->pr_WindowPtr = winPtr;
    
    LoadView(oldView);
    WaitTOF();
   
    // Free allocated resources
    
    for (i = 0; i < SPRITES_NB; i ++) {
        sp_dealloc(player_spr[i]);
    }

    tm_dealloc(tileMap);
    
    bm_dealloc(tiles_bm);
    bm_dealloc(sprite_bm);
    
    FreeMem(ptr10, ((WIDTH * HEIGHT) >> 3) << 2);
    FreeMem(ptr20, ((WIDTH * HEIGHT) >> 3) << 2);
    FreeMem(mod, MOD_SIZE);
    FreeMem(tiles, TILES_SIZE);
    FreeMem(player, PLAYER_SIZE);
    
    // Remove input handler
    removeInputHandler();
    
    printf("vblank: %i - frames: %i\n", vbCounter, fps);
    CloseLibrary((struct Library *)GfxBase);
    
    return 0;
}
