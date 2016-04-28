#	$OpenBSD: Makefile.inc,v 1.15 2016/04/28 19:53:20 kettenis Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c
SRCS+=	infinity.c ldexp.c modf.c nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
