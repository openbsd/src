#objdump: -d --prefix-addresses
#as: -m68hc11
#name: insns

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+000 <_start> lds	#0+0400 <L1\+0x3a9>
0+003 <_start\+0x3> ldx	#0+0001 <_start\+0x1>
0+006 <Loop> jsr	0+0010 <test>
0+009 <Loop\+0x3> dex
0+00a <Loop\+0x4> bne	0+0006 <Loop>
0+00c <Stop> .byte	0xcd, 0x03
0+00e <Stop\+0x2> bra	0+0000 <_start>
0+010 <test> ldd	#0+0002 <_start\+0x2>
0+013 <test\+0x3> jsr	0+0017 <test2>
0+016 <test\+0x6> rts
0+017 <test2> ldx	23,y
0+01a <test2\+0x3> std	23,x
0+01c <test2\+0x5> ldd	0,x
0+01e <test2\+0x7> sty	0,y
0+021 <test2\+0xa> stx	0,y
0+024 <test2\+0xd> brclr	6,x #\$04 00000017 <test2>
0+028 <test2\+0x11> brclr	12,x #\$08 00000017 <test2>
0+02c <test2\+0x15> ldd	\*0+0 <_start>
0+02e <test2\+0x17> ldx	\*0+2 <_start\+0x2>
0+030 <test2\+0x19> clr	0+0 <_start>
0+033 <test2\+0x1c> clr	0+1 <_start\+0x1>
0+036 <test2\+0x1f> bne	0+34 <test2\+0x1d>
0+038 <test2\+0x21> beq	0+3c <test2\+0x25>
0+03a <test2\+0x23> bclr	\*0+1 <_start\+0x1> #\$20
0+03d <test2\+0x26> brclr	\*0+2 <_start\+0x2> #\$28 0+017 <test2>
0+041 <test2\+0x2a> ldy	#0+ffec <L1\+0xff95>
0+045 <test2\+0x2e> ldd	12,y
0+048 <test2\+0x31> addd	44,y
0+04b <test2\+0x34> addd	50,y
0+04e <test2\+0x37> subd	0+02c <test2\+0x15>
0+051 <test2\+0x3a> subd	#0+02c <test2\+0x15>
0+054 <test2\+0x3d> jmp	0000000c <Stop>
0+057 <L1> anda	#23
0+059 <L1\+0x2> andb	#0
0+05b <L1\+0x4> rts