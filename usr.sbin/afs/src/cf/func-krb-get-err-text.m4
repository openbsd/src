dnl
dnl $KTH: func-krb-get-err-text.m4,v 1.3 2000/01/23 13:01:51 lha Exp $
dnl

dnl
dnl Check for krb_get_err_text
dnl

AC_DEFUN(AC_FUNC_KRB_GET_ERR_TEXT, [

AC_CACHE_CHECK(for krb_get_err_text, ac_cv_func_krb_get_err_text, [
if test "$ac_cv_found_krb4" = "yes"; then
save_CPPFLAGS="${CPPFLAGS}"
save_LIBS="${LIBS}"
CPPFLAGS="${KRB4_INC_FLAGS} ${CPPFLAGS}"
LIBS="${KRB4_LIB_FLAGS} ${LIBS}"
AC_TRY_LINK([#include <krb.h>],
[krb_get_err_text(0);],
ac_cv_func_krb_get_err_text=yes,
ac_cv_func_krb_get_err_text=no)
CPPFLAGS="${save_CPPFLAGS}"
LIBS="${save_LIBS}"
fi
])
if test "$ac_cv_func_krb_get_err_text" = "yes"; then
  AC_DEFINE(HAVE_KRB_GET_ERR_TEXT, 1, [define if you have krb_get_err_text])
fi
])
