dnl $OpenBSD: weird,name.m4,v 1.1 2001/10/10 11:16:28 espie Exp $
dnl trip up m4 if it forgets to quote filenames
define(`A', `$2')dnl
A(__file__)dnl
