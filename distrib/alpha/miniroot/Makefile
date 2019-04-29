#	$OpenBSD: Makefile,v 1.7 2019/04/29 15:56:25 deraadt Exp $

FS=		miniroot${OSrev}.fs
MOUNT_POINT=	/mnt

.ifndef DESTDIR
all ${FS}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

all: ${FS}

${FS}: vn_up install_files installboot showit vn_down

vn_up: blank_filesystem
	vnconfig -v ${FS} > vnd
	disklabel -w `cat vnd` fakeramdisk
	newfs -m 0 /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}

showit:
	df -i ${MOUNT_POINT}

vn_down:
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

install_files: bsd.rd boot

bsd.rd:
	install -c -m 555 -o root -g wheel \
	    ${.OBJDIR}/../bsd.rd/bsd.rd ${MOUNT_POINT}/bsd

boot:
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/boot ${MOUNT_POINT}/boot

installboot:
	/usr/mdec/installboot -v ${MOUNT_POINT}/boot \
	    ${DESTDRIR}/usr/mdec/bootxx `cat vnd`

blank_filesystem:
	dd if=/dev/zero of=${FS} bs=512 count=5760

.endif

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp ${FS} ${RELEASEDIR}
.endif

clean:
	rm -f ${FS}

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
