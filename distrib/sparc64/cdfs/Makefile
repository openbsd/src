#	$OpenBSD: Makefile,v 1.5 2003/03/02 03:27:24 brad Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDFS=	cd${REV}.fs

all: ${CDFS}

${CDFS}:
	-rm -rf ${.OBJDIR}/cd-dir
	-@mkdir ${.OBJDIR}/cd-dir
	mkhybrid -o ${.OBJDIR}/${CDFS} ${.OBJDIR}/cd-dir
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDFS}
	disklabel -w svnd0 fakecdrom "CD-ROM OpenBSD"
	vnconfig -u svnd0
	mksuncd f ${.OBJDIR}/${CDFS} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

install:
	cp ${CDFS} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDFS}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
