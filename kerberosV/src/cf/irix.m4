dnl
dnl $Id: irix.m4,v 1.3 2013/06/17 18:57:40 robert Exp $
dnl

AC_DEFUN([rk_IRIX],
[
irix=no
case "$host" in
*-*-irix*) 
	irix=yes
	;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl

])
