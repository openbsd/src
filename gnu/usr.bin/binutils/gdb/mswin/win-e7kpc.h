

#ifndef _WINE7KPC_H_
#define _WINE7KPC_H_

#define KERNEL
 #define STRICT

#include "windefs.h"

 // __D000h is an absolute value; by declaring it as a NEAR variable
 // in our data segment we can take its "address" and get the
 // 16-bit absolute value.

#if _WIN32
 #define DllExport __declspec(dllexport)
 DllExport short CALLBACK set_mem (short offset, short mem);
 DllExport short CALLBACK get_mem (short offset);
 int win_load_e7kpc (void);
 void win_unload_e7kpc (void);
 void win_mem_put (char *buf, int len, int offset);
 void win_mem_get (char *buf, int len, int offset);
#else
 typedef WORD SELECTOR;
 extern BYTE NEAR CDECL _D000h;
 SELECTOR selVGA = (SELECTOR) &_D000h;
 short CALLBACK set_mem (short offset, short mem);
 short CALLBACK get_mem (short offset);
#endif

#endif /* _WINE7KPC_H_ */
