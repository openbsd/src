dnl
dnl $KTH: type-msghdr.m4,v 1.1 1999/05/15 22:45:35 assar Exp $
dnl

dnl
dnl Check for struct msghdr
dnl

AC_DEFUN(AC_TYPE_MSGHDR, [

AC_CACHE_CHECK(for struct msghdr, ac_cv_struct_msghdr, [
AC_EGREP_HEADER([struct msghdr], sys/socket.h,
ac_cv_struct_msghdr=yes,
ac_cv_struct_msghdr=no)
])
if test "$ac_cv_struct_msghdr" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_MSGHDR, 1, [define if you have struct msghdr])
fi
])
