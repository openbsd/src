
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#ifdef __MFC4__
    #define VC_EXTRALEAN	/* FIXME: newer mfc samples have this... */
			/* FIXME: see database\daoctl\accspict,advanced\cube */
#endif
#include <limits.h> 
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcoll.h>
#include <setjmp.h>
#include "gdbwin.h"
#include "mini.h"
#include "resource.h"
#include "gui.h"
#include "fontinfo.h"
#ifdef __MFC4__
    //#include "waitcur.h"	/* FIXME: removed - causes class redef errors */
#else
    #include "waitcur.h"
#endif
#include "iface.h"

