dnl $OpenBSD: m4wrap2.m4,v 1.1 2001/10/06 10:59:11 espie Exp $
dnl another wrap test, to check that nothing adds bogus EOF
m4wrap(`m4wrap_string')dnl
define(m4wrap_string,`')dnl
