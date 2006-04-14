dnl $KTH: win32.m4,v 1.1 2004/04/13 14:29:47 lha Exp $
dnl rk_WIN32_EXPORT buildsymbol symbol-that-export
AC_DEFUN([rk_WIN32_EXPORT],[AH_TOP([#ifdef $1
#ifndef $2
#ifdef _WIN32_
#define $2 _export _stdcall
#else
#define $2
#endif
#endif
#endif
])])
