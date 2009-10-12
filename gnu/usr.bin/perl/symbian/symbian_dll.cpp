/*
 *	symbian_dll.cpp
 *
 *	Copyright (c) Nokia 2004-2005.  All rights reserved.
 *      This code is licensed under the same terms as Perl itself.
 *
 */

#define SYMBIAN_DLL_CPP
#include <e32base.h>
#include "PerlBase.h"

#ifdef __SERIES60_3X__ 
EXPORT_C GLDEF_C TInt E32Dll(/*TDllReason aReason*/) { return KErrNone; }
#else
EXPORT_C GLDEF_C TInt E32Dll(TDllReason /*aReason*/) { return KErrNone; }
#endif

extern "C" {
    EXPORT_C void* symbian_get_vars(void)	   { return Dll::Tls(); }
    EXPORT_C void  symbian_set_vars(const void *p) { Dll::SetTls((TAny*)p); }
    EXPORT_C void  symbian_unset_vars(void)	   { Dll::SetTls(0); }
}

