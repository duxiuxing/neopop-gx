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

unsigned char flash_ram[65536];		/*** Hopefully no more than 64k :) ***/

BOOL
system_io_flash_read (_u8 * buffer, _u32 len)
{
  if (len > 65536)
    len = 65536;
  memcpy (buffer, &flash_ram, len);
  return TRUE;
}

BOOL
system_io_flash_write (_u8 * buffer, _u32 len)
{
  if (len > 65536)
    len = 65536;

  memcpy (&flash_ram, buffer, len);

  return TRUE;
}
