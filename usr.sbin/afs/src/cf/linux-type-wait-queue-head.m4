dnl
dnl $Id: linux-type-wait-queue-head.m4,v 1.1 2000/09/11 14:40:50 art Exp $
dnl

AC_DEFUN(AC_LINUX_TYPE_WAIT_QUEUE_HEAD_T, [
AC_CACHE_CHECK([for wait_queue_head_t],
ac_cv_type_wait_queue_head_t,[
AC_TRY_COMPILE_KERNEL([#include <linux/wait.h>],
[wait_queue_head_t foo;],
ac_cv_type_wait_queue_head_t=yes,
ac_cv_type_wait_queue_head_t=no)])
if test "$ac_cv_type_wait_queue_head_t" = "yes"; then
  AC_DEFINE(HAVE_WAIT_QUEUE_HEAD_T, 1,
	[define if you have a wait_queue_head_t])
fi
])
