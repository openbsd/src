#	$OpenBSD: Makefile.inc,v 1.10 2016/03/30 06:38:42 jmc Exp $

.PATH: ${LIBCSRCDIR}/time

SRCS+=	asctime.c difftime.c localtime.c strftime.c strptime.c wcsftime.c
MAN+=	ctime.3 strftime.3 time2posix.3 tzfile.5 tzset.3 strptime.3 wcsftime.3
