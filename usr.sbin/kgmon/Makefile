#	$OpenBSD: Makefile,v 1.4 2002/06/17 07:07:45 deraadt Exp $

PROG=	kgmon
MAN=	kgmon.8
DPADD=	${LIBKVM}
LDADD=	-lkvm

#
# This program may safely be run setuid-root to allow non-root
# users to start, stop, and reset profiling buffers.
# However, we choose not to do so.
#
#BINOWN=root
#BINMODE=4555

.include <bsd.prog.mk>
