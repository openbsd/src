dnl
dnl $KTH: linux-func-init-wait-queue-head.m4,v 1.2 2000/09/25 09:13:56 lha Exp $
dnl

AC_DEFUN(AC_LINUX_FUNC_INIT_WAITQUEUE_HEAD, [
AC_CACHE_CHECK([for init_waitqueue_head],
ac_cv_func_init_waitqueue_head,[
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_LINUX_STDDEF_H
#include <linux/stddef.h>
#endif
#include <linux/wait.h>],
[wait_queue_head_t foo;
init_waitqueue_head(&foo)],
ac_cv_func_init_waitqueue_head=yes,
ac_cv_func_init_waitqueue_head=no)])
if test "$ac_cv_func_init_waitqueue_head" = "yes"; then
  AC_DEFINE(HAVE_INIT_WAITQUEUE_HEAD, 1,
	[define if you have a init_waitqueue_head])
fi
])
