#	$OpenBSD: Makefile,v 1.2 2005/03/18 20:28:38 deraadt Exp $

TOP=    ${.CURDIR}/..
IPK=	openbsd${OSrev}_arm.ipk

all: ${IPK}

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

install:
	cp ${IPK} ${DESTDIR}/snapshot

clean cleandir:
	rm -rf data control ${IPK} debian-binary control.tar.gz data.tar.gz \
	    bootbsd.png

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
