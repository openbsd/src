dnl $KTH: kernel-need-proto.m4,v 1.1.2.1 2001/02/14 12:55:08 lha Exp $
dnl
dnl
dnl Check if we need the prototype for a function in kernel-space
dnl

dnl AC_KERNEL_NEED_PROTO(includes, function)

AC_DEFUN(AC_KERNEL_NEED_PROTO, [
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $test_KERNEL_CFLAGS $KERNEL_CPPFLAGS"
AC_NEED_PROTO([$1],[$2])
CFLAGS="$save_CFLAGS"
])
