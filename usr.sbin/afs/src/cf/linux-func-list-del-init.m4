dnl
dnl $KTH: linux-func-list-del-init.m4,v 1.1.2.1 2002/01/31 13:03:16 lha Exp $
dnl

AC_DEFUN(AC_LINUX_FUNC_LIST_DEL_INIT, [
AC_CACHE_CHECK([for list_del_init],
ac_cv_func_list_del_init, [
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $test_KERNEL_CFLAGS $KERNEL_CPPFLAGS"
AC_EGREP_CPP([list_del_init],
[#include <linux/list.h>],
ac_cv_func_list_del_init=yes,
ac_cv_func_list_del_init=no)]
CPPFLAGS="$save_CPPFLAGS"
)
if test "$ac_cv_func_list_del_init" = "yes"; then
  AC_DEFINE(HAVE_LIST_DEL_INIT, 1,
	[define if you have a function list_del_init])
fi
])
