#	$OpenBSD: Makefile,v 1.3 2002/09/02 20:01:44 avsm Exp $

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

REGRESS_TARGETS=trivial with_signal time_elapsed time_elapsed_with_signal
REGRESS_TARGETS+=short_time

.include <bsd.regress.mk>
