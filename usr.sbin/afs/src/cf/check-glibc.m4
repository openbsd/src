dnl
dnl $Id: check-glibc.m4,v 1.1 2000/09/11 14:40:45 art Exp $
dnl
dnl
dnl test for GNU libc
dnl

AC_DEFUN(AC_CHECK_GLIBC,[
AC_CACHE_CHECK([for glibc],ac_cv_libc_glibc,[
AC_EGREP_CPP(yes,
[#include <features.h>
#ifdef __GLIBC__
yes
#endif
],
eval "ac_cv_libc_glibc=yes",
eval "ac_cv_libc_glibc=no")])
if test "$ac_cv_libc_glibc" = "yes";then
	AC_DEFINE(HAVE_GLIBC, 1,
	[define if you have a glibc-based system])
fi
])
