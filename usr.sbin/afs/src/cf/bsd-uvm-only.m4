dnl
dnl $KTH: bsd-uvm-only.m4,v 1.2 2000/10/04 00:01:06 lha Exp $
dnl

AC_DEFUN(AC_CHECK_BSD_UVM_ONLY,[
AC_CACHE_CHECK(if we can only include uvm headers,
ac_cv_kernel_uvm_only,
AC_TRY_COMPILE_KERNEL([
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_VM_VM_H
#include <vm/vm.h>
#endif
#ifdef HAVE_VM_VM_EXTERN_H
#include <vm/vm_extern.h>
#endif
#ifdef HAVE_VM_VM_ZONE_H
#include <vm/vm_zone.h>
#endif
#ifdef HAVE_VM_VM_OBJECT_H
#include <vm/vm_object.h>
#endif
#ifdef HAVE_UVM_UVM_EXTERN_H
#include <uvm/uvm_extern.h>
#endif
], [int suvmtiuk = 1;
],
ac_cv_kernel_uvm_only=yes,
ac_cv_kernel_uvm_only=no))]
if test "$ac_cv_kernel_uvm_only" = no; then
AC_DEFINE(HAVE_KERNEL_UVM_ONLY, 1,
[define if we only can include uvm headers])
fi
)
