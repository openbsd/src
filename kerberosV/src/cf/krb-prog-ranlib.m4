dnl $Id: krb-prog-ranlib.m4,v 1.3 2013/06/17 18:57:40 robert Exp $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN([AC_KRB_PROG_RANLIB],
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
