/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: DLLMain.c,v 1.3.2.1 2001/09/05 00:38:08 gson Exp $ */

#include <windows.h>
#include <stdio.h>

BOOL InitSockets(void);
 
/*
 * Called when we enter the DLL
 */
__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinstDLL,
					  DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) 
	{ 
	/*
	 * The DLL is loading due to process 
	 * initialization or a call to LoadLibrary. 
	 */
	case DLL_PROCESS_ATTACH: 
		if (!InitSockets())
			return (FALSE);
		break; 
 
	/* The attached process creates a new thread.  */
	case DLL_THREAD_ATTACH: 
		break; 
 
	/* The thread of the attached process terminates. */
	case DLL_THREAD_DETACH: 
		break; 
 
	/*
	 * The DLL is unloading from a process due to 
	 * process termination or a call to FreeLibrary. 
	 */
	case DLL_PROCESS_DETACH: 
		break; 

	default: 
		break; 
	} 
	return (TRUE);
}

