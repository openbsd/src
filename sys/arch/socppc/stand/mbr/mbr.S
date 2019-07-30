#include <machine/asm.h>

ENTRY(start)
	 nop

/* space for mbr_dsn */
	. = _C_LABEL(start) + 0x1b4
	.long	0

/* mbr_bootsel_magic */
	. = _C_LABEL(start) + 0x1b8
	.word	0

/*
 * MBR partition table
 */
	. = _C_LABEL(start) + 0x1be
_pbr_part0:
	.byte	0x80, 0, 0, 0, 0x27, 0, 0, 0	
	.byte	0x3f, 0, 0, 0, 0, 0x02, 0, 0
_pbr_part1:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.byte	0, 0, 0, 0, 0, 0, 0, 0
_pbr_part2:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.byte	0, 0, 0, 0, 0, 0, 0, 0
_pbr_part3:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.byte	0, 0, 0, 0, 0, 0, 0, 0

	. = _C_LABEL(start) + 0x1fe
magic:
	.word	0x55aa
mbr_end:
