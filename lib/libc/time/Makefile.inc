#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:34:45 tholo Exp $

.PATH: ${.CURDIR}/time

SRCS+=	asctime.c difftime.c localtime.c
MAN+=	ctime.3 time2posix.3 tzfile.5 tzset.3


MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=tzset.3 tzsetwall.3
