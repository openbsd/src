dnl $KTH: krb-prog-ranlib.m4,v 1.1 1999/05/15 22:45:30 assar Exp $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN(AC_KRB_PROG_RANLIB,
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
