dnl
dnl $KTH: check-dirsiz.m4,v 1.1 1999/05/15 22:45:21 assar Exp $
dnl

dnl
dnl Check where DIRSIZ lives
dnl

AC_DEFUN(AC_CHECK_DIRSIZ, [

AC_CACHE_CHECK([if DIRSIZ lives in dirent.h], ac_cv_dirsiz_in_dirent,
AC_EGREP_CPP(yes,[#include <dirent.h>
#ifdef DIRSIZE
yes
#endif],
eval "ac_cv_dirsiz_in_dirent=yes",
eval "ac_cv_dirsiz_in_dirent=no"))
if test "$ac_cv_dirsiz_in_dirent" = "yes"; then
  AC_DEFINE(DIRSIZ_IN_DIRENT_H, 1, [define if DIRSIZ is defined in dirent.h])
fi

AC_CACHE_CHECK([if DIRSIZ lives in sys/dir.h], ac_cv_dirsiz_in_sys_dir,
AC_EGREP_CPP(yes,[#include <sys/dir.h>
#ifdef DIRSIZ
yes
#endif],
eval "ac_cv_dirsiz_in_sys_dir=yes",
eval "ac_cv_dirsiz_in_sys_dir=no"))
if test "$ac_cv_dirsiz_in_sys_dir" = "yes"; then
  AC_DEFINE(DIRSIZ_IN_SYS_DIR_H, 1, [define if DIRSIZ is defined in sys/dir.h])
fi

AC_CACHE_CHECK([if _GENERIC_DIRSIZ lives in sys/dirent.h],
ac_cv_generic_dirsiz_in_sys_dirent,
AC_EGREP_CPP(yes,[#include <sys/dirent.h>
#ifdef _GENERIC_DIRSIZ
yes
#endif],
eval "ac_cv_generic_dirsiz_in_sys_dirent=yes",
eval "ac_cv_generic_dirsiz_in_sys_dirent=no"))
if test "$ac_cv_generic_dirsiz_in_sys_dirent" = "yes"; then
  AC_DEFINE(GENERIC_DIRSIZ_IN_SYS_DIRENT_H, 1,
	[define if DIRSIZ is defined in sys/dirent.h])
fi
])
