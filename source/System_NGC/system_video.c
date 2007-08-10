/******************************************************************************
 *  NEOPOP - Emulator as in Dreamland
 *  Copyright (C) 2001-2002 neopop_uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/
#include <neopop.h>
#include <ogcsys.h>
#include <sdcard.h>
#include <font.h>

void genpal ();

unsigned int *xfb[2];	/*** Double buffered ***/
int whichfb = 0;		/*** Switch ***/
GXRModeObj *vmode;		/*** General video mode ***/

/*** GX ***/
#define TEX_WIDTH 512
#define TEX_HEIGHT 256
#define DEFAULT_FIFO_SIZE 256 * 1024
unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static unsigned char texturemem[TEX_WIDTH * (TEX_HEIGHT + 8) * 2] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

#define HASPECT 76
#define VASPECT 54

/* New texture based scaler */
typedef struct tagcamera
{
  Vector pos;
  Vector up;
  Vector view;
} camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
s16 square[] ATTRIBUTE_ALIGN (32) =
{
	/*
	 * X,   Y,  Z
	 * Values set are for roughly 4:3 aspect
	 */
	-HASPECT, VASPECT, 0,	// 0
	HASPECT, VASPECT, 0,	// 1
	HASPECT, -VASPECT, 0,	// 2
	-HASPECT, -VASPECT, 0,	// 3
};

static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};


/****************************************************************************
 * VideoThreading
 ****************************************************************************/
#define TSTACK 16384
lwpq_t videoblankqueue;
lwp_t vbthread;
static unsigned char vbstack[TSTACK];

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank. 
 *
 * Putting LWP to good use :)
 ****************************************************************************/
static void *
vbgetback (void *arg)
{
  while (1)

    {
      VIDEO_WaitVSync ();	 /**< Wait for video vertical blank */
      LWP_SuspendThread (vbthread);
    }
}


/****************************************************************************
 * InitVideoThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ****************************************************************************/
void
InitVideoThread ()
{

	/*** Initialise a new queue ***/
  LWP_InitQueue (&videoblankqueue);

	/*** Create the thread on this queue ***/
  LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 80);
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ****************************************************************************/
static void
copy_to_xfb ()
{
  if (copynow == GX_TRUE)

    {
      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetColorUpdate (GX_TRUE);
      GX_CopyDisp (xfb[whichfb], GX_TRUE);
      GX_Flush ();
      copynow = GX_FALSE;
    }

  //frameticker++;
}


/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void
draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

  GX_SetNumTexGens (1);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  GX_InvalidateTexAll ();
  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
		 GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void
draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_Color1x8 (c);
  GX_TexCoord2f32 (s, t);
}

static void
draw_square (Mtx v)
{
  Mtx m;			// model matrix.
  Mtx mv;			// modelview matrix.

  guMtxIdentity (m);
  guMtxTransApply (m, m, 0, 0, -100);
  guMtxConcat (v, m, mv);

  GX_LoadPosMtxImm (mv, GX_PNMTX0);
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (0, 0, 0.0, 0.0);
  draw_vert (1, 0, 1.0, 0.0);
  draw_vert (2, 0, 1.0, 1.0);
  draw_vert (3, 0, 0.0, 1.0);
  GX_End ();
}

/****************************************************************************
 * StartGX
 *
 * This function initialises the GX.
 * WIP3 - Based on texturetest from libOGC examples.
 ****************************************************************************/
void
StartGX (void)
{
  Mtx p;

  GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);

  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE,
		    vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering,
		   ((vmode->viHeight ==
		     2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);
  vwidth = SCREEN_WIDTH;
  vheight = SCREEN_HEIGHT;
  oldvheight = oldvwidth = 0;
}

/****************************************************************************
 * InitGCVideo
 *
 * This function MUST be called at startup.
 ****************************************************************************/
u8 isWII = 0;
extern int dvd_inquiry();

void
InitGCVideo ()
{

  /*
   * Before doing anything else under libogc,
   * Call VIDEO_Init
   */
  VIDEO_Init ();
  PAD_Init ();

  /*
   * Reset the video mode
   * This is always set to 60hz
   * Whether your running PAL or NTSC
   */
  vmode = &TVNtsc480IntDf;
  VIDEO_Configure (vmode);

   /*** Now configure the framebuffer. 
	     Really a framebuffer is just a chunk of memory
	     to hold the display line by line.
   **/

  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** I prefer also to have a second buffer for double-buffering.
	     This is not needed for the console demo.
   ***/
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** Define a console ***/
  console_init(xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

  /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

  /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer(xfb[0]);

  /*** Increment frameticker and timer ***/
  VIDEO_SetPreRetraceCallback(copy_to_xfb);

  /*** Get the PAD status updated by libogc ***/
  VIDEO_SetPostRetraceCallback(PAD_ScanPads);
  VIDEO_SetBlack (FALSE);
  
  /*** Update the video for next vblank ***/
  VIDEO_Flush();

  VIDEO_WaitVSync();		/*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

  genpal ();
  DVD_Init ();
  SDCARD_Init ();

  unpackBackdrop ();
  init_font();
  copynow = GX_FALSE;
  StartGX ();
  InitVideoThread ();

  /* Wii drive detection for 4.7Gb support */
  int driveid = dvd_inquiry();
  if ((driveid == 4) || (driveid == 6) || (driveid == 8)) isWII = 0;
  else isWII = 1;
}

/****************************************************************************
 * BuildPalette
 *
 * The NeoGeo Pocket stores colour in BGR4 format.
 * Create a palette, shifted to RGB565 for texture mapping.
 ****************************************************************************/
unsigned short gc565pal[0x1000]; /**< 565 palette for rendering **/
void
genpal ()
{
  int r, g, b;
  int count = 0;

  for (b = 0; b < 16; b++)
    {
      for (g = 0; g < 16; g++)
	{
	  for (r = 0; r < 16; r++)
	    gc565pal[count++] = ((r << 12) | (g << 7) | (b << 1));
	}
    }
}

/****************************************************************************
 * Update Video
 ****************************************************************************/
void
update_video ()
{
  int h, w;

  /* Ensure previous vb has complete */
  while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
  {
      usleep (50);
  }

  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
  {
	  /** Update scaling **/
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();
      memset (&view, 0, sizeof (Mtx));
	  guLookAt(view, &cam.pos, &cam.up, &cam.view);
      GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

  unsigned short *dst = (unsigned short *) &texturemem[0];
  unsigned short *src = (unsigned short *) &cfb[0];

  int hpos;
  int lrow2 = SCREEN_WIDTH;
  int lrow3 = SCREEN_WIDTH << 1;
  int lrow4 = lrow3 + SCREEN_WIDTH;
  for (h = 0; h < vheight; h += 4)

    {
      for (w = 0; w < vwidth; w += 4)

	{
	  hpos = w;
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos]];

	  hpos = w + lrow2;
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos]];

	  hpos = w + lrow3;
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos]];

	  hpos = w + lrow4;
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos++]];
	  *dst++ = gc565pal[src[hpos]];
	}
    src += SCREEN_WIDTH << 2;
  }

  DCFlushRange (&texturemem, TEX_WIDTH * TEX_HEIGHT * 2);
  GX_SetNumChans (1);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  draw_square (view);
  GX_DrawDone ();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  copynow = GX_TRUE;

  /* Return to caller, don't waste time waiting for vb */
  LWP_ResumeThread (vbthread);
}
