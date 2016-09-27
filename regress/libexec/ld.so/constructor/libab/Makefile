# $OpenBSD: Makefile,v 1.3 2016/09/27 18:14:22 kettenis Exp $

LIB=ab
SRCS= ab.C
NOPROFILE=yes
CPPFLAGS=-I${.CURDIR}/../libaa
LDADD=-L../libaa
LDADD+=-laa

regress: all

.include <bsd.lib.mk>
