dnl $Id: broken-glob.m4,v 1.1 2000/09/11 14:40:44 art Exp $
dnl
dnl check for glob(3)
dnl
AC_DEFUN(AC_BROKEN_GLOB,[
AC_CACHE_CHECK(for working glob, ac_cv_func_glob_working,
ac_cv_func_glob_working=yes
AC_TRY_LINK([
#include <stdio.h>
#include <glob.h>],[
glob(NULL, GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE, NULL, NULL);
],:,ac_cv_func_glob_working=no,:))

if test "$ac_cv_func_glob_working" = yes; then
	AC_DEFINE(HAVE_GLOB, 1, [define if you have a glob() that groks 
	GLOB_BRACE, GLOB_NOCHECK, GLOB_QUOTE, and GLOB_TILDE])
fi
if test "$ac_cv_func_glob_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>
#include <glob.h>],glob)
fi
])
