#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:27:59 tholo Exp $

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
