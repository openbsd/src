#	$OpenBSD: Makefile,v 1.4 2002/03/30 20:35:31 millert Exp $

.PATH: ${.CURDIR}/../ramdisk
RAMDISK=RAMDISK_CD
FLOPPYSIZE=288
FLOPPYTYPE=floppy288
XNAME=cdrom
FLOPPYSECS=36

.include "${.CURDIR}/../common/Makefile.inc"

CRUNCHCONFS+=	${.CURDIR}/crunch_local.conf
LISTS+=		${.CURDIR}/list.local
