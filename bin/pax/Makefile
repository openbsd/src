#	$OpenBSD: Makefile,v 1.10 2001/05/26 00:32:20 millert Exp $

# To install on versions prior to BSD 4.4 the following may have to be
# defined with CFLAGS +=
#
# -DLONG_OFF_T	Define this if the base type of an off_t is a long (and is
#		NOT a quad).  (This is often defined in the file
#		/usr/include/sys/types.h).
# 		This define is important, as if you do have a quad_t
# 		off_t and define LONG_OFF_T, pax will compile but will
# 		NOT RUN PROPERLY.
#

PROG=   pax
SRCS=	ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c\
	gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
	tar.c tty_subs.c
MAN=	pax.1 tar.1 cpio.1
LINKS=	${BINDIR}/pax ${BINDIR}/tar ${BINDIR}/pax ${BINDIR}/cpio

.include <bsd.prog.mk>
