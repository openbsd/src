#	$OpenBSD: Makefile,v 1.2 1997/05/29 00:04:30 niklas Exp $
#	$NetBSD: Makefile,v 1.1 1996/03/08 12:41:33 ragge Exp $

BOOTS=	boot
STDBOOS= defboo.com anyboo.com
DEFBOOS= defboo.hk defboo.hp defboo.ra defboo.up
DEVBOOS= hkmboo.com hksboo.com hpmboo.com hpsboo.com \
	ramboo.com rasboo.com upmboo.com upsboo.com
#UTILS=	format copy drtest
# order is NOT random...
OBJS=	${BOOTS} ${STDBOOS} ${DEVBOOS} ${UTILS}
DRIVE=	/dev/crl

# build a boot console rl02 from a DEC console rl02 pack
update:	${OBJS}
	arff rmf ${DRIVE} ${OBJS}
	-rm -f defboo.com

defboo.com:
	@echo You must copy one of ${DEFBOOS} to defboo.com.
	@echo See installing and operating guide for details.
	@/bin/false	#force exits

boot:	../boot/boot
	(cd ../boot;make boot)
	dd if=../boot/boot of=boot bs=32 skip=1

clean:
	rm -f ${BOOTS} ${UTILS} defboo.com
