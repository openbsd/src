#	$OpenBSD: Makefile.inc,v 1.3 2002/04/05 03:10:04 deraadt Exp $

IMAGESIZE=		3072
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
#STRIP=			echo
#NEWFS_WILL_FAIL=	true
#NEWFSOPTS_RD= -t ffs -m 0 -o space -f 1024 -i 16384 -c 4 -s 8192
HOSTCCFLAGS=		-DDEBUG -O0
#HOSTCCFLAGS=		-DBROKEN_NMAGIC -DDEBUG
BOOTXX=			${DESTDIR}/usr/mdec/sdboot
INSTALLBOOT=		@true --		# totally incredible
NEWFSOPTS_RD=		${NEWFSOPTS} -i 4096 -c 64 -s ${IMAGESIZE}
