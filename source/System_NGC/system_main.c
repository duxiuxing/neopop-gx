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
#include <font.h>
#include <state.h>
#include <system_audio.h>
#include <system_video.h>
#include <system_input.h>

unsigned char system_frameskip_key = 0;
int neosize;
unsigned char *neorom;

extern void MainMenu ();
extern void legal ();
extern int ConfigRequested;

/**
 * system_message
 *
 * Does nothing ... but needs to be here
 */
void
system_message (char *vaMessage, ...)
{
  return;
}

/**
 * system_VBL 
 *
 * Called at each frame completion
 */
void
system_VBL ()
{
  update_video ();
  //system_sound_update();
  //update_audio();
  system_update_input ();
}

/**
 * Entry point of emulator
 */
int
main (int argc, char *argv[])
{
  InitGCVideo ();

  system_colour = COLOURMODE_AUTO;		/**< Only do colour emulation **/
  language_english = TRUE;			/**< Works for me! **/
  system_frameskip_key = 1;
  legal ();
  
  /*** Allocate cart_rom here ***/
  neorom = malloc(4194304 + 32);
  if ((unsigned int)neorom & 0x1f) neorom += 32 - ((unsigned int)neorom & 0x1f);
  memset(neorom, 0, 4194304);
  neosize = 0;

  while (!neosize) MainMenu ();
  
  rom.length = neosize;
  rom.data = neorom;
  rom_loaded ();
  reset ();
  bios_install ();
  InitGCAudio ();
  update_audio ();

  /**
   * Emulation loop
   */
  while (1)
  {
	if (ConfigRequested)
	{
		MainMenu ();
		ConfigRequested = 0;
	}
	emulate ();
  }
  return 0;
}
