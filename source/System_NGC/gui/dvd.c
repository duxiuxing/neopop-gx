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
 * Nintendo Gamecube DVD Reading Library
 *
 * This is NOT a complete DVD library, in that it works for reading 
 * ISO9660 discs only.
 *
 * If you need softmod drivecodes etc, look elsewhere.
 * There are known issues with libogc dvd handling, so these work
 * outside of it ,if you will.
 *
 *  This is ideal for using with a gc-linux self booting DVD only.
 *  Go http://www.gc-linux.org for further information and the tools
 *  for your platform.
 *
 *  To keep libOGC stable, make sure you call DVD_Init before using
 *  these functions.
 ***************************************************************************/
#include "neopop.h"
#include "font.h"
#include "wkf.h"

#ifdef HW_RVL
#include "di/di.h"
#endif

#ifndef HW_RVL
static u64 DvdMaxOffset = 0x57057C00;                                 /* 1.4 GB max. by default */
volatile unsigned long *dvd = (volatile unsigned long *) 0xCC006000;  /* DVD I/O Address base */
static unsigned char *inquiry=(unsigned char *)0x80000004;            /* pointer to drive ID */
#else
static u64 DvdMaxOffset = 0x118244F00LL;                              /* 4.7 GB max. */
#endif

 /** Due to lack of memory, we'll use this little 2k keyhole for all DVD operations **/
unsigned char DVDreadbuffer[2048] ATTRIBUTE_ALIGN (32);

 /***************************************************************************
  * dvd_read
  *
 * Read DVD disc sectors
  ***************************************************************************/
int dvd_read (void *dst, unsigned int len, u64 offset)
{
  
  if (len > 2048) return 1;      /*** We only allow 2k reads **/
  if(offset < DvdMaxOffset)      /*** Let's not read past end of DVD ***/
  {
     unsigned char *buffer = (unsigned char *) (unsigned int) DVDreadbuffer;
     DCInvalidateRange((void *)buffer, len);

#ifndef HW_RVL
      dvd[0] = 0x2E;
      dvd[1] = 0;
      dvd[2] = 0xA8000000;
      dvd[3] = (u32)(offset >> 2);
      dvd[4] = len;
      dvd[5] = (unsigned long) buffer;
      dvd[6] = len;
      dvd[7] = 3;
      
    /*** Enable reading with DMA ***/
    while (dvd[7] & 1);

    /*** Ensure it has completed ***/
    if (dvd[0] & 0x4) return 0;
      
#else
    if (DI_ReadDVD(buffer, len >> 11, (u32)(offset >> 11))) return 0;
#endif
    memcpy (dst, buffer, len);
    return 1;
  }

  return 0; 
}

/****************************************************************************
 * dvd_motor_off
 *
 * Stop the DVD Motor
 *
 * This can be used to prevent the Disc from spinning during playtime
 ****************************************************************************/
void dvd_motor_off( )
{
  
  ShowAction("Stopping DVD drive...");

#ifdef HW_RVL
  DI_StopMotor();
#else

  dvd[0] = 0x2e;
  dvd[1] = 0;
  dvd[2] = 0xe3000000;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = 0;
  dvd[6] = 0;
  dvd[7] = 1; // Do immediate
  while (dvd[7] & 1);

  /*** PSO Stops blackscreen at reload ***/
  dvd[0] = 0x14;
  dvd[1] = 0;

#endif
}


#ifndef HW_RVL
/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ****************************************************************************/
void uselessinquiry ()
{

  dvd[0] = 0;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 1;

  while (dvd[7] & 1);
}

/****************************************************************************
 * dvd_inquiry
 *
 * Return the Current DVD Drive ID
 *
 * This can be used to determine whereas the console is a Gamecube or a Wii
 ****************************************************************************/
void dvd_inquiry()
{
    dvd[0] = 0x2e;
    dvd[1] = 0;
    dvd[2] = 0x12000000;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = 0x80000000;
    dvd[6] = 0x20;
    dvd[7] = 3;
    while( dvd[7] & 1 );
    DCFlushRange((void *)0x80000000, 32);
    
      int driveid = (int)inquiry[2];
  
  if ((driveid == 4) || (driveid == 6) || (driveid == 8))
  {
      // do nothing just leave
  }
  else if  ((driveid == 2) && (__wkfSpiReadId() != 0 && __wkfSpiReadId() != 0xFFFFFFFF) && (wkfIsInserted(0) == true) )
  {
     WaitWKF();
     __wkfReset();
     dvd_inquiry();
  }

}
#endif