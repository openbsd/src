dnl
dnl $KTH: func-hstrerror-const.m4,v 1.2 1999/05/28 13:18:04 assar Exp $
dnl
dnl Test if hstrerror wants const or not
dnl

dnl AC_FUNC_HSTRERROR_CONST(includes, function)

AC_DEFUN(AC_FUNC_HSTRERROR_CONST, [
AC_CACHE_CHECK([if hstrerror needs const], ac_cv_func_hstrerror_const,
AC_TRY_COMPILE([netdb.h],
[const char *hstrerror(int);],
ac_cv_func_hstrerror_const=no,
ac_cv_func_hstrerror_const=yes))
if test "$ac_cv_func_hstrerror_const" = "yes"; then
	AC_DEFINE(NEED_HSTRERROR_CONST, 1, [define if hstrerror is const])
fi
])
