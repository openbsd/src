dnl $OpenBSD: redef2.m4,v 1.1 2014/12/07 14:32:34 jsg Exp $
dnl recursive macro redefinition
define(`A', `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')
A(
	define(`A', `bbbbbbbbbbbbbbbbbbb')
)
