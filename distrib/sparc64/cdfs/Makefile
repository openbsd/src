#	$OpenBSD: Makefile,v 1.2 2001/10/12 16:45:43 jason Exp $

TARGET=cd.fs

.ifndef DESTDIR
all ${TARGET}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

all: ${TARGET}

${TARGET}: vn_up install_files installboot vn_down

vn_up: blank_filesystem
	vnconfig svnd0 ${TARGET}
	disklabel -w svnd0 fakeramdisk
	newfs /dev/rsvnd0a
	mount /dev/svnd0a /mnt

vn_down:
	-umount /mnt
	-vnconfig -u svnd0

install_files: bsd.rd ofwboot

bsd.rd:
	install -c -m 555 -o root -g wheel \
	    ${.OBJDIR}/../bsd.rd/bsd.rd /mnt/bsd

ofwboot:
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/ofwboot /mnt/ofwboot

installboot:
	/usr/mdec/installboot -v ${DESTDIR}/usr/mdec/bootblk /dev/svnd0c

blank_filesystem:
	dd if=/dev/zero of=${TARGET} bs=512 count=12800

.endif

install:
	cp cd.fs ${DESTDIR}/snapshot/

clean:
	rm -f ${TARGET}


.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>

