#	$NetBSD: Makefile.inc,v 1.4 1996/05/20 00:48:43 thorpej Exp $

.if target(ncr53cxxx.o)
PATH: $S/dev/microcode/ncr53cxxx

ncr53cxxx.o: ncr53cxxx_seq.h

ncr53cxxx_seq.h: ncr53cxxx_asm $S/dev/microcode/ncr53cxxx/ncr53cxxx.seq
	./ncr53cxxx_asm -o ${.TARGET} $S/dev/microcode/ncr53cxxx/ncr53cxxx.seq

ncr53cxxx_asm: $S/dev/microcode/ncr53cxxx/ncr53cxxx_asm.c
	${HOSTCC} -U_KERNEL -o ${.TARGET} $<

clean::
	rm -f ncr53cxxx_asm
.endif
