dnl
dnl $Id: check-kernel-var.m4,v 1.1 2000/09/11 14:40:46 art Exp $
dnl

dnl AC_CHECK_KERNEL_VAR(var, type, [includes])
AC_DEFUN(AC_CHECK_KERNEL_VAR,
AC_CHECK_KERNEL($1, ac_cv_kernel_var_$1, [extern $2 $1; return $1], $4)
: << END
@@@funcs="$funcs kernel_$1"@@@
END
)
