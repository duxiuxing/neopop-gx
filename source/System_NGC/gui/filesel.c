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
 * File Selection
 ***************************************************************************/ 
#include "filesel.h"
#include "neopop.h"
#include "dvd.h"
#include "iso9660.h"
#include "font.h"
#include "unzip.h"
#include "wkf.h"
#include "ata.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include "di/di.h"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#define PAGESIZE 12

static int fat_type   = 0;
static int maxfiles;
u8 havedir = 0;
u8 haveFATdir = 0;
u8 UseSDCARD = 0;
FILE * filehandle;
char rootFATdir[MAXPATHLEN];
char root[10] = "";
int LoadFile (unsigned char *buffer);
int offset = 0;
int selection = 0;
int old_selection = 0;
int old_offset = 0;

extern int neosize;
extern unsigned char *neorom;
extern u16 getMenuButtons(void);

#ifndef HW_RVL
extern const DISC_INTERFACE* WKF_slot;
#endif
extern const DISC_INTERFACE* IDEA_slot;
extern const DISC_INTERFACE* IDEB_slot;

/***************************************************************************
 * Showfile screen
 ***************************************************************************/
static void ShowFiles (int offset, int selection) 
{
  int i, j;
  char text[MAXJOLIET+2];

  ClearScreen ();
  j = 0;

  for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
  {
     memset(text,0,MAXJOLIET+2);
     if (filelist[i].flags) sprintf(text, "[%s]", filelist[i].filename + filelist[i].filename_offset);
     else sprintf (text, "%s", filelist[i].filename + filelist[i].filename_offset);

     if (j == (selection - offset)) WriteCentre_HL ((j * fheight) + 120, text);
     else WriteCentre ((j * fheight) + 120, text);
     j++;
  }
  SetScreen ();
}

/***************************************************************************
 * Update SDCARD curent directory name 
 ***************************************************************************/ 
int updateSDdirname()
{
  int size=0;
  char *test;
  char temp[1024];
//  char msg[1024];

  if (strcmp(filelist[selection].filename,".") == 0) return 0;    // current directory doesn't change
  else if (strcmp(filelist[selection].filename,"..") == 0) {      // go up to parent directory
     sprintf(temp,"%s",rootFATdir);                                // determine last subdirectory namelength
     test= strtok(temp,"/");
     while (test != NULL) { 
        size = strlen(test);
        test = strtok(NULL,"/");
     }

     size = strlen(rootFATdir) - size;                             // remove last subdirectory name
     rootFATdir[size-1] = 0;
     return 1;
  }
  else {                                                          // update current directory name
     sprintf(rootFATdir, "%s%s/",rootFATdir, filelist[selection].filename); 
     return 1;
  } 
}

int stricmp(const char* a, const char* b)
{
    for (; tolower(*a) == tolower(*b); a += 1, b += 1)
        if (*a == '\0')
            return 0;
    return tolower(*a) - tolower(*b);
}
/***************************************************************************
* FileSortCallback
*
* Quick sort callback to sort file entries with the following order:
*  .
*  ..
*  <dirs>
*  <files>
***************************************************************************/
static int FileSortCallback(const void *f1, const void *f2)
{
  // Special case for implicit directories
  if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
  {
     if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
     if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
     if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
     if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
  }

  // If one is a file and one is a directory the directory is first
  if(((FILEENTRIES *)f1)->flags == 1 && ((FILEENTRIES *)f2)->flags == 0) return -1;
  if(((FILEENTRIES *)f1)->flags == 0 && ((FILEENTRIES *)f2)->flags == 1) return 1;

  return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/***************************************************************************
 * Browse SDCARD subdirectories 
 ***************************************************************************/ 
int parseSDdirectory()
{
  int nbfiles = 0;
  char filename[MAXPATHLEN];
  char filename1[MAXPATHLEN];

  DIR* dp = opendir(rootFATdir);                            // open directory
     if (!dp) {
        sprintf(filename, "Error opening %s", rootFATdir);
        WaitPrompt (filename);
        return 0;
     }

  struct dirent *entry = readdir(dp);
  struct stat filestat;

  while ((entry != NULL)&& (nbfiles < MAXFILES) ){         // list entries
     if (strcmp(filename,".") != 0) {
        memset(&filelist[nbfiles], 0, sizeof (FILEENTRIES));    
        sprintf(filelist[nbfiles].filename,"%s", entry->d_name);         
        sprintf(filename1, "%s%s", rootFATdir, filelist[nbfiles].filename); 
        stat(filename1, &filestat);  
        filelist[nbfiles].length = filestat.st_size;
        filelist[nbfiles].flags  = (filestat.st_mode & S_IFDIR) ? 1 : 0;
        nbfiles++;
     }

     entry = readdir(dp);                                  // next entry
  }
  closedir(dp);

  // Sort the file list
  qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

  return nbfiles;  
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the File listing
 ****************************************************************************/
void FileSelector () 
{
  short p;
  int haverom = 0;
  int redraw = 1;
  int go_up = 0;
  int i,size;

  while (haverom == 0)
  {
     if (redraw) ShowFiles (offset, selection);
        redraw = 0;
        p = getMenuButtons();

        // *** check selection screen changes *** //

        if (p & PAD_BUTTON_LEFT)                             // scroll displayed filename
        {
           if (filelist[selection].filename_offset > 0)
           {
              filelist[selection].filename_offset --;
              redraw = 1;
           }
        }
        else if (p & PAD_BUTTON_RIGHT)
        {
           size = 0;
           for (i=filelist[selection].filename_offset; i<strlen(filelist[selection].filename); i++)
              size += font_size[(int)filelist[selection].filename[i]];
           if (size > back_framewidth)
           {
              filelist[selection].filename_offset ++;
              redraw = 1;
           }
        }

        else if (p & PAD_BUTTON_DOWN)                        // highlight next item
        {
           filelist[selection].filename_offset = 0;
           selection++;
           if (selection == maxfiles) selection = offset = 0;
           if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
           redraw = 1;
        }

        else if (p & PAD_BUTTON_UP)                          // highlight previous item
        {
           filelist[selection].filename_offset = 0;
           selection--;
           if (selection < 0)
           {
              selection = maxfiles - 1;
              offset = selection - PAGESIZE + 1;
           }
           if (selection < offset) offset -= PAGESIZE;
           if (offset < 0) offset = 0;
           redraw = 1;
        }

        else if (p & PAD_TRIGGER_L)                        // go back one page
        {
           filelist[selection].filename_offset = 0;
           selection -= PAGESIZE;
           if (selection < 0)
           {
              selection = maxfiles - 1;
              offset = selection - PAGESIZE + 1;
           }
           if (selection < offset) offset -= PAGESIZE;
           if (offset < 0) offset = 0;
           redraw = 1;
        }

        else if (p & PAD_TRIGGER_R)                        // go forward one page
        {
           filelist[selection].filename_offset = 0;
           selection += PAGESIZE;
           if (selection > maxfiles - 1) selection = offset = 0;
           if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
           redraw = 1;
        }

        // *** Check pressed key *** //

        if (p & PAD_BUTTON_B)                              // go up one directory or quit
        {
           filelist[selection].filename_offset = 0;
           if ((!UseSDCARD) && (basedir == rootdir)) return;
           if (UseSDCARD)
           {
              char foo[10];
              sprintf(foo,"%s/",root);
              if ((strcmp(rootFATdir,foo) == 0)) return;
           }
           go_up = 1;
        }

        if (p & PAD_TRIGGER_Z)                             // quit
        {
           filelist[selection].filename_offset = 0;
           return;
        }

        if ((p & PAD_BUTTON_A) || go_up)                   // open selected file or directory
        {
           filelist[selection].filename_offset = 0;
           if (go_up)
           {
              go_up = 0;
              selection = 1;
           }
           
           // *** This is directory *** //
           if (filelist[selection].flags)
           {
              if (UseSDCARD)  //  SDCARD directory handler
              {
                 int status = updateSDdirname();           // update current directory
                    if (status == 1)                       // move to new directory
                    {
                       if (selection == 1)                 // reinit selector
                       {                                   //   previous value is saved for one level
                          selection = old_selection;
                          offset = old_offset;
                          old_selection = 0;
                          old_offset = 0;
                       }
                       else
                       {
                          old_selection = selection;       // save current selector value
                          old_offset = offset;
                          selection = 0;
                          offset = 0;
                       }
                       maxfiles = parseSDdirectory();      // set new entry list
                       if (!maxfiles)
                       {                                   // read error:  quit
                          WaitPrompt ("Error reading directory !");
                          haverom   = 1;
                          haveFATdir = 0;
                       }
                    }
                    else if (status == -1)                 // quit
                    {
                       haverom   = 1;
                       haveFATdir = 0;
                    }
              }
              else            // DVD directory handler
              {
                 if (selection != 0)                       // move to a new directory
                 {
                    rootdir = filelist[selection].offset;  // update current directory
                    rootdirlength = filelist[selection].length;
                    if (selection == 1)                    // reinit selector
                    {                                      //   previous value is saved for one level
                       selection = old_selection;
                       offset = old_offset;
                       old_selection = 0;
                       old_offset = 0;
                    }
                    else                                   // save current selector value
                    {
                       old_selection = selection;
                       old_offset = offset;
                       selection = 0;
                       offset = 0;
                    }
                    maxfiles = parsedirectory ();          // get new entry list
                 }
              }
           }
           else // *** This is a file *** //
           {    // if neosize is found, then exit load menu and start emulator...
              rootdir = filelist[selection].offset;
              rootdirlength = filelist[selection].length;   
              neosize = LoadFile (&neorom[0]);
              rom.length = neosize;
              rom.data = neorom;
              rom_loaded ();
              reset ();
              haverom = 1;
           }
        redraw = 1;
     }
    }
}

/****************************************************************************
 * OpenDVD
 *
 * Function to load a DVD directory and display to user.
 ****************************************************************************/
void OpenDVD () 
{
  UseSDCARD = 0;

#ifndef HW_RVL
  DVD_Init ();
  dvd_inquiry();
#endif

  if (!getpvd()) {
     ShowAction("Mounting DVD ... Wait");
     
#ifdef HW_RVL
    u32 val;
    DI_GetCoverRegister(&val);  

    if(val & 0x1)
    {
      WaitPrompt("No Disc inserted !");
      return;
    }

    DI_Mount();
    while(DI_GetStatus() & DVD_INIT);
    if (!(DI_GetStatus() & DVD_READY))
    {
      char msg[50];
      sprintf(msg, "DI Status Error: 0x%08X\n",DI_GetStatus());
      WaitPrompt(msg);
      return;
    }
#else
     DVD_Mount();
#endif
     
     havedir = 0;
     if (!getpvd()) {
        WaitPrompt ("Failed to mount DVD");
        return;
     }
  }

  if (havedir == 0) { 
     haveFATdir = 0;                                        // don't mess with SD entries
     rootdir = basedir;                                    // Reset root directory
     old_selection = selection = offset = old_offset = 0;  // reinit selector

     ShowAction ("Reading Directory ...");                 // Parse initial root directory and get entries list
     if ((maxfiles = parsedirectory ())) {
        FileSelector ();                                   // Select an entry
        havedir = 1;                                       // set entries list for next access
     }
  }
  else FileSelector ();                                    // Retrieve previous entries list and make a new selection
}

/****************************************************************************
 * OpenSD
 *
 * Function to load a SDCARD directory and display to user.
 ****************************************************************************/ 
int OpenSD (int type) 
{

  UseSDCARD = 1;

  if (type == TYPE_IDE)
  {
     if ( IDEA_slot->startup() && fatMountSimple("IDEA", IDEA_slot) ) sprintf (root, "IDEA:"); 
     else if ( IDEB_slot->startup() && fatMountSimple("IDEB", IDEB_slot) ) sprintf (root, "IDEB:"); 
     else { WaitPrompt ("IDE-EXI not initialized"); return 0; }
  }
#ifdef HW_RVL
  else if (type == TYPE_SD) sprintf (root, "sd:");
  else if (type == TYPE_USB) sprintf (root, "usb:");
#else
  else if (type == TYPE_WKF)
  {
     if ( WKF_slot->startup() && fatMountSimple("WKF", WKF_slot) )  sprintf (root, "WKF:");
     else { WaitPrompt ("WKF not initialized"); return 0; }
  }
#endif
  
  /* if FAT device type changed, reload filelist */
  if (fat_type != type) 
  {
    haveFATdir = 0;
  }
  fat_type = type;


  if (haveFATdir == 0) {

     havedir = 0;                                          // don't mess with DVD entries
     old_selection = selection = offset = old_offset = 0;  // reinit selector

     sprintf (rootFATdir, "%s/roms/NGP/", root);     // Reset SD root directory

     DIR* dir = opendir(rootFATdir);                       // if directory doesn't exist use root as default
     if (dir == NULL) sprintf (rootFATdir, "%s/", root);
     else closedir(dir);

     ShowAction ("Reading Directory ...");                 // Parse initial root directory and get entries list
     if ((maxfiles = parseSDdirectory ())) {
        FileSelector ();                                   // Select an entry
        haveFATdir = 1;                                    // set entries list for next access
     }
     else {
        WaitPrompt ("No Entries Found !!");
        return 0;
     }
  }
  else  FileSelector ();   // Retrieve previous entries list and make a new selection
return 1;
}

/****************************************************************************
 * LoadFile
 *
 * This function will load a file from DVD or SDCARD, in BIN, SMD or ZIP format.
 * The values for offset and length are inherited from rootdir and 
 * rootdirlength.
 *
 * The buffer parameter should re-use the initial ROM buffer.
 ****************************************************************************/
int LoadFile (unsigned char *buffer) 
{
  int offset;
  int blocks;
  int i;
  u64 discoffset;
  char readbuffer[2048];
  char fname[MAXPATHLEN];
  rootdirlength = 0;

  if (UseSDCARD) {  // open file
     sprintf(fname, "%s%s",rootFATdir,filelist[selection].filename);
     filehandle = fopen(fname, "rb");
     if (filehandle == NULL){
        WaitPrompt ("Unable to open file!");
        haveFATdir = 0;
        return 0;
     }
     else {
        fseek(filehandle, 0, SEEK_END);
        rootdirlength = ftell(filehandle);
        fseek(filehandle, 0, SEEK_SET);
     }
  }
  
  // confirm rootdirlength 
  if (rootdirlength == 0) { WaitPrompt ("rootdirlength == 0 ??"); return 0;}

  // How many 2k blocks to read 
  blocks = rootdirlength / 2048;
  offset = 0;
  discoffset = rootdir;
    
  ShowAction ("Loading ... Wait");
  
  // Read first data chunk
  if (UseSDCARD) fread(readbuffer, 1, 2048, filehandle); // SD
  else dvd_read (&readbuffer, 2048, discoffset);         // DVD

  if (!IsZipFile ((char *) readbuffer)) {
     if (UseSDCARD) fseek(filehandle, 0, SEEK_SET);
     // read data chunks
     for (i = 0; i < blocks; i++) {
        if (UseSDCARD) fread(readbuffer, 1, 2048, filehandle);
        else dvd_read (readbuffer, 2048, discoffset);
        memcpy (buffer + offset, readbuffer, 2048);
        offset += 2048;
        discoffset += 2048;
     }
     // And final cleanup  
     if (rootdirlength % 2048) {
        i = rootdirlength % 2048;
        if (UseSDCARD) fread(readbuffer, 1, i, filehandle);
        else dvd_read (readbuffer, 2048, discoffset);
        memcpy (buffer + offset, readbuffer, i);
     }
  }
  else return UnZipBuffer (buffer, discoffset, rootdirlength);
  
  if (UseSDCARD) fclose(filehandle);
  return rootdirlength;
}