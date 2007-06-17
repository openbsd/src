#	$OpenBSD: Makefile.inc,v 1.7 2007/06/17 00:28:21 deraadt Exp $

IMAGESIZE=		3072
GZIPCMD?=		gzip
GZIPFLAGS?=		-9nv
GZIPEXT?=		.gz
MAKEFLOPPY=		1
#STRIP=			echo
#NEWFS_WILL_FAIL=	true
#NEWFSOPTS_RD= -t ffs -m 0 -o space -f 1024 -i 16384 -s 8192
HOSTCCFLAGS=		-DDEBUG -O0
#HOSTCCFLAGS=		-DBROKEN_NMAGIC -DDEBUG
BOOTXX=			${DESTDIR}/usr/mdec/sdboot
INSTALLBOOT=		@true --		# totally incredible
NEWFSOPTS_RD=		${NEWFSOPTS} -i 4096 -s ${IMAGESIZE}
