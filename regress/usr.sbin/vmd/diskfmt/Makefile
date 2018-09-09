#	$OpenBSD: Makefile,v 1.1 2018/09/09 04:25:32 ccardenas Exp $

# This regression test creates a raw disk image and a
# qcow disk image, and scribbles the same data to both
# of them. It verifies that they both have the same
# result.
#
# In order for this test to work, qemu must be installed
# in order to create the disk images.

VMD_DIR=$(BSDSRCDIR)/usr.sbin/vmd/

PROG=vioscribble
SRCS=vioscribble.c $(VMD_DIR)/vioqcow2.c $(VMD_DIR)/vioraw.c
CFLAGS+=-I$(VMD_DIR) -pthread
LDFLAGS+=-pthread

run-regress-vioscribble: scribble-images

scribble-images:
	rm -f scribble.raw scribble.qc2
	vmctl create scribble.raw -s 4G
	qemu-img create -f qcow2 scribble.qc2 4G


.PHONY: ${REGRESS_TARGETS} scribble-images

.include <bsd.regress.mk>
