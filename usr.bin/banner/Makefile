#	$NetBSD: Makefile,v 1.1.1.1 1995/04/09 05:53:04 cgd Exp $

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

