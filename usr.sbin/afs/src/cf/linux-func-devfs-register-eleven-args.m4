dnl
dnl $KTH: linux-func-devfs-register-eleven-args.m4,v 1.1 1999/11/03 01:09:04 mackan Exp $
dnl

AC_DEFUN(AC_LINUX_FUNC_DEVFS_REGISTER_ELEVEN_ARGS, [

AC_CACHE_CHECK(if devfs_register takes the dir argument,
ac_cv_func_devfs_register_eleven_args,
AC_TRY_COMPILE_KERNEL([#include <linux/devfs_fs_kernel.h>
],[devfs_handle_t de;
devfs_register(de, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
],
ac_cv_func_devfs_register_eleven_args=yes,
ac_cv_func_devfs_register_eleven_args=no))
if test "$ac_cv_func_devfs_register_eleven_args" = "yes"; then
  AC_DEFINE(HAVE_DEVFS_REGISTER_ELEVEN_ARGS, 1,
	[define if devfs_register takes eleven arguments])
fi
])
