#	$OpenBSD: Makefile,v 1.6 2005/03/19 00:28:04 deraadt Exp $

TOP=    ${.CURDIR}/..
IPK=	openbsd${OSrev}_arm.ipk

all: ${IPK} Packages

${IPK}:
	install -o ${BINOWN} -g ${BINGRP} -m 755 -d ${.OBJDIR}/data/hdd2/OpenBSD
	install -o ${BINOWN} -g ${BINGRP} -m 755 -d \
	    ${.OBJDIR}/data/opt/QtPalmtop/apps/Applications
	install -o ${BINOWN} -g ${BINGRP} -m 755 -d ${.OBJDIR}/data/opt/QtPalmtop/bin
	install -o ${BINOWN} -g ${BINGRP} -m 755 -d ${.OBJDIR}/data/opt/QtPalmtop/pics

	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.OBJDIR}/../ramdisk/bsd.rd \
	    ${.OBJDIR}/data/hdd2/OpenBSD
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/mdec/zboot \
	    ${.OBJDIR}/data/hdd2/OpenBSD
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/mdec/zbsdmod.o \
	    ${.OBJDIR}/data/hdd2/OpenBSD
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/rc.zboot \
	    ${.OBJDIR}/data/hdd2/OpenBSD

	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/bootbsd \
	    ${.OBJDIR}/data/opt/QtPalmtop/bin
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/bootbsd.desktop \
	    ${.OBJDIR}/data/opt/QtPalmtop/apps/Applications
	uudecode < ${.CURDIR}/bootbsd.png.uu
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.OBJDIR}/bootbsd.png \
	    ${.OBJDIR}/data/opt/QtPalmtop/pics
	cd ${.OBJDIR}/data && tar -czf ../data.tar.gz .
	install -o ${BINOWN} -g ${BINGRP} -m 755 -d ${.OBJDIR}/control

	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/controlfile \
	    ${.OBJDIR}/control/control;
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/postinst \
	    ${.OBJDIR}/control;
	install -o ${BINOWN} -g ${BINGRP} -m 755 ${.CURDIR}/postrm \
	    ${.OBJDIR}/control;

	cd ${.OBJDIR}/data/ && find . -type f | sed 's,^\.,,' > ../control/openbsd.list
	cd ${.OBJDIR}/control && tar -czf ../control.tar.gz .
	echo 2.0 > debian-binary; chown ${BINOWN}:${BINGRP} debian-binary
	tar -czf ${IPK} ./debian-binary ./data.tar.gz ./control.tar.gz

Packages: ${IPK}
	echo "Package: openbsd" > ${.OBJDIR}/Packages
	echo "Version: ${OSREV}" >> ${.OBJDIR}/Packages
	echo "Priority: optional" >> ${.OBJDIR}/Packages
	echo "Section: Misc" >> ${.OBJDIR}/Packages
	echo "Architecture: arm" >> ${.OBJDIR}/Packages
	echo "Maintainer: uwe@openbsd.org" >> ${.OBJDIR}/Packages
	echo "Filename: ${IPK}" >> ${.OBJDIR}/Packages
	echo "Size:" `/bin/ls -l ${IPK} | awk '{print $$5}'` \
	    >> ${.OBJDIR}/Packages
	echo "Description: OpenBSD ${OSREV} boot loader and kernel" \
	    >> ${.OBJDIR}/Packages

install: ${IPK} Packages
	cp ${IPK} ${DESTDIR}/snapshot
	cp Packages ${DESTDIR}/snapshot

clean cleandir:
	rm -rf data control ${IPK} debian-binary control.tar.gz data.tar.gz \
	    bootbsd.png Packages

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
