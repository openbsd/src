dnl
dnl $KTH: have-kernel-struct-field.m4,v 1.3.22.1 2001/04/16 23:07:52 lha Exp $
dnl

dnl AC_HAVE_KERNEL_STRUCT_FIELD(includes, struct, type, field)
AC_DEFUN(AC_HAVE_KERNEL_STRUCT_FIELD, [
define(cache_val, translit(ac_cv_struct_$2_$4, [A-Z ], [a-z_]))
AC_CACHE_CHECK([if struct $2 has a field $4], cache_val, [
AC_TRY_COMPILE_KERNEL([$1],
[struct $2 foo; $3 bar = foo.$4; ],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_STRUCT_$2_$4, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if struct $2 has field $4])
	undefine([foo])
fi
undefine([cache_val])
])
