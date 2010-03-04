#	$OpenBSD: Makefile,v 1.16 2010/03/04 22:13:57 deraadt Exp $

.if ${MACHINE} == "i386"
SUBDIR=	etc libsa libz mbr cdbr
.endif
SUBDIR+= biosboot installboot boot pxeboot cdboot

test-fd0:
	sudo mount /dev/fd0a /mnt
	sudo cp /usr/mdec/boot /mnt
	sudo /usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot fd0
	sudo umount /mnt

zero-fd0:
	sudo dd if=/dev/zero of=/dev/rfd0a count=144 bs=10240

prep-fd0:
	sudo disklabel -w fd0 floppy
	sudo newfs -O 0 -m 0 -o space -i 3000 -c 80 /dev/rfd0a

.include <bsd.subdir.mk>
