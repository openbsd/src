dnl
dnl $KTH: type-krb-principal.m4,v 1.3 2000/01/23 13:01:42 lha Exp $
dnl

dnl
dnl Check for struct krb_principal
dnl

AC_DEFUN(AC_TYPE_KRB_PRINCIPAL, [

AC_CACHE_CHECK(for krb_principal, ac_cv_struct_krb_principal, [
if test "$ac_cv_found_krb4" = "yes"; then
save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${KRB4_INC_FLAGS} $CPPFLAGS"
AC_EGREP_HEADER(krb_principal, krb.h,ac_cv_struct_krb_principal=yes)
CPPFLAGS="${save_CPPFLAGS}"
eval "ac_cv_struct_krb_principal=${ac_cv_struct_krb_principal-no}"
else
dnl Gross hack to avoid struct krb_principal get defined when we don't have krb
eval "ac_cv_struct_krb_principal=no"
fi
])
if test "$ac_cv_struct_krb_principal" = "yes"; then
  AC_DEFINE(HAVE_KRB_PRINCIPAL, 1, [define if you have a struct krb_principal])
fi
])
