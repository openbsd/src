#	$OpenBSD: Makefile,v 1.1 2002/02/16 00:47:55 art Exp $

PROG=	nanosleep
SRCS=	nanosleep.c

trivial: nanosleep
	./nanosleep -t

with_signal: nanosleep
	./nanosleep -s

time_elapsed: nanosleep
	./nanosleep -e

time_elapsed_with_signal: nanosleep
	./nanosleep -E

REGRESSTARGETS=trivial with_signal time_elapsed time_elapsed_with_signal

.include <bsd.regress.mk>
