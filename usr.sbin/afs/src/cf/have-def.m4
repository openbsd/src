dnl
dnl $KTH: have-def.m4,v 1.2.24.1 2001/04/16 23:07:50 lha Exp $
dnl

dnl AC_HAVE_DEF(includes, [struct|typedef])
AC_DEFUN(AC_HAVE_DEF, [
define(cache_val, translit(ac_cv_have_def_$2, [A-Z ], [a-z_]))
AC_CACHE_CHECK([if $2 exists], cache_val, [
AC_TRY_COMPILE([$1],
[$2 foo],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_DEF_$2, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if you have $2])
	undefine([foo])
fi
undefine([cache_val])
])
