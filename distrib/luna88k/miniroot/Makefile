#	$OpenBSD: Makefile,v 1.5 2019/04/29 01:48:40 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

TARGET=		miniroot${OSrev}.fs
MOUNT_POINT=	/mnt

.ifndef DESTDIR
all ${TARGET}:
	@echo setenv DESTDIR before creating a miniroot!
	@false
.else

all: ${TARGET}

${TARGET}: vn_up install_files showit vn_down

vn_up: blank_filesystem
	vnconfig -v ${TARGET} > vnd
	disklabel -w `cat vnd` miniroot
	newfs -m 0 -f 1024 -b 8192 /dev/r`cat vnd`a
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
	    ${.OBJDIR}/../ramdisk/bsd.rd ${MOUNT_POINT}/bsd

boot:
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/boot ${MOUNT_POINT}/boot
	ln ${MOUNT_POINT}/boot ${MOUNT_POINT}/vmunix

blank_filesystem:
	dd if=/dev/zero of=${TARGET} bs=32k count=128	# 4MB

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
