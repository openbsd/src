

#define	INTRNAMES_DEFINITION						\
/* 0x00 */	ASCIZ "clock";						\
		ASCIZ "isa irq 0";					\
		ASCIZ "isa irq 1";					\
		ASCIZ "isa irq 2";					\
		ASCIZ "isa irq 3";					\
		ASCIZ "isa irq 4";					\
		ASCIZ "isa irq 5";					\
		ASCIZ "isa irq 6";					\
		ASCIZ "isa irq 7";					\
		ASCIZ "isa irq 8";					\
		ASCIZ "isa irq 9";					\
		ASCIZ "isa irq 10";					\
		ASCIZ "isa irq 11";					\
		ASCIZ "isa irq 12";					\
		ASCIZ "isa irq 13";					\
		ASCIZ "isa irq 14";					\
/* 0x10 */	ASCIZ "isa irq 15";					\
		ASCIZ "kn20aa irq 0";					\
		ASCIZ "kn20aa irq 1";					\
		ASCIZ "kn20aa irq 2";					\
		ASCIZ "kn20aa irq 3";					\
		ASCIZ "kn20aa irq 4";					\
		ASCIZ "kn20aa irq 5";					\
		ASCIZ "kn20aa irq 6";					\
		ASCIZ "kn20aa irq 7";					\
		ASCIZ "kn20aa irq 8";					\
		ASCIZ "kn20aa irq 9";					\
		ASCIZ "kn20aa irq 10";					\
		ASCIZ "kn20aa irq 11";					\
		ASCIZ "kn20aa irq 12";					\
		ASCIZ "kn20aa irq 13";					\
		ASCIZ "kn20aa irq 14";					\
/* 0x20 */	ASCIZ "kn20aa irq 15";					\
		ASCIZ "kn20aa irq 16";					\
		ASCIZ "kn20aa irq 17";					\
		ASCIZ "kn20aa irq 18";					\
		ASCIZ "kn20aa irq 19";					\
		ASCIZ "kn20aa irq 20";					\
		ASCIZ "kn20aa irq 21";					\
		ASCIZ "kn20aa irq 22";					\
		ASCIZ "kn20aa irq 23";					\
		ASCIZ "kn20aa irq 24";					\
		ASCIZ "kn20aa irq 25";					\
		ASCIZ "kn20aa irq 26";					\
		ASCIZ "kn20aa irq 27";					\
		ASCIZ "kn20aa irq 28";					\
		ASCIZ "kn20aa irq 29";					\
		ASCIZ "kn20aa irq 30";					\
/* 0x30 */	ASCIZ "kn20aa irq 31";

#define INTRCNT_DEFINITION						\
/* 0x00 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x10 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x20 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x30 */	.quad 0;

#define	INTRCNT_CLOCK		0
#define INTRCNT_ISA_IRQ		(INTRCNT_CLOCK + 1)
#define	INTRCNT_ISA_IRQ_LEN	16
#define INTRCNT_KN20AA_IRQ	(INTRCNT_ISA_IRQ + INTRCNT_ISA_IRQ_LEN)
#define INTRCNT_KN20AA_IRQ_LEN	32

#ifndef LOCORE
extern long intrcnt[];
#endif
