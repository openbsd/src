dnl $KTH: header-dirent-dir-h.m4,v 1.1 2000/03/24 04:07:32 assar Exp $

dnl
dnl Check if we can include both dirent.h and sys/dir.h
dnl

AC_DEFUN(AC_DIRENT_SYS_DIR_H, [
AC_CACHE_CHECK(if we can include both dirent.h and sys/dir.h,
ac_cv_header_dirent_and_sys_dir,
AC_TRY_CPP([
#include <sys/types.h>
#include <dirent.h>
#include <sys/dir.h>
],
ac_cv_header_dirent_and_sys_dir=yes,
ac_cv_header_dirent_and_sys_dir=no))
if test "$ac_cv_header_dirent_and_sys_dir" = yes; then
	AC_DEFINE(DIRENT_AND_SYS_DIR_H, 1,
	[define if you can include both dirent.h and sys/dir.h])
fi
])
