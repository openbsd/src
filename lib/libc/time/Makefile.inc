#	$OpenBSD: Makefile.inc,v 1.4 1998/02/04 22:22:42 deraadt Exp $

.PATH: ${.CURDIR}/time

SRCS+=	asctime.c difftime.c localtime.c strftime.c strptime.c
MAN+=	ctime.3 strftime.3 time2posix.3 tzfile.5 tzset.3 strptime.3


MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=tzset.3 tzsetwall.3
