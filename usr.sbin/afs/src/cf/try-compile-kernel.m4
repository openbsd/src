dnl
dnl $KTH: try-compile-kernel.m4,v 1.2.12.1 2001/05/28 23:22:51 mattiasa Exp $
dnl

AC_DEFUN(AC_TRY_COMPILE_KERNEL,[
save_CFLAGS="$CFLAGS"
save_CC="$CC"
if test "X${KERNEL_CC}" != "X"; then
  CC="$KERNEL_CC"
fi
CFLAGS="$CFLAGS $test_KERNEL_CFLAGS $KERNEL_CPPFLAGS"
AC_TRY_COMPILE([$1], [$2], [$3], [$4])
CFLAGS="$save_CFLAGS"
CC="$save_CC"
])
