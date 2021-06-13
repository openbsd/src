#	$OpenBSD: Makefile,v 1.3 2021/06/13 21:43:35 dv Exp $

# This regression test creates a raw disk image and a
# qcow disk image, and scribbles the same data to both
# of them. It verifies that they both have the same
# result.

VMD_DIR=$(BSDSRCDIR)/usr.sbin/vmd

PROG=vioscribble
SRCS=vioscribble.c vioqcow2.c vioraw.c log.c
CFLAGS+=-I$(VMD_DIR) -pthread
LDFLAGS+=-pthread

run-regress-vioscribble: scribble-images

scribble-images:
	rm -f scribble.raw scribble.qcow2
	vmctl create -s 4G scribble.raw
	vmctl create -s 4G scribble.qcow2


.PHONY: ${REGRESS_TARGETS} scribble-images

.include <bsd.regress.mk>

vioqcow2.c vioraw.c log.c: $(VMD_DIR)/vioqcow2.c $(VMD_DIR)/vioraw.c \
				$(VMD_DIR)/log.c
	cp $(VMD_DIR)/vioqcow2.c $(VMD_DIR)/vioraw.c $(VMD_DIR)/log.c .
