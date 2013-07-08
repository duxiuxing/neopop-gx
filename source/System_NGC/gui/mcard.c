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
#include "neopop.h"
#include "state.h"
#include "dvd.h"
#include "font.h"
#include "saveicon.h"	/*** Nice little icon - thanks brakken! ***/


/* Support for MemCards  */
/**
 * libOGC System Work Area
 */
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);

/**
 * DMA Transfer Area.
 * Must be 32-byte aligned.
 * 64k SRAM + 2k Icon
 */
unsigned char srambuffer[0x20000] ATTRIBUTE_ALIGN (32);
unsigned char savebuffer[0x10000];
card_dir CardDir;
card_file CardFile;
card_stat CardStatus;

extern void read_state_0050 (char *filename);
extern int state_store (char *filename);
extern RomHeader *rom_header;
extern int CARDSLOT;
extern int use_SDCARD;

/****************************************************************************
 * SDCARD Access functions
 *
 * We use the same buffer as for Memory Card manager
 * Function returns TRUE on success.
 *****************************************************************************/
int SD_ManageFile(char *filename, int direction)
{
   char name[1024];
   FILE *handle;
   int len = 0;
   int offset = 0;
   int filesize;

   // build complete SDCARD filename
   sprintf (name, "/neopop/saves/%s", filename);

   // open file
   handle = direction ? fopen(name, "rb") : fopen(name, "wb");


	if (handle == NULL)
	{
		sprintf (filename, "Error opening %s", name);
		WaitPrompt(filename);
		return 0;
	}
	
	switch (direction)
	{
		case 0: // SAVING
			filesize = state_store ("NEOPOP");
                        len = fwrite(savebuffer, 1, filesize, handle);
                        fclose(handle);
			if (len != filesize)
			{
				sprintf (filename, "Error writing %s", name);
				WaitPrompt(filename);
				return 0;
			}
			sprintf (filename, "Saved %d bytes successfully", filesize);
			WaitPrompt (filename);
			return 1;
		
		case 1: // LOADING 
                        while ((len = fread(&savebuffer[offset], 1, 2048, handle)) > 0) { offset += len; }
                        fclose(handle);
                        read_state_0050 ("NEOPOP");
			sprintf (filename, "Loaded %d bytes successfully", offset);
			WaitPrompt (filename);
			return 1;
	}

  return 0;
}

/****************************************************************************
 * MountTheCard
 *
 * libOGC provides the CARD_Mount function, and it should be all you need.
 * However, experience with previous emulators has taught me that you are
 * better off doing a little bit more than that!
 *
 * Function returns TRUE on success.
 *****************************************************************************/
int MountTheCard ()
{
	int tries = 0;
	int CardError;
	while (tries < 10)
	{
		*(unsigned long *) (0xcc006800) |= 1 << 13; /*** Disable Encryption ***/
#ifndef HW_RVL
		uselessinquiry ();
#endif
		VIDEO_WaitVSync ();
		CardError = CARD_Mount (CARDSLOT, SysArea, NULL); /*** Don't need or want a callback ***/
		if (CardError == 0) return 1;
		else EXI_ProbeReset ();
		tries++;
	}
	return 0;
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 * Returns TRUE if found.
 ****************************************************************************/
int CardFileExists (char *filename)
{
	int CardError = CARD_FindFirst (CARDSLOT, &CardDir, TRUE);
	while (CardError != CARD_ERROR_NOFILE)
	{
		CardError = CARD_FindNext (&CardDir);
		if (strcmp ((char *) CardDir.filename, filename) == 0) return 1;
	}
	return 0;
}

/****************************************************************************
 * ManageState
 *
 * Here is the main State Management stuff.
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
extern int neosize;

int ManageState (int direction)
{
  char filename[128];
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"NeoPop 0.7"}, {"Savegame"} };
  int outbytes = 0;
  int sbo;
  int state_size = 0;

  if (!neosize) return 0;

  if (direction) ShowAction ("Loading State ...");
  else ShowAction ("Saving State ...");

  /*** No idea what the damn ROM is, so just use CRC32 ***/
  sprintf (filename, "%s.npz", rom_header->name);
  strcpy (comment[1], filename);

  /* device is SDCARD, let's go */
  if (use_SDCARD) return SD_ManageFile(filename,direction);

  /* device is MCARD, we continue */
  if (direction == 0)		/*** Saving ***/
  {
	  /*** Build the output buffer ***/
      memcpy (&srambuffer, &icon, 2048);
      memcpy (&srambuffer[2048], &comment[0], 64);
      state_size = state_store ("NEOPOP");
      memcpy (&srambuffer[2112], &savebuffer, state_size);
  }

  outbytes = (2048 + 64 + state_size);

  /*** Initialise the CARD system ***/
  memset (&SysArea, 0, CARD_WORKAREA);
  CARD_Init ("NEOP", "00");

  /*** Attempt to mount the card ***/
  CardError = MountTheCard ();

  if (CardError)
  {
    /*** Retrieve the sector size ***/
    CardError = CARD_GetSectorSize (CARDSLOT, &SectorSize);

    switch (direction)
	{
		case 0:	/*** Saving ***/
				/*** Determine number of blocks on this card ***/
			blocks = (outbytes / SectorSize) * SectorSize;
			if (outbytes % SectorSize) blocks += SectorSize;

			/*** Check if a previous save exists ***/
			if (CardFileExists (filename))
			{
				CardError = CARD_Open (CARDSLOT, filename, &CardFile);
				if (CardError)
				{
					sprintf (action, "Error Open : %d", CardError);
					WaitPrompt (action);
					CARD_Unmount (CARDSLOT);
					return 0;
				}
			}
			else
			{
				/*** Create a new slot ***/
				CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
				if (CardError)
				{
					sprintf (action, "Error create : %d %d", CardError, CARDSLOT);
					WaitPrompt (action);
					CARD_Unmount (CARDSLOT);
					return 0;
				}
			}

			/*** Continue and save ***/
			CARD_GetStatus (CARDSLOT, CardFile.filenum, &CardStatus);
			CardStatus.icon_addr = 0x0;
			CardStatus.icon_fmt = 2;
			CardStatus.icon_speed = 1;
			CardStatus.comment_addr = 2048;
			CARD_SetStatus (CARDSLOT, CardFile.filenum, &CardStatus);

			/*** And write the blocks out ***/
			sbo = 0;
			while (outbytes > 0)
			{
				CardError = CARD_Write (&CardFile, &srambuffer[sbo], SectorSize, sbo);
				outbytes -= SectorSize;
				sbo += SectorSize;
			}

			CARD_Close (&CardFile);
			CARD_Unmount (CARDSLOT);

			sprintf (action, "Saved %d bytes successfully", blocks);
			WaitPrompt (action);
			return 1;

		default: /*** Loading ***/
			if (!CardFileExists (filename))
			{
				WaitPrompt ("No State Save Found");
				CARD_Unmount (CARDSLOT);
				return 0;
			}

			memset (&CardFile, 0, sizeof (CardFile));
			CardError = CARD_Open (CARDSLOT, filename, &CardFile);
			if (CardError)
			{
				sprintf (action, "Error Open : %d", CardError);
				WaitPrompt (action);
				CARD_Unmount (CARDSLOT);
				return 0;
			}

			blocks = CardFile.len;
			if (blocks < SectorSize) blocks = SectorSize;
			if (blocks % SectorSize) blocks++;

			/*** Just read the file back in ***/
			sbo = 0;
			int size = blocks;
			while (blocks > 0)
			{
				CARD_Read (&CardFile, &srambuffer[sbo], SectorSize, sbo);
				sbo += SectorSize;
				blocks -= SectorSize;
			}

			CARD_Close (&CardFile);
			CARD_Unmount (CARDSLOT);

			memcpy (&savebuffer, &srambuffer[2112], 0x10000);
			read_state_0050 ("NEOPOP");

			/*** Inform user ***/
			sprintf (action, "Loaded %d bytes successfully", size);
			WaitPrompt (action);
			return 1;
		}
	}
	else WaitPrompt ("Unable to mount memory card");
	
	return 0;		/*** Signal failure ***/
}
