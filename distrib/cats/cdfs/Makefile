#	$OpenBSD: Makefile,v 1.2 2004/11/11 12:47:42 deraadt Exp $

TOP=	${.CURDIR}/..

CDROM=	cd${OSrev}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/cats
	cp ${.OBJDIR}/../ramdisk/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/cats
	ln ${.OBJDIR}/cd-dir/${OSREV}/cats/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} cats bootonly CD" \
	    -P "Copyright (c) 2004 Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/cats   ${OSREV} boot-only CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log

install:
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir log

unconfig:
	echo nothing to do

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
