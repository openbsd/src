#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <add>:
   0:	0d ad f0 00 	add fp,fp || nop

00000004 <add3>:
   4:	8d ad 00 00 	add3 fp,fp,0

00000008 <and>:
   8:	0d cd f0 00 	and fp,fp || nop

0000000c <and3>:
   c:	8d cd 00 00 	and3 fp,fp,0x0

00000010 <or>:
  10:	0d ed f0 00 	or fp,fp || nop

00000014 <or3>:
  14:	8d ed 00 00 	or3 fp,fp,0x0

00000018 <xor>:
  18:	0d dd f0 00 	xor fp,fp || nop

0000001c <xor3>:
  1c:	8d dd 00 00 	xor3 fp,fp,0x0

00000020 <addi>:
  20:	4d 00 f0 00 	addi fp,0 || nop

00000024 <addv>:
  24:	0d 8d f0 00 	addv fp,fp || nop

00000028 <addv3>:
  28:	8d 8d 00 00 	addv3 fp,fp,0

0000002c <addx>:
  2c:	0d 9d f0 00 	addx fp,fp || nop

00000030 <bc8>:
  30:	7c f4 f0 00 	bc 0 <add> || nop

00000034 <bc8_s>:
  34:	7c f3 f0 00 	bc 0 <add> || nop

00000038 <bc24>:
  38:	7c f2 f0 00 	bc 0 <add> || nop

0000003c <bc24_l>:
  3c:	fc ff ff f1 	bc 0 <add>

00000040 <beq>:
  40:	bd 0d ff f0 	beq fp,fp,0 <add>

00000044 <beqz>:
  44:	b0 8d ff ef 	beqz fp,0 <add>

00000048 <bgez>:
  48:	b0 bd ff ee 	bgez fp,0 <add>

0000004c <bgtz>:
  4c:	b0 dd ff ed 	bgtz fp,0 <add>

00000050 <blez>:
  50:	b0 cd ff ec 	blez fp,0 <add>

00000054 <bltz>:
  54:	b0 ad ff eb 	bltz fp,0 <add>

00000058 <bnez>:
  58:	b0 9d ff ea 	bnez fp,0 <add>

0000005c <bl8>:
  5c:	7e e9 f0 00 	bl 0 <add> || nop

00000060 <bl8_s>:
  60:	7e e8 f0 00 	bl 0 <add> || nop

00000064 <bl24>:
  64:	7e e7 f0 00 	bl 0 <add> || nop

00000068 <bl24_l>:
  68:	fe ff ff e6 	bl 0 <add>

0000006c <bnc8>:
  6c:	7d e5 f0 00 	bnc 0 <add> || nop

00000070 <bnc8_s>:
  70:	7d e4 f0 00 	bnc 0 <add> || nop

00000074 <bnc24>:
  74:	7d e3 f0 00 	bnc 0 <add> || nop

00000078 <bnc24_l>:
  78:	fd ff ff e2 	bnc 0 <add>

0000007c <bne>:
  7c:	bd 1d ff e1 	bne fp,fp,0 <add>

00000080 <bra8>:
  80:	7f e0 f0 00 	bra 0 <add> || nop

00000084 <bra8_s>:
  84:	7f df f0 00 	bra 0 <add> || nop

00000088 <bra24>:
  88:	7f de f0 00 	bra 0 <add> || nop

0000008c <bra24_l>:
  8c:	ff ff ff dd 	bra 0 <add>

00000090 <cmp>:
  90:	0d 4d f0 00 	cmp fp,fp || nop

00000094 <cmpi>:
  94:	80 4d 00 00 	cmpi fp,0

00000098 <cmpu>:
  98:	0d 5d f0 00 	cmpu fp,fp || nop

0000009c <cmpui>:
  9c:	80 5d 00 00 	cmpui fp,0

000000a0 <div>:
  a0:	9d 0d 00 00 	div fp,fp

000000a4 <divu>:
  a4:	9d 1d 00 00 	divu fp,fp

000000a8 <rem>:
  a8:	9d 2d 00 00 	rem fp,fp

000000ac <remu>:
  ac:	9d 3d 00 00 	remu fp,fp

000000b0 <jl>:
  b0:	1e cd f0 00 	jl fp || nop

000000b4 <jmp>:
  b4:	1f cd f0 00 	jmp fp || nop

000000b8 <ld>:
  b8:	2d cd f0 00 	ld fp,@fp || nop

000000bc <ld_2>:
  bc:	2d cd f0 00 	ld fp,@fp || nop

000000c0 <ld_d>:
  c0:	ad cd 00 00 	ld fp,@\(0,fp\)

000000c4 <ld_d2>:
  c4:	ad cd 00 00 	ld fp,@\(0,fp\)

000000c8 <ldb>:
  c8:	2d 8d f0 00 	ldb fp,@fp || nop

000000cc <ldb_2>:
  cc:	2d 8d f0 00 	ldb fp,@fp || nop

000000d0 <ldb_d>:
  d0:	ad 8d 00 00 	ldb fp,@\(0,fp\)

000000d4 <ldb_d2>:
  d4:	ad 8d 00 00 	ldb fp,@\(0,fp\)

000000d8 <ldh>:
  d8:	2d ad f0 00 	ldh fp,@fp || nop

000000dc <ldh_2>:
  dc:	2d ad f0 00 	ldh fp,@fp || nop

000000e0 <ldh_d>:
  e0:	ad ad 00 00 	ldh fp,@\(0,fp\)

000000e4 <ldh_d2>:
  e4:	ad ad 00 00 	ldh fp,@\(0,fp\)

000000e8 <ldub>:
  e8:	2d 9d f0 00 	ldub fp,@fp || nop

000000ec <ldub_2>:
  ec:	2d 9d f0 00 	ldub fp,@fp || nop

000000f0 <ldub_d>:
  f0:	ad 9d 00 00 	ldub fp,@\(0,fp\)

000000f4 <ldub_d2>:
  f4:	ad 9d 00 00 	ldub fp,@\(0,fp\)

000000f8 <lduh>:
  f8:	2d bd f0 00 	lduh fp,@fp || nop

000000fc <lduh_2>:
  fc:	2d bd f0 00 	lduh fp,@fp || nop

00000100 <lduh_d>:
 100:	ad bd 00 00 	lduh fp,@\(0,fp\)

00000104 <lduh_d2>:
 104:	ad bd 00 00 	lduh fp,@\(0,fp\)

00000108 <ld_plus>:
 108:	2d ed f0 00 	ld fp,@fp\+ || nop

0000010c <ld24>:
 10c:	ed 00 00 00 	ld24 fp,0 <add>
			10c: R_M32R_24	.data

00000110 <ldi8>:
 110:	6d 00 f0 00 	ldi fp,0 || nop

00000114 <ldi8a>:
 114:	6d 00 f0 00 	ldi fp,0 || nop

00000118 <ldi16>:
 118:	6d 00 f0 00 	ldi fp,0 || nop

0000011c <ldi16a>:
 11c:	9d f0 00 00 	ldi fp,0

00000120 <lock>:
 120:	2d dd f0 00 	lock fp,@fp || nop

00000124 <machi>:
 124:	3d 4d f0 00 	machi fp,fp || nop

00000128 <maclo>:
 128:	3d 5d f0 00 	maclo fp,fp || nop

0000012c <macwhi>:
 12c:	3d 6d f0 00 	macwhi fp,fp || nop

00000130 <macwlo>:
 130:	3d 7d f0 00 	macwlo fp,fp || nop

00000134 <mul>:
 134:	1d 6d f0 00 	mul fp,fp || nop

00000138 <mulhi>:
 138:	3d 0d f0 00 	mulhi fp,fp || nop

0000013c <mullo>:
 13c:	3d 1d f0 00 	mullo fp,fp || nop

00000140 <mulwhi>:
 140:	3d 2d f0 00 	mulwhi fp,fp || nop

00000144 <mulwlo>:
 144:	3d 3d f0 00 	mulwlo fp,fp || nop

00000148 <mv>:
 148:	1d 8d f0 00 	mv fp,fp || nop

0000014c <mvfachi>:
 14c:	5d f0 f0 00 	mvfachi fp || nop

00000150 <mvfaclo>:
 150:	5d f1 f0 00 	mvfaclo fp || nop

00000154 <mvfacmi>:
 154:	5d f2 f0 00 	mvfacmi fp || nop

00000158 <mvfc>:
 158:	1d 90 f0 00 	mvfc fp,psw || nop

0000015c <mvtachi>:
 15c:	5d 70 f0 00 	mvtachi fp || nop

00000160 <mvtaclo>:
 160:	5d 71 f0 00 	mvtaclo fp || nop

00000164 <mvtc>:
 164:	10 ad f0 00 	mvtc fp,psw || nop

00000168 <neg>:
 168:	0d 3d f0 00 	neg fp,fp || nop

0000016c <nop>:
 16c:	0d bd f0 00 	not fp,fp || nop

00000170 <rac>:
 170:	dd c0 00 00 	seth fp,0x0

00000174 <sll>:
 174:	1d 4d f0 00 	sll fp,fp || nop

00000178 <sll3>:
 178:	9d cd 00 00 	sll3 fp,fp,0

0000017c <slli>:
 17c:	5d 40 f0 00 	slli fp,0x0 || nop

00000180 <sra>:
 180:	1d 2d f0 00 	sra fp,fp || nop

00000184 <sra3>:
 184:	9d ad 00 00 	sra3 fp,fp,0

00000188 <srai>:
 188:	5d 20 f0 00 	srai fp,0x0 || nop

0000018c <srl>:
 18c:	1d 0d f0 00 	srl fp,fp || nop

00000190 <srl3>:
 190:	9d 8d 00 00 	srl3 fp,fp,0

00000194 <srli>:
 194:	5d 00 f0 00 	srli fp,0x0 || nop

00000198 <st>:
 198:	2d 4d f0 00 	st fp,@fp || nop

0000019c <st_2>:
 19c:	2d 4d f0 00 	st fp,@fp || nop

000001a0 <st_d>:
 1a0:	ad 4d 00 00 	st fp,@\(0,fp\)

000001a4 <st_d2>:
 1a4:	ad 4d 00 00 	st fp,@\(0,fp\)

000001a8 <stb>:
 1a8:	2d 0d f0 00 	stb fp,@fp || nop

000001ac <stb_2>:
 1ac:	2d 0d f0 00 	stb fp,@fp || nop

000001b0 <stb_d>:
 1b0:	ad 0d 00 00 	stb fp,@\(0,fp\)

000001b4 <stb_d2>:
 1b4:	ad 0d 00 00 	stb fp,@\(0,fp\)

000001b8 <sth>:
 1b8:	2d 2d f0 00 	sth fp,@fp || nop

000001bc <sth_2>:
 1bc:	2d 2d f0 00 	sth fp,@fp || nop

000001c0 <sth_d>:
 1c0:	ad 2d 00 00 	sth fp,@\(0,fp\)

000001c4 <sth_d2>:
 1c4:	ad 2d 00 00 	sth fp,@\(0,fp\)

000001c8 <st_plus>:
 1c8:	2d 6d f0 00 	st fp,@\+fp || nop

000001cc <st_minus>:
 1cc:	2d 7d f0 00 	st fp,@-fp || nop

000001d0 <sub>:
 1d0:	0d 2d f0 00 	sub fp,fp || nop

000001d4 <subv>:
 1d4:	0d 0d f0 00 	subv fp,fp || nop

000001d8 <subx>:
 1d8:	0d 1d f0 00 	subx fp,fp || nop

000001dc <trap>:
 1dc:	10 f0 f0 00 	trap 0x0 || nop

000001e0 <unlock>:
 1e0:	2d 5d f0 00 	unlock fp,@fp || nop

000001e4 <push>:
 1e4:	2d 7f f0 00 	st fp,@-sp || nop

000001e8 <pop>:
 1e8:	2d ef f0 00 	ld fp,@sp\+ || nop
