#	$OpenBSD: Makefile.inc,v 1.3 1998/08/29 21:11:38 deraadt Exp $

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
