#	$OpenBSD: Makefile,v 1.13 2019/04/30 17:46:06 deraadt Exp $

FS=		miniroot${OSrev}.fs
FSSIZE=		5760
FSDISKTYPE=	fakeramdisk
MTREE=		${UTILS}/mtree.conf
MOUNT_POINT=	/mnt

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

STRIPOPTS=	-R .SUNW_ctf

MRDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=4096

all: ${FS}

${FS}: bsd.gz
	dd if=/dev/zero of=${FS} bs=512 count=${FSSIZE}
	vnconfig -v ${FS} > vnd
	disklabel -w `cat vnd` ${FSDISKTYPE}
	newfs -m 0 /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	install -c -m 555 -o root -g wheel bsd.gz ${MOUNT_POINT}/bsd
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/ofwbootfd ${MOUNT_POINT}/ofwboot
	installboot -v -r ${MOUNT_POINT} `cat vnd` \
	    ${DESTDIR}/usr/mdec/bootblk ${MOUNT_POINT}/ofwboot
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd.gz: bsd.rd
	gzip -c -9nv < bsd.rd > bsd.gz

bsd.rd: bsd mr.fs
	cp bsd bsd.rd_unz
	rdsetroot bsd.rd_unz mr.fs
	objcopy -Sg -R .comment bsd.rd_unz bsd.rd
	strip ${STRIPOPTS} bsd.rd

bsd:
	cd ${.CURDIR}/../../../sys/arch/sparc64/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/sparc64/compile/RAMDISK/obj/bsd bsd

mr.fs: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MRMAKEFSARGS} $@ $@.d

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp bsd.rd ${FS} ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif

clean cleandir:
	/bin/rm -f *.core mr.fs instbin instbin.mk instbin*.cache \
	    lib*.a lib*.olist instbin.map \
	    *.o *.lo *.c bsd bsd.rd{,_unz} bsd.gz bsd.strip ${FS}
	/bin/rm -rf mr.fs.d

.include <bsd.obj.mk>
