dnl
dnl $KTH: osf-func-vfs-name-hash.m4,v 1.1.2.1 2001/05/28 23:22:48 mattiasa Exp $
dnl

AC_DEFUN(AC_OSF_FUNC_VFS_NAME_HASH, [

AC_CACHE_CHECK(if vfs_name_hash takes four arguments,
ac_cv_func_vfs_name_hash_four_args,
AC_TRY_COMPILE_KERNEL([
#if defined(__osf__) && defined(__GNUC__)
#define asm __foo_asm
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/vfs_proto.h>
#include <vm/vm_ubc.h>
], [vfs_name_hash(NULL, NULL, NULL, NULL)],
ac_cv_func_vfs_name_hash_four_args=yes,
ac_cv_func_vfs_name_hash_four_args=no))
if test "$ac_cv_func_vfs_name_hash_four_args" = yes; then
	AC_DEFINE(HAVE_FOUR_ARGUMENT_VFS_NAME_HASH, 1,
	[define if vfs_name_hash takes four arguments])
fi

AC_CACHE_CHECK(if vfs_name_hash takes three arguments,
ac_cv_func_vfs_name_hash_three_args,
AC_TRY_COMPILE_KERNEL([
#if defined(__osf__) && defined(__GNUC__)
#define asm __foo_asm
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/vfs_proto.h>
#include <vm/vm_ubc.h>
], [vfs_name_hash(NULL, NULL, NULL)],
ac_cv_func_vfs_name_hash_three_args=yes,
ac_cv_func_vfs_name_hash_three_args=no))
if test "$ac_cv_func_vfs_name_hash_three_args" = yes; then
	AC_DEFINE(HAVE_THREE_ARGUMENT_VFS_NAME_HASH, 1,
	[define if vfs_name_hash takes three arguments])
fi

])
