dnl
dnl $KTH: check-kernel-var.m4,v 1.2 2000/01/25 20:31:24 assar Exp $
dnl

dnl AC_CHECK_KERNEL_VAR(var, type, [includes])
AC_DEFUN(AC_CHECK_KERNEL_VAR,
AC_CHECK_KERNEL($1, ac_cv_kernel_var_$1, [extern $2 $1; return $1], $4)
: << END
@@@funcs="$funcs kernel_$1"@@@
END
)
