#ifndef _FILESEL_H
#define _FILESEL_H

#define TYPE_SD       0

#ifdef HW_RVL
#define TYPE_USB      1
#define TYPE_IDE      2
#else
#define TYPE_IDE      1
#define TYPE_WKF      2
#endif

extern void OpenDVD ();
extern int OpenSD (int type);
#endif 
