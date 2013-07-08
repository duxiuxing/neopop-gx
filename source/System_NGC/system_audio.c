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
//#include <ogcsys.h>

extern int ConfigRequested;

/****************************************************************************
 * A U D I O
 ****************************************************************************/
#define ABSIZE 3216 * 2
unsigned char soundbuffer[2][ABSIZE] __attribute__ ((__aligned__ (32)));
//int buffsize[2] = { 0, 0 };
int whichab = 0;
//int IsPlaying = 0;
int IsMixing = 0;

#define AUDIOSTACK 16384
//lwpq_t audioqueue;
//lwp_t athread;
static unsigned char astack[AUDIOSTACK];

void system_sound_update ();
/*
static void *
AudioThread (void *arg)
{
  while (1)
    {
      whichab ^= 1;
      memset (&soundbuffer[whichab], 0, ABSIZE);
      system_sound_update ();
      LWP_SuspendThread (athread);
    }
    return 0;
}
*/
/****************************************************************************
 * AudioSwitchBuffers
 *
 * Genesis Plus only provides sound data on completion of each frame.
 * To try to make the audio less choppy, this function is called from both the
 * DMA completion and update_audio.
 *
 * Testing for data in the buffer ensures that there are no clashes.
 ****************************************************************************/
static void
AudioSwitchBuffers ()
{
// AUDIO_StopDMA ();
// AUDIO_InitDMA ((u32) soundbuffer[whichab], ABSIZE);
      whichab ^= 1;
      memset (&soundbuffer[whichab], 0, ABSIZE);
      memset(&astack, 0, AUDIOSTACK);
      system_sound_update ();
      AUDIO_InitDMA ((u32) soundbuffer[whichab], ABSIZE);
  DCFlushRange (&soundbuffer[whichab], ABSIZE);
  AUDIO_StartDMA ();
  
//  whichab ^= 1;  
//  IsPlaying = 1;   
//system_sound_update ();
//  if (LWP_ThreadIsSuspended (athread))
//    LWP_ResumeThread (athread);
}

/****************************************************************************
 * InitGCAudio
 *
 * Stock code to set the DSP at 48Khz
 ****************************************************************************/
void
InitGCAudio ()
{
  AUDIO_Init (NULL);

  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioSwitchBuffers);

//  LWP_InitQueue (&audioqueue);
//  LWP_CreateThread (&athread, AudioThread, NULL, astack, AUDIOSTACK, 80);

}

/****************************************************************************
 * update_audio
 ****************************************************************************/
void
update_audio ()
{
  AudioSwitchBuffers ();
}

/****************************************************************************
 * NeoPop Specific
 ****************************************************************************/
void
system_sound_chipreset (void)
{
  sound_init (48000);
}

void
system_sound_silence (void)
{
  return;
}

/****************************************************************************
 * system_update_audio
 *
 * This is the Nintendo GC Audio Mixer
 *
 * NeoPop delivers PSG sound as 16bit Mono Samples
 *                 DAC sound as 8bit Mono Samples
 *
 * Here, we mix these together, and duplicate for output
 ****************************************************************************/
#define PSGSAMPLES 804 * 2	/*** Do roughly 1 frame ***/
#define DACSAMPLES 134 * 2	/*** DAC is delivered at 8Khz ALWAYS! ***/
unsigned short psgbuffer[PSGSAMPLES];
unsigned char dacbuffer[DACSAMPLES];
void
system_sound_update ()
{
  int count;
//char *bufc;
  unsigned char *bufc;
  unsigned short *src;
  unsigned short *dst;
  unsigned char m;
  short s;
  int i;

  if (ConfigRequested)
    return;

  IsMixing = 1;

	/**
	 * Step One. Get 16bit Mono PSG samples
	 */
  sound_update ((unsigned short *) &psgbuffer, PSGSAMPLES << 1);

	/**
	 * Step Two. Get 8bit Mono DAC samples
	 */
//dac_update ((char *) &dacbuffer, DACSAMPLES);
  dac_update ((unsigned char *) &dacbuffer, DACSAMPLES);

	/**
	 * Step Three. Mix these samples.
	 */
  count = DACSAMPLES;
  bufc = (unsigned char *) &dacbuffer;
//dst = (signed short *) &psgbuffer;
  dst = (unsigned short *) &psgbuffer;
  while (count)
    {
      m = *bufc++;

      s = (m << 8) ^ 0x8000;

      for (i = 0; i < 6; i++)
	{
	  *dst++ += s;
	}

      count--;
    }

	/**
	 * Step Four. Duplicate for stereo
	 */
  count = PSGSAMPLES;
  dst = (unsigned short *) &soundbuffer[whichab];
  src = (unsigned short *) &psgbuffer;
  while (count)
    {
      *dst++ = *src;
      *dst++ = *src++;
      count--;
    }

  IsMixing = 0;
}
