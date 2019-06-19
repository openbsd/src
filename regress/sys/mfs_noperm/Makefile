#	$OpenBSD: Makefile,v 1.1 2018/12/23 11:23:21 natano Exp $

PROG=   nopermtest
CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wshadow

REGRESS_SETUP_ONCE=	mount
REGRESS_CLEANUP=	umount
REGRESS_TARGETS=	run-regress-locked run-regress-unlocked

mount:
	mount_mfs -s1M -o noperm swap /mnt

umount:
	umount -f /mnt

run-regress-locked: ${PROG}
	chown root:wheel /mnt
	chmod 700 /mnt
	su build -c './${PROG} /mnt locked'

run-regress-unlocked: ${PROG}
	chown build /mnt
	chmod 700 /mnt
	su build -c './${PROG} /mnt unlocked'

.include <bsd.regress.mk>
