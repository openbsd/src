dnl
dnl $KTH: check-kernel-vop-t.m4,v 1.1 1999/05/15 22:45:22 assar Exp $
dnl

dnl
dnl try to find out if we have a vop_t
dnl

AC_DEFUN(AC_CHECK_KERNEL_VOP_T, [

AC_CACHE_CHECK(for vop_t, ac_cv_type_vop_t,
AC_EGREP_HEADER(vop_t, sys/vnode.h, ac_cv_type_vop_t=yes, ac_cv_type_vop_t=no))

if test "$ac_cv_type_vop_t" = "yes"; then
	AC_DEFINE(HAVE_VOP_T, 1, [define if you have a vop_t])
fi
])
