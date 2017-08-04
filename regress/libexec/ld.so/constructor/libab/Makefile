# $OpenBSD: Makefile,v 1.4 2017/08/04 18:23:52 kettenis Exp $

LIB=ab
SRCS= ab.C
NOPROFILE=yes
CPPFLAGS=-I${.CURDIR}/../libaa
LDADD=-L../libaa
LDADD+=-laa

CC=${CXX}

regress: all

.include <bsd.lib.mk>
