#	$OpenBSD: Makefile,v 1.1 2002/04/22 02:40:03 deraadt Exp $

.PATH: ${.CURDIR}/../ramdisk
RAMDISK=RAMDISK_CD
FLOPPYSIZE=288
FLOPPYTYPE=floppy288
XNAME=cdrom
FLOPPYSECS=36

.include "${.CURDIR}/../common/Makefile.inc"

CRUNCHCONFS+=	${.CURDIR}/crunch_local.conf
LISTS+=		${.CURDIR}/list.local
