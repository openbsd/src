dnl
dnl $KTH: linux-d_delete-void.m4,v 1.1.2.1 2002/01/31 10:23:22 lha Exp $
dnl

AC_DEFUN(AC_LINUX_D_DELETE_VOID, [
AC_CACHE_CHECK(if d_delete in struct dentry_operations returns void,
ac_cv_member_dentry_operations_d_delete_void,
AC_TRY_COMPILE_KERNEL([#include <asm/current.h>
#include <linux/fs.h>
#include <linux/dcache.h>],
[
struct dentry_operations foo_operations;

int hata_foo(void)
{
  return foo_operations.d_delete(0);
}
],
ac_cv_member_dentry_operations_d_delete_void=no,
ac_cv_member_dentry_operations_d_delete_void=yes))

if test "$ac_cv_member_dentry_operations_d_delete_void" = "yes"; then
  AC_DEFINE(HAVE_D_DELETE_VOID, 1,
	[define if d_delete in struct dentry_operations returns void])
fi
])
