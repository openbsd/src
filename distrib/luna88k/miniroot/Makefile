#	$OpenBSD: Makefile,v 1.2 2014/03/18 14:35:01 aoyama Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

TARGET=miniroot${REV}.fs

.ifndef DESTDIR
all ${TARGET}:
	@echo setenv DESTDIR before creating a miniroot!
	@false
.else

all: ${TARGET}

${TARGET}: vn_up install_files showit vn_down

vn_up: blank_filesystem
	vnconfig vnd0 ${TARGET}
	disklabel -w vnd0 miniroot
	newfs -m 0 -f 1024 -b 8192 /dev/rvnd0a
	mount /dev/vnd0a /mnt

showit:
	df -ki /mnt

vn_down:
	-umount /mnt
	-vnconfig -u vnd0

install_files: bsd.rd boot

bsd.rd:
	install -c -m 555 -o root -g wheel \
	    ${.OBJDIR}/../ramdisk/bsd.rd /mnt/bsd

boot:
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/boot /mnt/boot
	ln /mnt/boot /mnt/vmunix

blank_filesystem:
	dd if=/dev/zero of=${TARGET} bs=32k count=128	# 4MB

.endif

unconfig:
	-umount -f /mnt
	-vnconfig -u /dev/vnd0a

.ifdef RELEASEDIR
install:
	cp ${TARGET} ${RELEASEDIR}
.endif

clean:
	rm -f ${TARGET}

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
