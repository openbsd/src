dnl
dnl $KTH: check-kernel-func.m4,v 1.2 1999/08/09 23:02:01 assar Exp $
dnl

dnl AC_CHECK_KERNEL_FUNC(func, param, [includes])
AC_DEFUN(AC_CHECK_KERNEL_FUNC,
AC_CHECK_KERNEL($1, ac_cv_kernel_func_$1, [$1]([$2]), $4)
: << END
@@@funcs="$funcs [patsubst([$1], [\w+], [kernel_\&])]"@@@
END
)
