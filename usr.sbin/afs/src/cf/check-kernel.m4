dnl
dnl $KTH: check-kernel.m4,v 1.6.2.2 2001/04/27 11:43:40 ahltorp Exp $
dnl

dnl there are two different heuristics for doing the kernel tests
dnl a) running nm and greping the output
dnl b) trying linking against the kernel

dnl AC_CHECK_KERNEL(name, cv, magic, [includes])
AC_DEFUN(AC_CHECK_KERNEL,
[AC_MSG_CHECKING([for $1 in kernel])
AC_CACHE_VAL($2,
[
if expr "$target_os" : "darwin" > /dev/null 2>&1; then
  if nm $KERNEL | egrep "\\<_?$1\\>" >/dev/null 2>&1; then
    eval "$2=yes"
  else
    eval "$2=no"
  fi
elif expr "$target_os" : "osf" >/dev/null 2>&1; then
  if nm  $KERNEL | egrep "^$1 " > /dev/null 2>&1; then
    eval "$2=yes"
  else
    eval "$2=no"
  fi
else
cat > conftest.$ac_ext <<EOF
dnl This sometimes fails to find confdefs.h, for some reason.
dnl [#]line __oline__ "[$]0"
[#]line __oline__ "configure"
#include "confdefs.h"
[$4]
int _foo() {
return foo();
}
int foo() {
[$3];
return 0; }
EOF
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $test_KERNEL_CFLAGS $KERNEL_CPPFLAGS"
if AC_TRY_EVAL(ac_compile) && AC_TRY_EVAL(ac_kernel_ld) && test -s conftest; then
  eval "$2=yes"
else
  eval "$2=no"
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
fi
CFLAGS="$save_CFLAGS"
rm -f conftest*
fi])
changequote(, )dnl
eval "ac_tr_var=HAVE_KERNEL_`echo $1 | tr '[a-z]' '[A-Z]'`"
changequote([, ])dnl

AC_MSG_RESULT(`eval echo \\${$2}`)
if test `eval echo \\${$2}` = yes; then
  AC_DEFINE_UNQUOTED($ac_tr_var, 1)
fi
])

dnl define([foo], [[HAVE_KERNEL_]translit($1, [a-z], [A-Z])])
dnl : << END
dnl @@@syms="$syms foo"@@@
dnl END
dnl undefine([foo])
