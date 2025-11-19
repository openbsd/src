#	$OpenBSD: Makefile,v 1.2 2025/11/19 17:50:46 deraadt Exp $

NOMAN=
#MAN=		vmboot.8

RDBOOT=		${.CURDIR}/../rdboot/obj/rdboot

MRDISKTYPE= rdroot
MRMAKEFSARGS= -o disklabel=${MRDISKTYPE},minfree=0,density=1024

.if ${MACHINE} == "amd64"
all: bsd rdboot

bsd:
	cd ${.CURDIR}/../../compile/VMBOOT && \
	    ${MAKE} config && ${MAKE} clean && ${MAKE}
	cp -p ${.CURDIR}/../../compile/VMBOOT/obj/bsd bsd

rdboot: ${RDBOOT}
	cp -p ${RDBOOT} rdboot
	strip rdboot

mr.fs: rdboot bsd
	rm -rf $@.d
	install -d -o root -g wheel $@.d/dev
	install -d -o root -g wheel $@.d/mnt
	install -d -o root -g wheel $@.d/sbin
	install -o ${BINOWN} -g ${BINGRP} -m 555 rdboot $@.d/sbin/init
	cd $@.d/dev && MAKEDEV_PARTITIONS=16 sh ${DESTDIR}/dev/MAKEDEV vmboot
	makefs ${MRMAKEFSARGS} $@ $@.d

vmboot: mr.fs
	cp bsd vmboot
	rdsetroot vmboot mr.fs

realinstall: vmboot
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 vmboot ${DESTDIR}/usr/mdec

clean:
	rm -f vmboot bsd mr.fs rdboot
	rm -rf mr.fs.d
.endif

.include <bsd.prog.mk>
