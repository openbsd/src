#	$OpenBSD: Makefile,v 1.2 2002/02/17 05:33:33 art Exp $

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

short_time: nanosleep
	./nanosleep -S

REGRESSTARGETS=trivial with_signal time_elapsed time_elapsed_with_signal
REGRESSTARGETS+=short_time

.include <bsd.regress.mk>
