#objdump: -S
#as: -m68hc12 -gdwarf2
#name: Dwarf2 test on opers12.s
#source: opers12.s

.*: +file format elf32\-m68hc12

Disassembly of section .text:

0+0 <start>:
	sect .text
	globl start

start:
	anda	\[12,x\]		; Indexed indirect
   0:	a4 e3 00 0c 	anda	\[12,X\]
	ldaa	#10
   4:	86 0a       	ldaa	#10
	ldx	L1
   6:	fe 00 09    	ldx	9 <L1>

00000009 <L1>:
L1:	ldy	,x
   9:	ed 00       	ldy	0,X
	addd	1,y		; Offset from register
   b:	e3 41       	addd	1,Y
	subd	\-1,y
   d:	a3 5f       	subd	\-1,Y
	eora	15,y
   f:	a8 4f       	eora	15,Y
	eora	\-16,y
  11:	a8 50       	eora	\-16,Y
	eorb	16,y
  13:	e8 e8 10    	eorb	16,Y
	eorb	\-17,y
  16:	e8 e9 ef    	eorb	\-17,Y
	oraa	128,sp
  19:	aa f0 80    	oraa	128,SP
	orab	\-128,sp
  1c:	ea f1 80    	orab	\-128,SP
	orab	255,x
  1f:	ea e0 ff    	orab	255,X
	orab	\-256,x
  22:	ea e1 00    	orab	\-256,X
	anda	256,x
  25:	a4 e2 01 00 	anda	256,X
	andb	\-257,x
  29:	e4 e2 fe ff 	andb	\-257,X
	anda	\[12,x\]		; Indexed indirect \(16\-bit offset\)
  2d:	a4 e3 00 0c 	anda	\[12,X\]
	ldaa	\[257,y\]
  31:	a6 eb 01 01 	ldaa	\[257,Y\]
	ldab	\[32767,sp\]
  35:	e6 f3 7f ff 	ldab	\[32767,SP\]
	ldd	\[32768,pc\]
  39:	ec fb 80 00 	ldd	\[32768,PC\]
	ldd	L1,pc
  3d:	ec f8 09    	ldd	9,PC
	std	a,x		; Two\-reg index
  40:	6c e4       	std	A,X
	ldx	b,x
  42:	ee e5       	ldx	B,X
	stx	d,y
  44:	6e ee       	stx	D,Y
	addd	1,\+x		; Pre\-Auto inc
  46:	e3 20       	addd	1,\+X
	addd	2,\+x
  48:	e3 21       	addd	2,\+X
	addd	8,\+x
  4a:	e3 27       	addd	8,\+X
	addd	1,sp\+		; Post\-Auto inc
  4c:	e3 b0       	addd	1,SP\+
	addd	2,sp\+
  4e:	e3 b1       	addd	2,SP\+
	addd	8,sp\+
  50:	e3 b7       	addd	8,SP\+
	subd	1,\-y		; Pre\-Auto dec
  52:	a3 6f       	subd	1,\-Y
	subd	2,\-y
  54:	a3 6e       	subd	2,\-Y
	subd	8,\-y
  56:	a3 68       	subd	8,\-Y
	addd	1,y\-		; Post\-Auto dec
  58:	e3 7f       	addd	1,Y\-
	addd	2,y\-
  5a:	e3 7e       	addd	2,Y\-
	addd	8,y\-
  5c:	e3 78       	addd	8,Y\-
	std	\[d,x\]		; Indexed indirect with two reg index
  5e:	6c e7       	std	\[D,X\]
	std	\[d,y\]
  60:	6c ef       	std	\[D,Y\]
	std	\[d,sp\]
  62:	6c f7       	std	\[D,SP\]
	std	\[d,pc\]
  64:	6c ff       	std	\[D,PC\]
	beq	L1
  66:	27 a1       	beq	9 <L1>
	lbeq	start
  68:	18 27 ff 94 	lbeq	0 <start>
	lbcc	L2
  6c:	18 24 00 4c 	lbcc	bc <L2>
;;
;; Move insn with various operands
;; 
	movb	start, 1,x
  70:	18 09 01 00 	movb	0 <start>, 1,X
  74:	00 
	movw	1,x, start
  75:	18 05 01 00 	movw	1,X, 0 <start>
  79:	00 
	movb	start, 1,\+x
  7a:	18 09 20 00 	movb	0 <start>, 1,\+X
  7e:	00 
	movb	start, 1,\-x
  7f:	18 09 2f 00 	movb	0 <start>, 1,\-X
  83:	00 
	movb	#23, 1,\-sp
  84:	18 08 af 17 	movb	#23, 1,\-SP
	movb	L1, L2
  88:	18 0c 00 09 	movb	9 <L1>, bc <L2>
  8c:	00 bc 
	movb	L1, a,x
  8e:	18 09 e4 00 	movb	9 <L1>, A,X
  92:	09 
	movw	L1, b,x
  93:	18 01 e5 00 	movw	9 <L1>, B,X
  97:	09 
	movw	L1, d,x
  98:	18 01 e6 00 	movw	9 <L1>, D,X
  9c:	09 
	movw	d,x, a,x
  9d:	18 02 e6 e4 	movw	D,X, A,X
	movw	b,sp, d,pc
  a1:	18 02 f5 fe 	movw	B,SP, D,PC
	movw	b,sp, L1
  a5:	18 05 f5 00 	movw	B,SP, 9 <L1>
  a9:	09 
	movw	b,sp, 1,x
  aa:	18 02 f5 01 	movw	B,SP, 1,X
	movw	d,x, a,y
  ae:	18 02 e6 ec 	movw	D,X, A,Y
	trap	#0x30
  b2:	18 30       	trap	#48
	trap	#0x39
  b4:	18 39       	trap	#57
	trap	#0x40
  b6:	18 40       	trap	#64
	trap	#0x80
  b8:	18 80       	trap	#128
	trap	#255
  ba:	18 ff       	trap	#255

000000bc <L2>:
L2:	
	movw 1,x,2,x
  bc:	18 02 01 02 	movw	1,X, 2,X
	movw \-1,\-1
  c0:	18 04 ff ff 	movw	ffff <bb\+0xd7ff>, ffff <bb\+0xd7ff>
  c4:	ff ff 
	movw \-1,1,x
  c6:	18 01 01 ff 	movw	ffff <bb\+0xd7ff>, 1,X
  ca:	ff 
	movw #\-1,1,x
  cb:	18 00 01 ff 	movw	#ffff <bb\+0xd7ff>, 1,X
  cf:	ff 
	movw 3,8
  d0:	18 04 00 03 	movw	3 <start\+0x3>, 8 <start\+0x8>
  d4:	00 08 
	movw #3,3
  d6:	18 03 00 03 	movw	#3 <start\+0x3>, 3 <start\+0x3>
  da:	00 03 
	movw #3,1,x
  dc:	18 00 01 00 	movw	#3 <start\+0x3>, 1,X
  e0:	03 
	movw 3,1,x
  e1:	18 01 01 00 	movw	3 <start\+0x3>, 1,X
  e5:	03 
	movw 3,\+2,x
  e6:	18 01 02 00 	movw	3 <start\+0x3>, 2,X
  ea:	03 
	movw 4,\-2,x
  eb:	18 01 1e 00 	movw	4 <start\+0x4>, \-2,X
  ef:	04 
	rts
  f0:	3d          	rts

000000f1 <post_indexed_pb>:
;;
;; Post\-index byte with relocation
;; 
post_indexed_pb:
t1:
	leas	abort,x
  f1:	1b e2 00 00 	leas	0,X

000000f5 <t2>:
t2:
	leax	t2\-t1,y
  f5:	1a 44       	leax	4,Y
	leax	toto,x
  f7:	1a e2 00 64 	leax	100,X
	leas	toto\+titi,sp
  fb:	1b f2 00 6e 	leas	110,SP
	leay	titi,x
  ff:	19 e2 00 0a 	leay	10,X
	leas	bb,y
 103:	1b ea 28 00 	leas	10240,Y
	leas	min5b,pc
 107:	1b fa 00 ff 	leas	255,PC
	leas	max5b,pc
 10b:	1b fa 00 00 	leas	0,PC
	leas	min9b,pc
 10f:	1b fa 00 ff 	leas	255,PC
	leas	max9b,pc
 113:	1b fa 00 00 	leas	0,PC
