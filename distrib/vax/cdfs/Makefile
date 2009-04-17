#	$OpenBSD: Makefile,v 1.2 2009/04/17 03:58:55 deraadt Exp $

TOP=    ${.CURDIR}/..

.include "${TOP}/Makefile.inc"

FLOPPY=	floppy${REV}.fs
CDROM=	cd${REV}.iso

# The floppy image works fine as a CD image, too

.ifdef RELEASEDIR
install:
	cp ${RELEASEDIR}/${FLOPPY} ${RELEASEDIR}/${CDROM}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
