#	$OpenBSD: Makefile,v 1.2 1996/03/28 23:21:53 niklas Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:22:12 jtk Exp $

PROG=	telnetd
CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR}
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERM}
LDADD+=	-lutil -ltermcap -ltelnet
MAN=	telnetd.8


# These are the sources that have encryption stuff in them.
CRYPT_SRC= authenc.c ext.h state.c telnetd.c termstat.c
CRYPT_SRC+= utility.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.prog.mk>

nocrypt:
	@echo "Encryption code already removed."
