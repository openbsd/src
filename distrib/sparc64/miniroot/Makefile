#	$OpenBSD: Makefile,v 1.11 2019/04/29 01:48:41 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

TARGET=		miniroot${OSrev}.fs
MOUNT_POINT=	/mnt

.ifndef DESTDIR
all ${TARGET}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

all: ${TARGET}

${TARGET}: vn_up install_files installboot showit vn_down

vn_up: blank_filesystem
	vnconfig -v ${TARGET} > vnd
	disklabel -w `cat vnd` fakeramdisk
	newfs -m 0 /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}

showit:
	df -i ${MOUNT_POINT}

vn_down:
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

install_files: bsd.rd ofwboot

bsd.rd:
	install -c -m 555 -o root -g wheel \
	    ${.OBJDIR}/../bsd.rd/bsd.rd ${MOUNT_POINT}/bsd

ofwboot:
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/ofwbootfd ${MOUNT_POINT}/ofwboot

installboot:
	installboot -v `cat vnd`

blank_filesystem:
	dd if=/dev/zero of=${TARGET} bs=512 count=5760

.endif

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp ${TARGET} ${RELEASEDIR}
.endif

clean:
	rm -f ${TARGET}

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
