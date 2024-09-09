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
 *
 * Nintendo Gamecube Menus
 *
 * Please put any user menus here! - softdev March 12 2006
 ***************************************************************************/
#include "neopop.h"
#include "dvd.h"
#include "font.h"
#include "filesel.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#endif

int mega;
extern int legal();
extern u16 getMenuButtons(void);
/***************************************************************************
 * drawmenu
 *
 * As it says, simply draws the menu with a highlight on the currently
 * selected item :)
 ***************************************************************************/
char menutitle[60] = { "" };
int menu = 0;

void drawmenu (char items[][20], int maxitems, int selected)
{
  int i;
  int ypos;

  ypos = (310 - (fheight * maxitems)) >> 1;
  ypos += 130;

  ClearScreen ();
  WriteCentre (134, menutitle);

  for (i = 0; i < maxitems; i++)
  {
     if (i == selected) WriteCentre_HL (i * fheight + ypos, (char *) items[i]);
     else WriteCentre (i * fheight + ypos, (char *) items[i]);
  }

  SetScreen ();
}

/****************************************************************************
 * domenu
 *
 * Returns index into menu array when A is pressed, -1 for B
 ****************************************************************************/
int domenu (char items[][20], int maxitems)
{
  int redraw = 1;
  int quit = 0;
  short p;
  int ret = 0;
  signed char b;

  while (quit == 0)
  {
     if (redraw)
     {
        drawmenu (&items[0], maxitems, menu);
        redraw = 0;
     }

     p = getMenuButtons ();
     b = PAD_StickX (0);


     // Look for up
     if (p & PAD_BUTTON_UP)
     {
        redraw = 1;
        menu--;
        if (menu < 0) menu = maxitems - 1;
     }

     // Look for down
     if (p & PAD_BUTTON_DOWN)
     {
        redraw = 1;
        menu++;
        if (menu == maxitems) menu = 0;
     }

     if (p & PAD_BUTTON_A)
     {
        quit = 1;
        ret = menu;
     }

     if (p & PAD_BUTTON_B)
     {
        quit = 1;
        ret = -1;
     }

     if ((mega == 1) && (( menu == 0 ) || ( menu == 1 )))
     {
        if ((b > 40) || (p & PAD_BUTTON_RIGHT))
        {
           quit = 1;
           ret = menu;
        }

        if ((b < -40) || (p & PAD_BUTTON_LEFT))
        {
           quit = 1;
           ret = 0 - 2 - menu;
        }
     }
  }

  return ret;
}

/****************************************************************************
 * display menu
 *
 ****************************************************************************/
extern s16 square[];
extern int oldvwidth, oldvheight;

void displaymenu ()
{
  int ret;
  int quit = 0;
  int misccount = 4;
  char miscmenu[4][20];
  int prevmenu = menu;

  strcpy (menutitle, "Adjust Aspect and Scale");

  //  Capture running values //
  sprintf (miscmenu[0], "Scale X     - %02d", square[3]);
  sprintf (miscmenu[1], "Scale Y     - %02d", square[1]);
  sprintf (miscmenu[2], "Reset Default");
  strcpy (miscmenu[3], "Return to previous");

  mega = 1;
  menu = 0;

  while (quit == 0)
  {
     ret = domenu (&miscmenu[0], misccount);
     switch (ret)
     {
        case 0:  // Scale X
        case -2:
           if (ret<0) square[3] -= 2;
           else square[3] += 2;
           if (square[3] < 40) square[3] = 80;
           if (square[3] > 80) square[3] = 40;
           square[6] = square[3];
           square[0] = square[9] = -square[3];
           sprintf (miscmenu[0], "Scale X     - %02d", square[3]);
           oldvwidth = -1;
           break;
        case 1:  // Scale Y
        case -3:
           if (ret<0) square[1] -= 2;
           else square[1] += 2;
           if (square[1] < 30) square[1] = 60;
           if (square[1] > 60) square[1] = 30;
           square[4] = square[1];
           square[7] = square[10] = -square[1];
           sprintf (miscmenu[1], "Scale Y     - %02d", square[1]);
           oldvheight = -1;
           break;
        case 2:
           sprintf (miscmenu[0], "Scale X     - 76");
              square[3] = 76;
              square[6] = square[3];
              square[0] = square[9] = -square[3];
           sprintf (miscmenu[1], "Scale Y     - 54");
              square[1] = 54;
              square[4] = square[1];
              square[7] = square[10] = -square[1];
           oldvwidth = oldvheight = -1;
           break;
        case -1:
        case 3:
           quit = 1;
	   mega = 0;
           break;
     }
  }

  menu = prevmenu;
}

/****************************************************************************
* Generic Load/Save menu
*
****************************************************************************/
int CARDSLOT = CARD_SLOTA;
int use_SDCARD = 1;
extern int ManageState (int direction);

int statemenu ()
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  int count = 5;
  char items[5][20];

  strcpy (menutitle, "Savestate Manager");
  menu = 2;

  if (use_SDCARD) sprintf(items[0], "Device: SD");
  else sprintf(items[0], "Device:  MCARD");

  if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
  else sprintf(items[1], "Use: SLOT B");

  sprintf(items[2], "Save State");
  sprintf(items[3], "Load State");
  strcpy (items[4], "Return to previous");

  while (quit == 0)
  {
     ret = domenu (&items[0], count);
     switch (ret)
     {
        case -1:
        case  4:
           quit = 1;
           break;
        case 0:
           use_SDCARD ^= 1;
           if (use_SDCARD) sprintf(items[0], "Device: SD");
           else sprintf(items[0], "Device:  MCARD");
           break;
        case 1:
           CARDSLOT ^= 1;
           if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
           else sprintf(items[1], "Use: SLOT B");
           break;
        case 2:
        case 3:
           if (ManageState (ret-2)) return 1;
           break;
     }
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Option menu
 *
 ****************************************************************************/
void optionmenu ()
{
  int prevmenu = menu;
  int ret;
  int quit = 0;
  int count = 4;
  char item[4][20] = {
    {"Aspect & Size"},
    {"Savestate Manager"},
    {"View Credits"},
    {"Return to previous"}
};

  menu = 0;

  while (quit == 0)
  {
     strcpy (menutitle, "Emulator Options");
     ret = domenu (&item[0], count);
     switch (ret)
     {
        case -1: // Button B
        case 3:  // Quit
           quit = 1;
           menu = prevmenu;
           break;
        case 0:  // Display Settings
           displaymenu (); 
           break;
        case 1:  // Savestate Manager
           statemenu();
           break;
        case 2:
           legal();
           break;
     }
  }  menu = prevmenu;
}

/****************************************************************************
 * Load Rom menu
 *
 ****************************************************************************/
extern int neosize;

void loadmenu ()
{
  int prevmenu = menu;
  int ret;
  int quit = 0;
  int count = 6;
  char item[6][20] = 
  {
    {"Load from SD"},
#ifdef HW_RVL
    {"Load from USB"},
#endif
    {"Load from IDE-EXI"},
#ifndef HW_RVL
    {"Load from WKF"},
#endif
    {"Load from DVD"},
    {"Stop DVD Motor"},
    {"Return to previous"}
  };

  menu = 0;

  while (quit == 0)
  {
     strcpy (menutitle, "");
     ret = domenu (&item[0], count);
     switch (ret)
     {
        // Button B - Return to Previous
        case -1:
        case 5: 
           quit = 1;
           menu = prevmenu;
           break;

        // Load from DVD
        case 3:
           OpenDVD ();
           if (neosize) quit = 1;
           break;

        // Stop DVD Disc
        case 4:
           dvd_motor_off();
           break;

        // Load from SD, USB, IDE-EXI, or WKF device
        default: 
           OpenSD (ret);
           if (neosize) quit = 1;
           break;

     }
  }  menu = prevmenu;
}


/****************************************************************************
 * Top menu
 *
 ****************************************************************************/
extern unsigned int *xfb[2];
extern int whichfb;
extern GXRModeObj *vmode;


void MainMenu ()
{
  menu = 0;
  int ret;
  int quit = 0;
#ifdef HW_RVL
  int maincount = 6;
  char mainmenu[6][20] = 
#else
  int maincount = 5;
  char mainmenu[5][20] = 
#endif
  {
    {"Play Game"},
    {"Reset Game"},
    {"Load New Game"},
    {"Emulator Options"},
#ifdef HW_RVL
    {"Return to Loader"},
#endif
    {"System Reboot"}
  };

  while (quit == 0)
  {
     strcpy (menutitle, "Version 0.71.2");
     ret = domenu (&mainmenu[0], maincount);
     switch (ret)
     {
        case -1:  // Button B
        case 0:   // Play Game
           quit = 1;
           break;
        case 1:
           reset ();
           quit = 1;
           break;
        case 2:
           loadmenu();
           if (neosize) quit = 1;
           break;
        case 3:
           optionmenu();
           break;
        case 4:
           VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
           VIDEO_Flush();
           VIDEO_WaitVSync();
#ifdef HW_RVL
           DI_Close();
           exit(0);
           break;
        case 5:
           VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
           VIDEO_Flush();
           VIDEO_WaitVSync();
           SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
           SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
	   break;
	}
  }

  /*** Remove any still held buttons ***/
  while(PAD_ButtonsHeld(0))  PAD_ScanPads();
#ifdef HW_RVL
  while (WPAD_ButtonsHeld(0)) WPAD_ScanPads();
#endif
  

#ifndef HW_RVL
  /*** Stop the DVD from causing clicks while playing ***/
  uselessinquiry ();
#endif
}
