#	$OpenBSD: Makefile,v 1.5 2011/06/23 22:46:12 schwarze Exp $

# NOTE: The mkhybrid distrib also makes an "mkisofs" which is the same binary
# linked. When we know mkhybrid makes distrib CDs ok we should modify this
# to make mkisofs as a link to this, rather than the (earlier) version in
# the tree.

PROG=	mkhybrid
MAN=	mkhybrid.8
BINDIR=	/usr/sbin
CLEANFILES+= mkhybrid.8
.PATH:	${.CURDIR}/../src ${.CURDIR}/../src/libhfs_iso ${.CURDIR}/../src/libfile

SRCS=	data.c block.c low.c lfile.c btree.c node.c record.c lvolume.c \
	hfs.c file.c apprentice.c softmagic.c mkisofs.c tree.c write.c \
	hash.c rock.c multi.c joliet.c match.c name.c eltorito.c \
	getopt.c getopt1.c apple.c volume.c desktop.c mac_label.c		

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"OpenBSD\" -DAPPLE_HYB -DVANILLA_AUTOCONF \
	-I${.CURDIR}/../src -I${.CURDIR}/../src/include \
	-I${.CURDIR}/../src/libhfs_iso \
	-I${.CURDIR}/../src/libfile

# XXX: alpha gcc bug
write.o: write.c
	${CC} ${CFLAGS} -O0 -c ${.CURDIR}/../src/write.c

mkhybrid.8: mkhybrid.8tbl
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
