dnl
dnl $KTH: bsd-vfs-object-create.m4,v 1.1 1999/05/15 22:45:19 assar Exp $
dnl

dnl
dnl check for number of arguments to vfs_object_create
dnl

AC_DEFUN(AC_BSD_FUNC_VFS_OBJECT_CREATE, [
AC_CHECK_KERNEL_FUNCS(vfs_object_create)
if test "$ac_cv_kernel_func_vfs_object_create" = "yes"; then
AC_CACHE_CHECK(if vfs_object_create takes four arguments,
ac_cv_func_vfs_object_create_four_args,
AC_TRY_COMPILE_KERNEL([
#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
],
[vfs_object_create(0, 0, 0, 0);],
ac_cv_func_vfs_object_create_four_args=yes,
ac_cv_func_vfs_object_create_four_args=no))
if test "$ac_cv_func_vfs_object_create_four_args" = "yes"; then
	ac_foo=
	AC_DEFINE(HAVE_FOUR_ARGUMENT_VFS_OBJECT_CREATE, 1,
	[if vfs_object_create takes four arguments])
fi
fi
])
