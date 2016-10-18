#	$OpenBSD: Makefile,v 1.8 2016/10/18 17:05:30 natano Exp $
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
SRCS+=	cd9660_write.c cd9660_conversion.c iso9660_rrip.c

# ffs
SRCS+=	ffs_alloc.c ffs_balloc.c ffs_subr.c ffs_tables.c ufs_bmap.c buf.c mkfs.c

# msdos
SRCS+=	mkfs_msdos.c msdosfs_fat.c msdosfs_conv.c msdosfs_vfsops.c
SRCS+=	msdosfs_lookup.c msdosfs_denode.c msdosfs_vnops.c msdosfs_unicode.c

MAN=	makefs.8

CPPFLAGS+=	-I${.CURDIR}
#CPPFLAGS+=	-DMSDOSFS_DEBUG

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.PATH:	${.CURDIR}/cd9660 ${.CURDIR}/ffs ${.CURDIR}/ufs/ffs \
	${.CURDIR}/msdos

.include <bsd.prog.mk>
