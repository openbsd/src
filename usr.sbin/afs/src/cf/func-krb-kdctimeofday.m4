dnl
dnl $KTH: func-krb-kdctimeofday.m4,v 1.4 2000/05/23 07:21:24 assar Exp $
dnl

dnl
dnl Check for krb_kdctimeofday
dnl

AC_DEFUN(AC_FUNC_KRB_KDCTIMEOFDAY, [

AC_CACHE_CHECK(for krb_kdctimeofday, ac_cv_func_krb_kdctimeofday, [
if test "$ac_cv_found_krb4" = "yes"; then
save_CPPFLAGS="${CPPFLAGS}"
save_LIBS="${LIBS}"
CPPFLAGS="${KRB4_INC_FLAGS} ${CPPFLAGS}"
LIBS="${KRB4_LIB_FLAGS} ${LIBS}"
AC_TRY_LINK([#include <krb.h>],
[krb_kdctimeofday(0);],
ac_cv_func_krb_kdctimeofday=yes,
ac_cv_func_krb_kdctimeofday=no)
CPPFLAGS="${save_CPPFLAGS}"
LIBS="${save_LIBS}"
else
dnl Make it say no
eval "ac_cv_func_krb_kdctimeofday=no"
fi
])
if test "$ac_cv_func_krb_kdctimeofday" = "yes"; then
  AC_DEFINE(HAVE_KRB_KDCTIMEOFDAY, 1, [define if you have krb_kdctimeofday])
fi
])
