#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:48:23 deraadt Exp $

PROG=	banner

SRCS=	$(PROG).c chset.c

# if you want to make chars from the letter involved (like lpd), then
# un-comment the following.
#CFLAGS+= -DNOHASH_ONLY

# if you want to use the same char set that lpd uses then un-comment
# the following.  Lpd uses a much nicer character set but banner
# output will not be the same as the real thing.
#CFLAGS+= -DLPD_CHSET

.include <bsd.prog.mk>

