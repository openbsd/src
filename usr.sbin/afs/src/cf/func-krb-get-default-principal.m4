dnl
dnl $KTH: func-krb-get-default-principal.m4,v 1.1.2.1 2001/02/02 01:26:07 assar Exp $
dnl

dnl
dnl Check for krb_get_default_principal
dnl

AC_DEFUN(AC_FUNC_KRB_GET_DEFAULT_PRINCIPAL, [

AC_CACHE_CHECK(for krb_get_default_principal, ac_cv_func_krb_get_default_principal, [
if test "$ac_cv_found_krb4" = "yes"; then
save_CPPFLAGS="${CPPFLAGS}"
save_LIBS="${LIBS}"
CPPFLAGS="${KRB4_INC_FLAGS} ${CPPFLAGS}"
LIBS="${KRB4_LIB_FLAGS} ${LIBS}"
AC_TRY_LINK([#include <krb.h>],
[krb_get_default_principal(0, 0, 0);],
ac_cv_func_krb_get_default_principal=yes,
ac_cv_func_krb_get_default_principal=no)
CPPFLAGS="${save_CPPFLAGS}"
LIBS="${save_LIBS}"
else
ac_cv_func_krb_get_default_principal=no
fi
])
if test "$ac_cv_func_krb_get_default_principal" = "yes"; then
  AC_DEFINE(HAVE_KRB_GET_DEFAULT_PRINCIPAL, 1, [define if you have krb_get_default_principal])
fi
])
