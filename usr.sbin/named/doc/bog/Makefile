#	$OpenBSD: Makefile,v 1.1 1998/05/22 04:51:31 millert Exp $

DIR=    smm/10.named
MACROS= -me
SRCS= 00macs.me 00title.me intro.me ns.me types.me files.me \
        named.boot.primary named.boot.secondary named.boot.cache \
        resolv.conf root.cache named.local ucbhosts.rev ucbhosts \
        setup.me manage.me build.me ack.me

all: bog.ps

bog.ps: ${SRCS}
	rm -f ${.TARGET}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
