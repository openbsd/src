dnl
dnl $KTH: prog-cc-flags.m4,v 1.2 2000/01/23 12:34:11 assar Exp $
dnl

AC_DEFUN(AC_PROG_CC_FLAGS, [
AC_REQUIRE([AC_PROG_CC])dnl
AC_MSG_CHECKING(for $CC warning options)
if test "$GCC" = "yes"; then
  extra_flags="-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs"
  CFLAGS="$CFLAGS $extra_flags"
  AC_MSG_RESULT($extra_flags)
else
  AC_MSG_RESULT(none)
fi
])
