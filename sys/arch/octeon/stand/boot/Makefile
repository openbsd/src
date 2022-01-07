#	$OpenBSD: Makefile,v 1.9 2022/01/07 10:48:59 visa Exp $

NOMAN=
#MAN=		boot.8

RDBOOT=		${.CURDIR}/../rdboot/obj/rdboot

.if ${MACHINE} == "octeon"
all: bsd rdboot

bsd:
	cd ${.CURDIR}/../../compile/BOOT && \
	    ${MAKE} config && ${MAKE} clean && ${MAKE}
	cp -p ${.CURDIR}/../../compile/BOOT/obj/bsd bsd

rdboot: ${RDBOOT}
	cp -p ${RDBOOT} rdboot
	strip rdboot

mr.fs: rdboot
	rm -rf $@.d
	install -d -o root -g wheel $@.d/dev
	install -d -o root -g wheel $@.d/mnt
	install -d -o root -g wheel $@.d/sbin
	install -o ${BINOWN} -g ${BINGRP} -m 555 rdboot $@.d/sbin/init
	cd $@.d/dev && sh ${DESTDIR}/dev/MAKEDEV boot
	makefs -o disklabel=rdboot,minfree=0,density=1024 $@ $@.d

boot: bsd mr.fs
	cp bsd boot
	rdsetroot boot mr.fs

realinstall: boot
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 boot ${DESTDIR}/usr/mdec

clean:
	rm -f boot bsd mr.fs rdboot
	rm -rf mr.fs.d
.endif

.include <bsd.prog.mk>
