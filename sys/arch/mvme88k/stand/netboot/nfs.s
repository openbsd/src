#NO_APP
gcc2_compiled.:
___gnu_compiled_c:
.text
	.even
.globl _nfs_getrootfh
	.type	 _nfs_getrootfh,@function
_nfs_getrootfh:
	link a6,#-392
	moveml #0x3038,sp@-
	movel a6@(12),d3
	lea a6@(-132),a2
	lea a6@(-280),a3
	pea 132:w
	movel a2,sp@-
	jbsr _bzero
	movel d3,sp@-
	jbsr _strlen
	movel d0,d2
	addqw #8,sp
	addqw #4,sp
	moveq #120,d0
	addqw #8,d0
	cmpl d2,d0
	jcc L2
	movel d0,d2
L2:
	movel d2,a2@
	movel d2,sp@-
	pea a6@(-128)
	movel d3,sp@-
	lea _bcopy,a4
	jbsr a4@
	movel d2,d0
	addql #3,d0
	jpl L3
	movel d2,d0
	addql #6,d0
L3:
	moveq #-4,d1
	andl d1,d0
	pea 36:w
	movel a3,sp@-
	movel d0,a0
	pea a0@(4)
	movel a2,sp@-
	pea 1:w
	pea 1:w
	movel #100005,sp@-
	movel a6@(8),sp@-
	jbsr _rpc_call
	addw #44,sp
	moveq #-1,d1
	cmpl d0,d1
	jeq L7
	moveq #3,d1
	cmpl d0,d1
	jcs L5
	moveq #72,d1
	movel d1,_errno
	jra L8
L5:
	tstl a3@
	jne L6
	pea 32:w
	movel a6@(16),sp@-
	pea a3@(4)
	jbsr a4@
	clrl d0
	jra L7
L6:
	movel a3@,_errno
L8:
	moveq #-1,d0
L7:
	moveml a6@(-412),#0x1c0c
	unlk a6
	rts
Lfe1:
	.size	 _nfs_getrootfh,Lfe1-_nfs_getrootfh
	.even
.globl _nfs_lookupfh
	.type	 _nfs_lookupfh,@function
_nfs_lookupfh:
	link a6,#-492
	moveml #0x303c,sp@-
	movel a6@(8),a5
	movel a6@(12),d3
	lea a6@(-164),a2
	lea a6@(-380),a3
	pea 164:w
	movel a2,sp@-
	jbsr _bzero
	pea 32:w
	movel a2,sp@-
	pea a5@(12)
	lea _bcopy,a4
	jbsr a4@
	movel d3,sp@-
	jbsr _strlen
	movel d0,d2
	addw #24,sp
	moveq #120,d0
	addqw #8,d0
	cmpl d2,d0
	jcc L10
	movel d0,d2
L10:
	movel d2,sp@-
	pea a6@(-128)
	movel d3,sp@-
	jbsr a4@
	movel d2,a6@(-132)
	movel d2,d0
	addql #3,d0
	jpl L11
	movel d2,d0
	addql #6,d0
L11:
	moveq #-4,d1
	andl d1,d0
	pea 104:w
	movel a3,sp@-
	movel d0,a0
	pea a0@(36)
	movel a2,sp@-
	pea 4:w
	pea 2:w
	movel #100003,sp@-
	movel a5@,sp@-
	jbsr _rpc_call
	addw #44,sp
	moveq #-1,d1
	cmpl d0,d1
	jne L12
	movel _errno,d0
	jra L15
L12:
	moveq #3,d1
	cmpl d0,d1
	jlt L13
	moveq #5,d0
	jra L15
L13:
	tstl a3@
	jne L14
	pea 32:w
	movel a6@(16),a0
	pea a0@(12)
	pea a3@(4)
	jbsr a4@
	pea 68:w
	movel a6@(16),a0
	pea a0@(44)
	pea a3@(36)
	jbsr a4@
	clrl d0
	jra L15
L14:
	movel a3@,d0
L15:
	moveml a6@(-516),#0x3c0c
	unlk a6
	rts
Lfe2:
	.size	 _nfs_lookupfh,Lfe2-_nfs_lookupfh
LC0:
	.ascii "nfsread: short packet, %d < %d\12\0"
	.even
.globl _nfs_readdata
	.type	 _nfs_readdata,@function
_nfs_readdata:
	link a6,#-1368
	moveml #0x383c,sp@-
	movel a6@(8),a4
	movel a6@(12),d2
	movel a6@(16),d3
	movel a6@(24),d4
	lea a6@(-44),a3
	lea a6@(-1256),a2
	pea 32:w
	movel a3,sp@-
	pea a4@(12)
	lea _bcopy,a5
	jbsr a5@
	movel d3,a6@(-12)
	addqw #8,sp
	addqw #4,sp
	movel #1024,d0
	cmpl d4,d0
	jcc L17
	movel d0,d4
L17:
	movel d4,a6@(-8)
	clrl a6@(-4)
	pea 1100:w
	movel a2,sp@-
	pea 44:w
	movel a3,sp@-
	pea 6:w
	pea 2:w
	movel #100003,sp@-
	movel a4@,sp@-
	jbsr _rpc_call
	movel d0,a0
	addw #32,sp
	moveq #-1,d1
	cmpl a0,d1
	jeq L22
	moveq #76,d1
	cmpl a0,d1
	jhi L23
	tstl a2@
	jeq L20
	movel a2@,_errno
	jra L24
L20:
	addw #-76,a0
	movel a6@(-1184),d2
	cmpl a0,d2
	jgt L21
	movel d2,sp@-
	movel a6@(20),sp@-
	pea a2@(76)
	jbsr a5@
	movel d2,d0
	jra L22
L21:
	movel d2,sp@-
	movel a0,sp@-
	pea LC0
	jbsr _printf
L23:
	moveq #72,d1
	movel d1,_errno
L24:
	moveq #-1,d0
L22:
	moveml a6@(-1396),#0x3c1c
	unlk a6
	rts
Lfe3:
	.size	 _nfs_readdata,Lfe3-_nfs_readdata
	.even
.globl _nfs_mount
	.type	 _nfs_mount,@function
_nfs_mount:
	link a6,#0
	movel a2,sp@-
	movel a6@(8),sp@-
	jbsr _socktodesc
	movel d0,a2
	addqw #4,sp
	tstl a2
	jeq L26
	movel _rpc_port,d0
	subql #1,d0
	movel d0,_rpc_port
	movew d0,a2@(10)
	movel a6@(12),a2@
	pea _nfs_root_node+12
	movel a6@(16),sp@-
	movel a2,sp@-
	jbsr _nfs_getrootfh
	tstl d0
	jne L27
	movel a2,_nfs_root_node
	moveq #2,d1
	movel d1,_nfs_root_node+44
	movel #493,_nfs_root_node+48
	movel d1,_nfs_root_node+52
	clrl d0
	jra L28
L26:
	moveq #22,d1
	movel d1,_errno
L27:
	moveq #-1,d0
L28:
	movel a6@(-4),a2
	unlk a6
	rts
Lfe4:
	.size	 _nfs_mount,Lfe4-_nfs_mount
LC1:
	.ascii "nfs_open: must mount first.\12\0"
	.even
.globl _nfs_open
	.type	 _nfs_open,@function
_nfs_open:
	link a6,#0
	moveml #0x2038,sp@-
	movel a6@(12),a4
	lea _nfs_root_node,a3
	tstl a3@
	jne L30
	pea LC1
	jbsr _printf
	moveq #6,d0
	jra L32
L30:
	pea 112:w
	jbsr _alloc
	movel d0,a2
	movel a3@,a2@
	clrl a2@(4)
	clrl a2@(8)
	movel a2,sp@-
	movel a6@(8),sp@-
	movel a3,sp@-
	jbsr _nfs_lookupfh
	movel d0,d2
	addqw #8,sp
	addqw #8,sp
	jeq L31
	pea 112:w
	movel a2,sp@-
	jbsr _free
	movel d2,d0
	jra L32
L31:
	movel a2,a4@(16)
	clrl d0
L32:
	moveml a6@(-16),#0x1c04
	unlk a6
	rts
Lfe5:
	.size	 _nfs_open,Lfe5-_nfs_open
	.even
.globl _nfs_close
	.type	 _nfs_close,@function
_nfs_close:
	link a6,#0
	movel a2,sp@-
	movel a6@(8),a2
	movel a2@(16),d0
	jeq L34
	pea 112:w
	movel d0,sp@-
	jbsr _free
L34:
	clrl a2@(16)
	clrl d0
	movel a6@(-4),a2
	unlk a6
	rts
Lfe6:
	.size	 _nfs_close,Lfe6-_nfs_close
LC2:
	.ascii "nfs_read: hit EOF unexpectantly\0"
	.even
.globl _nfs_read
	.type	 _nfs_read,@function
_nfs_read:
	link a6,#0
	moveml #0x3f3c,sp@-
	movel a6@(8),a0
	movel a6@(16),a3
	movel a6@(20),a5
	movel a0@(16),a2
	movel a6@(12),a4
	tstl a3
	jle L41
L42:
	jbsr _twiddle
	movel a3,sp@-
	movel a4,sp@-
	movel a2@(8),sp@-
	movel a2@(4),sp@-
	movel a2,sp@-
	jbsr _nfs_readdata
	movel d0,d7
	addw #20,sp
	moveq #-1,d5
	cmpl d7,d5
	jne L38
	movel _errno,d0
	jra L44
L38:
	tstl d7
	jne L39
	tstl _debug
	jeq L41
	pea LC2
	jbsr _printf
	jra L41
L39:
	movel d7,d4
	movel d7,d3
	moveq #31,d5
	asrl d5,d3
	movel d4,d2
	addl a2@(8),d2
	cmpl d2,d4
	shi d0
	extbl d0
	movel d3,d1
	addl a2@(4),d1
	subl d0,d1
	movel d1,a2@(4)
	movel d2,a2@(8)
	addl d7,a4
	subl d7,a3
	tstl a3
	jgt L42
L41:
	tstl a5
	jeq L43
	movel a3,a5@
L43:
	clrl d0
L44:
	moveml a6@(-40),#0x3cfc
	unlk a6
	rts
Lfe7:
	.size	 _nfs_read,Lfe7-_nfs_read
	.even
.globl _nfs_write
	.type	 _nfs_write,@function
_nfs_write:
	link a6,#0
	moveq #30,d0
	unlk a6
	rts
Lfe8:
	.size	 _nfs_write,Lfe8-_nfs_write
	.even
.globl _nfs_seek
	.type	 _nfs_seek,@function
_nfs_seek:
	link a6,#0
	moveml #0x3f00,sp@-
	movel a6@(8),a0
	movel a6@(12),d5
	movel a6@(16),d6
	movel a6@(20),d0
	movel a0@(16),a0
	movel a0@(64),d1
	moveq #1,d7
	cmpl d0,d7
	jeq L49
	jlt L53
	tstl d0
	jeq L48
	jra L51
L53:
	moveq #2,d7
	cmpl d0,d7
	jeq L50
	jra L51
L48:
	movel d5,a0@(4)
	movel d6,a0@(8)
	jra L47
L49:
	movel a0@(4),d3
	movel a0@(8),d4
	movel d6,d2
	addl d4,d2
	cmpl d2,d6
	shi d0
	extbl d0
	movel d5,d1
	addl d3,d1
	subl d0,d1
	movel d1,a0@(4)
	movel d2,a0@(8)
	jra L47
L50:
	movel d1,d4
	clrl d3
	movel d4,d2
	subl d6,d2
	cmpl d2,d4
	scs d0
	extbl d0
	movel d3,d1
	subl d5,d1
	addl d0,d1
	movel d1,a0@(4)
	movel d2,a0@(8)
	jra L47
L51:
	moveq #-1,d0
	moveq #-1,d1
	jra L54
L47:
	movel a0@(4),d0
	movel a0@(8),d1
L54:
	moveml a6@(-24),#0xfc
	unlk a6
	rts
Lfe9:
	.size	 _nfs_seek,Lfe9-_nfs_seek
.globl _nfs_stat_types
.data
	.even
	.type	 _nfs_stat_types,@object
	.size	 _nfs_stat_types,32
_nfs_stat_types:
	.long 0
	.long 32768
	.long 16384
	.long 24576
	.long 8192
	.long 40960
	.long 0
	.skip 4
.text
	.even
.globl _nfs_stat
	.type	 _nfs_stat,@function
_nfs_stat:
	link a6,#0
	movel a2,sp@-
	movel d2,sp@-
	movel a6@(8),a0
	movel a6@(12),a2
	movel a0@(16),a1
	moveq #7,d0
	andl a1@(44),d0
	lea _nfs_stat_types,a0
	movew a1@(50),d2
	orw a0@(2,d0:l:4),d2
	movew d2,a2@(8)
	movew a1@(54),a2@(10)
	movel a1@(56),a2@(12)
	movel a1@(60),a2@(16)
	movel a1@(64),d1
	clrl d0
	movel d0,a2@(48)
	movel d1,a2@(52)
	movel a6@(-8),d2
	movel a6@(-4),a2
	unlk a6
	rts
Lfe10:
	.size	 _nfs_stat,Lfe10-_nfs_stat
.comm _nfs_root_node,112
