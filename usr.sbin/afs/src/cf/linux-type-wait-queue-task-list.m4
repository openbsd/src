dnl
dnl $KTH: linux-type-wait-queue-task-list.m4,v 1.1 2000/09/25 09:14:55 lha Exp $
dnl

AC_DEFUN(AC_LINUX_TYPE_WAIT_QUEUE_TASK_LIST, [
AC_CACHE_CHECK([for wait_queue_task_list],
ac_cv_type_wait_queue_task_list,[
AC_TRY_COMPILE_KERNEL([#include <linux/stddef.h>
#include <linux/wait.h>],
[wait_queue_head_t foo;
void *p;
p = &foo.task_list;],
ac_cv_type_wait_queue_task_list=yes,
ac_cv_type_wait_queue_task_list=no)])
if test "$ac_cv_type_wait_queue_task_list" = "yes"; then
  AC_DEFINE(HAVE_WAIT_QUEUE_TASK_LIST, 1,
	[define if you have a wait_queue_task_list])
fi
])
