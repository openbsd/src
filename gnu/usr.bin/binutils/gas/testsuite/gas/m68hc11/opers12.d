#objdump: -d --prefix-addresses
#as: -m68hc12
#name: opers

.*: +file format elf32\-m68hc12

Disassembly of section .text:
0+000 <start> anda	\[12,X\]
0+004 <start\+0x4> ldaa	#10
0+006 <start\+0x6> ldx	0+009 <L1>
0+009 <L1> ldy	0,X
0+00b <L1\+0x2> addd	1,Y
0+00d <L1\+0x4> subd	-1,Y
0+00f <L1\+0x6> eora	15,Y
0+011 <L1\+0x8> eora	-16,Y
0+013 <L1\+0xa> eorb	16,Y
0+016 <L1\+0xd> eorb	-17,Y
0+019 <L1\+0x10> oraa	128,SP
0+01c <L1\+0x13> orab	-128,SP
0+01f <L1\+0x16> orab	255,X
0+022 <L1\+0x19> orab	-256,X
0+025 <L1\+0x1c> anda	256,X
0+029 <L1\+0x20> andb	-257,X
0+02d <L1\+0x24> anda	\[12,X\]
0+031 <L1\+0x28> ldaa	\[257,Y\]
0+035 <L1\+0x2c> ldab	\[32767,SP\]
0+039 <L1\+0x30> ldd	\[32768,PC\]
0+03d <L1\+0x34> ldd	9,PC
0+040 <L1\+0x37> std	A,X
0+042 <L1\+0x39> ldx	B,X
0+044 <L1\+0x3b> stx	D,Y
0+046 <L1\+0x3d> addd	1,\+X
0+048 <L1\+0x3f> addd	2,\+X
0+04a <L1\+0x41> addd	8,\+X
0+04c <L1\+0x43> addd	1,SP\+
0+04e <L1\+0x45> addd	2,SP\+
0+050 <L1\+0x47> addd	8,SP\+
0+052 <L1\+0x49> subd	1,\-Y
0+054 <L1\+0x4b> subd	2,\-Y
0+056 <L1\+0x4d> subd	8,\-Y
0+058 <L1\+0x4f> addd	1,Y\-
0+05a <L1\+0x51> addd	2,Y\-
0+05c <L1\+0x53> addd	8,Y\-
0+05e <L1\+0x55> std	\[D,X\]
0+060 <L1\+0x57> std	\[D,Y\]
0+062 <L1\+0x59> std	\[D,SP\]
0+064 <L1\+0x5b> std	\[D,PC\]
0+066 <L1\+0x5d> beq	0+009 <L1>
0+068 <L1\+0x5f> lbeq	0+000 <start>
0+06c <L1\+0x63> lbcc	0+0bc <L2>
0+070 <L1\+0x67> movb	0+000 <start>, 1,X
0+075 <L1\+0x6c> movw	1,X, 0+000 <start>
0+07a <L1\+0x71> movb	0+000 <start>, 1,\+X
0+07f <L1\+0x76> movb	0+000 <start>, 1,\-X
0+084 <L1\+0x7b> movb	#23, 1,\-SP
0+088 <L1\+0x7f> movb	0+009 <L1>, 0+0bc <L2>
0+08e <L1\+0x85> movb	0+009 <L1>, A,X
0+093 <L1\+0x8a> movw	0+009 <L1>, B,X
0+098 <L1\+0x8f> movw	0+009 <L1>, D,X
0+09d <L1\+0x94> movw	D,X, A,X
0+0a1 <L1\+0x98> movw	B,SP, D,PC
0+0a5 <L1\+0x9c> movw	B,SP, 0+009 <L1>
0+0aa <L1\+0xa1> movw	B,SP, 1,X
0+0ae <L1\+0xa5> movw	D,X, A,Y
0+0b2 <L1\+0xa9> trap	#48
0+0b4 <L1\+0xab> trap	#57
0+0b6 <L1\+0xad> trap	#64
0+0b8 <L1\+0xaf> trap	#128
0+0ba <L1\+0xb1> trap	#255
0+0bc <L2> movw	1,X, 2,X
0+0c0 <L2\+0x4> movw	0+0ffff <bb\+0xd7ff>, 0000ffff <bb\+0xd7ff>
0+0c6 <L2\+0xa> movw	0+0ffff <bb\+0xd7ff>, 1,X
0+0cb <L2\+0xf> movw	#0+0ffff <bb\+0xd7ff>, 1,X
0+0d0 <L2\+0x14> movw	0+03 <start\+0x3>, 0+08 <start\+0x8>
0+0d6 <L2\+0x1a> movw	#0+03 <start\+0x3>, 0+03 <start\+0x3>
0+0dc <L2\+0x20> movw	#0+03 <start\+0x3>, 1,X
0+0e1 <L2\+0x25> movw	0+03 <start\+0x3>, 1,X
0+0e6 <L2\+0x2a> movw	0+03 <start\+0x3>, 2,X
0+0eb <L2\+0x2f> movw	0+04 <start\+0x4>, -2,X
0+0f0 <L2\+0x34> rts
0+0f1 <post_indexed_pb> leas	0,X
0+0f5 <t2> leax	4,Y
0+0f7 <t2\+0x2> leax	100,X
0+0fb <t2\+0x6> leas	110,SP
0+0ff <t2\+0xa> leay	10,X
0+103 <t2\+0xe> leas	10240,Y
0+107 <t2\+0x12> leas	255,PC
0+10b <t2\+0x16> leas	0,PC
0+10f <t2\+0x1a> leas	255,PC
0+113 <t2\+0x1e> leas	0,PC
