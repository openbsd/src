.if target(aic7xxx.o)
PATH: $S/dev/microcode/aic7xxx

aic7xxx.o: aic7xxx_seq.h

aic7xxx_seq.h: aic7xxx_asm $S/dev/microcode/aic7xxx/aic7xxx.seq
	./aic7xxx_asm -v -o ${.TARGET} $S/dev/microcode/aic7xxx/aic7xxx.seq

aic7xxx_asm: $S/dev/microcode/aic7xxx/aic7xxx_asm.c
.endif
