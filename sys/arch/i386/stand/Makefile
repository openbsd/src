#	$OpenBSD: Makefile,v 1.9 2000/05/26 11:50:46 deraadt Exp $

.if ${MACHINE} == "i386"
SUBDIR=	etc libsa libz mbr
.endif
SUBDIR+= biosboot installboot boot

test-fd0:
	sudo mount /dev/fd0a /mnt
	sudo cp /usr/mdec/boot /mnt
	sudo /usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot fd0
	sudo umount /mnt

.include <bsd.subdir.mk>
