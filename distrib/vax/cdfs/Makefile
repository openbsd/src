#	$OpenBSD: Makefile,v 1.1 2008/02/25 19:52:05 deraadt Exp $

TOP=    ${.CURDIR}/..

.include "${TOP}/Makefile.inc"

FLOPPY=	floppy${REV}.fs
CDROM=	cd${REV}.iso

# The floppy image works fine as a CD image, too

install:
	cp ${DESTDIR}/snapshot/${FLOPPY} ${DESTDIR}/snapshot/${CDROM}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
