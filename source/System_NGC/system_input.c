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

int ConfigRequested = 0;

/*** Some equates to make my life easy ***/
#define JOYUP 		1
#define JOYDOWN 	2
#define JOYLEFT		4
#define JOYRIGHT 	8
#define JOYA		16
#define JOYB		32
#define JOYOPTION	64

unsigned short gcpadmap[] = { PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
  PAD_BUTTON_A, PAD_BUTTON_B,
  PAD_BUTTON_START
};
char neomap[] = { JOYUP, JOYDOWN, JOYLEFT, JOYRIGHT,
  JOYA, JOYB, JOYOPTION
};

void
system_update_input ()
{
  int i;
  unsigned short p;
  char ret = 0;
  signed char x, y;

  p = PAD_ButtonsHeld (0);

  if (p & PAD_TRIGGER_Z)
    {
      ConfigRequested = 1;
      return;
    }

  for (i = 0; i < 7; i++)
    {
      if (p & gcpadmap[i])
	ret |= neomap[i];
    }

	/*** And do any Analog fixup ***/
  x = PAD_StickX (0);
  y = PAD_StickY (0);

  if (x > 45)
    ret |= JOYRIGHT;
  if (x < -45)
    ret |= JOYLEFT;
  if (y > 45)
    ret |= JOYUP;
  if (y < -45)
    ret |= JOYDOWN;

	/*** Directly update RAM ***/
  ram[0x6f82] = ret;

}
