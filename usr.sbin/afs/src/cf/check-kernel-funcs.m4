dnl
dnl $KTH: check-kernel-funcs.m4,v 1.2 1999/08/09 23:02:45 assar Exp $
dnl

dnl AC_CHECK_KERNEL_FUNCS(functions...)
AC_DEFUN(AC_CHECK_KERNEL_FUNCS,
[for ac_func in $1
do
AC_CHECK_KERNEL($ac_func, ac_cv_kernel_func_$ac_func, [$ac_func]())
done
]
: << END
@@@funcs="$funcs [patsubst([$1], [\w+], [kernel_\&])]"@@@
END
)
