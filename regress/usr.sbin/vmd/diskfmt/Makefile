#	$OpenBSD: Makefile,v 1.2 2018/10/08 16:32:01 reyk Exp $

# This regression test creates a raw disk image and a
# qcow disk image, and scribbles the same data to both
# of them. It verifies that they both have the same
# result.
#
# In order for this test to work, qemu must be installed
# in order to create the disk images.

VMD_DIR=$(BSDSRCDIR)/usr.sbin/vmd/

PROG=vioscribble
SRCS=vioscribble.c vioqcow2.c vioraw.c
CFLAGS+=-I$(VMD_DIR) -pthread
LDFLAGS+=-pthread

run-regress-vioscribble: scribble-images

scribble-images:
	rm -f scribble.raw scribble.qc2
	vmctl create scribble.raw -s 4G
	qemu-img create -f qcow2 scribble.qc2 4G


.PHONY: ${REGRESS_TARGETS} scribble-images

.include <bsd.regress.mk>

vioqcow2.c vioraw.c: $(VMD_DIR)/vioqcow2.c $(VMD_DIR)/vioraw.c
	cp $(VMD_DIR)/vioqcow2.c $(VMD_DIR)/vioraw.c .
