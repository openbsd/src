dnl
dnl $KTH: bsd-vfs-busy.m4,v 1.3 2000/02/20 03:06:43 assar Exp $
dnl

dnl
dnl try to find out if vfs_busy takes two/three/four arguments
dnl

AC_DEFUN(AC_BSD_FUNC_VFS_BUSY,[
AC_CACHE_CHECK(if vfs_busy takes two arguments, ac_cv_func_vfs_busy_two_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ucred.h>
#ifdef HAVE_SYS_MODULE_H
#include <sys/module.h>
#endif
#include <sys/mount.h>],[vfs_busy(0, 0)],
ac_cv_func_vfs_busy_two_args=yes,
ac_cv_func_vfs_busy_two_args=no))
if test "$ac_cv_func_vfs_busy_two_args" = yes; then
	AC_DEFINE(HAVE_TWO_ARGUMENT_VFS_BUSY, 1,
	[define if vfs_busy takes two arguments])
fi

AC_CACHE_CHECK(if vfs_busy takes three arguments, ac_cv_func_vfs_busy_three_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ucred.h>
#ifdef HAVE_SYS_MODULE_H
#include <sys/module.h>
#endif
#include <sys/mount.h>],[vfs_busy(0, 0, 0)],
ac_cv_func_vfs_busy_three_args=yes,
ac_cv_func_vfs_busy_three_args=no))
if test "$ac_cv_func_vfs_busy_three_args" = yes; then
	AC_DEFINE(HAVE_THREE_ARGUMENT_VFS_BUSY, 1,
	[define if vfs_busy takes three arguments])
fi

AC_CACHE_CHECK(if vfs_busy takes four arguments, ac_cv_func_vfs_busy_four_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ucred.h>
#ifdef HAVE_SYS_MODULE_H
#include <sys/module.h>
#endif
#include <sys/mount.h>],[vfs_busy(0, 0, 0, 0)],
ac_cv_func_vfs_busy_four_args=yes,
ac_cv_func_vfs_busy_four_args=no))
if test "$ac_cv_func_vfs_busy_four_args" = yes; then
	AC_DEFINE(HAVE_FOUR_ARGUMENT_VFS_BUSY, 1,
	[define if vfs_busy takes four arguments])
fi
])
