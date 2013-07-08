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
#include <math.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

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


#ifdef HW_RVL

#define MAX_HELD_CNT 15
static u32 held_cnt = 0;

static u32 wpadmap[3][8] =
{
  {
    WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_2, WPAD_BUTTON_1,
    WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS
  },
  {
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
    WPAD_BUTTON_A, WPAD_BUTTON_B,
    WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS
  },
  {
    WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
    WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT,
    WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B,
    WPAD_CLASSIC_BUTTON_MINUS, WPAD_CLASSIC_BUTTON_PLUS
  }
};

#define PI 3.14159265f

static s8 WPAD_StickX(u8 chan,u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * sin(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}


static s8 WPAD_StickY(u8 chan, u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * cos(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}
#endif

u16 getMenuButtons(void)
{
  
  /* slowdown input updates */
  VIDEO_WaitVSync();
  
  /* get gamepad inputs */
  PAD_ScanPads();
  u16 p = PAD_ButtonsDown(0);
  s8 x  = PAD_StickX(0);
  s8 y  = PAD_StickY(0);
  if (x > 70) p |= PAD_BUTTON_RIGHT;
  else if (x < -70) p |= PAD_BUTTON_LEFT;
  if (y > 60) p |= PAD_BUTTON_UP;
  else if (y < -60) p |= PAD_BUTTON_DOWN;
  
  
#ifdef HW_RVL
  /* get wiimote + expansions inputs */
  WPAD_ScanPads();
  u32 q = WPAD_ButtonsDown(0);
  u32 h = WPAD_ButtonsHeld(0);
  x = WPAD_StickX(0, 0);
  y = WPAD_StickY(0, 0);

  /* is Wiimote directed toward screen (horizontal/vertical orientation) ? */
  struct ir_t ir;
  WPAD_IR(0, &ir);

  /* wiimote directions */
  if (q & WPAD_BUTTON_UP)         p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
  else if (q & WPAD_BUTTON_DOWN)  p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
  else if (q & WPAD_BUTTON_LEFT)  p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
  else if (q & WPAD_BUTTON_RIGHT) p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;
  
  if (h & WPAD_BUTTON_UP)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
    }
  }
  else if (h & WPAD_BUTTON_DOWN)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
    }
  }
  else if (h & WPAD_BUTTON_LEFT)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
    }
  }
  else if (h & WPAD_BUTTON_RIGHT)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;
    }
  }
  else
  {
    held_cnt = 0;
  }

  /* analog sticks */
  if (y > 70)       p |= PAD_BUTTON_UP;
  else if (y < -70) p |= PAD_BUTTON_DOWN;
  if (x < -60)      p |= PAD_BUTTON_LEFT;
  else if (x > 60)  p |= PAD_BUTTON_RIGHT;

  /* classic controller directions */
  if (q & WPAD_CLASSIC_BUTTON_UP)         p |= PAD_BUTTON_UP;
  else if (q & WPAD_CLASSIC_BUTTON_DOWN)  p |= PAD_BUTTON_DOWN;
  if (q & WPAD_CLASSIC_BUTTON_LEFT)       p |= PAD_BUTTON_LEFT;
  else if (q & WPAD_CLASSIC_BUTTON_RIGHT) p |= PAD_BUTTON_RIGHT;

  /* wiimote keys */
  if (q & WPAD_BUTTON_MINUS)  p |= PAD_TRIGGER_L;
  if (q & WPAD_BUTTON_PLUS)   p |= PAD_TRIGGER_R;
  if (q & WPAD_BUTTON_A)      p |= PAD_BUTTON_A;
  if (q & WPAD_BUTTON_B)      p |= PAD_BUTTON_B;
  if (q & WPAD_BUTTON_2)      p |= PAD_BUTTON_A;
  if (q & WPAD_BUTTON_1)      p |= PAD_BUTTON_B;
  if (q & WPAD_BUTTON_HOME)   p |= PAD_TRIGGER_Z;

  /* classic controller keys */
  if (q & WPAD_CLASSIC_BUTTON_FULL_L) p |= PAD_TRIGGER_L;
  if (q & WPAD_CLASSIC_BUTTON_FULL_R) p |= PAD_TRIGGER_R;
  if (q & WPAD_CLASSIC_BUTTON_A)      p |= PAD_BUTTON_A;
  if (q & WPAD_CLASSIC_BUTTON_B)      p |= PAD_BUTTON_B;
  if (q & WPAD_CLASSIC_BUTTON_HOME)   p |= PAD_TRIGGER_Z;

 #endif
  
  
    return p;
}



void
system_update_input ()
{
  int i;
  unsigned short p;
  char ret = 0;
  signed char x, y;

  /* update PAD status */
  p = PAD_ButtonsHeld(0);


// Exit Emulator and Return to Menu
#ifdef HW_RVL
  if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
#else
  if (p & PAD_TRIGGER_L)
#endif
    {
      ConfigRequested = 1;
      return;
    }

#ifdef HW_RVL
  u32 exp;

  /* update WPAD status */
  WPAD_ScanPads();
  if (WPAD_Probe(0, &exp) == WPAD_ERR_NONE)
  {
     p = WPAD_ButtonsHeld(0);

     if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
     {
        ConfigRequested = 1;
        return;
     }

     /* PAD Buttons */
     for (i = 0; i < 7; i++)
     {
        if (p & wpadmap[exp][i])
        ret |= neomap[i];
     }
  }
  
  /*** And do any Analog fixup ***/
  x = WPAD_StickX(0, 0);
  y = WPAD_StickY(0, 0);
#else
   for (i = 0; i < 7; i++)
   {
      if (p & gcpadmap[i])
         ret |= neomap[i];
   }
   
  /*** And do any Analog fixup ***/
  x = PAD_StickX (0);
  y = PAD_StickY (0);
#endif

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
