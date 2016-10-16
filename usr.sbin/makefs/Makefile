#	$NetBSD: Makefile,v 1.36 2013/08/05 14:41:57 reinoud Exp $
#

.include <bsd.own.mk>

PROG=	makefs

SRCS=	cd9660.c ffs.c msdos.c \
	makefs.c \
	walk.c \
	xmalloc.c

# cd9660
SRCS+=	cd9660_strings.c cd9660_debug.c cd9660_eltorito.c
SRCS+=	cd9660_write.c cd9660_conversion.c iso9660_rrip.c cd9660_archimedes.c

# ffs
SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ffs_tables.c ufs_bmap.c
SRCS+=	buf.c mkfs.c

# msdos
SRCS+=	mkfs_msdos.c msdosfs_fat.c msdosfs_conv.c msdosfs_vfsops.c
SRCS+=	msdosfs_lookup.c msdosfs_denode.c msdosfs_vnops.c msdosfs_unicode.c
SRCS+=	clock_subr.c

MAN=	makefs.8

CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/fs/cd9660
CPPFLAGS+=	-DMSDOS_EI -I${.CURDIR}/fs/msdosfs -I${.CURDIR}/newfs_msdos
#CPPFLAGS+=	-DMSDOSFS_DEBUG

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.PATH:	${.CURDIR}/cd9660 ${.CURDIR}/fs/cd9660 \
	${.CURDIR}/ffs ${.CURDIR}/ufs/ffs \
	${.CURDIR}/msdos ${.CURDIR}/fs/msdosfs ${.CURDIR}/newfs_msdos

.include <bsd.prog.mk>
