#objdump: -dr --prefix-addresses --show-raw-insn -mmips:mips64
#name: MIPS MIPS64 instructions
#as: -mips64

# Check MIPS32 instruction assembly

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 70410825 	dclo	at,v0
0+0004 <[^>]*> 70831824 	dclz	v1,a0
0+0008 <[^>]*> 48232000 	dmfc2	v1,a0
0+000c <[^>]*> 48242800 	dmfc2	a0,a1
0+0010 <[^>]*> 48253007 	dmfc2	a1,a2,7
0+0014 <[^>]*> 48a63800 	dmtc2	a2,a3
0+0018 <[^>]*> 48a74000 	dmtc2	a3,t0
0+001c <[^>]*> 48a84807 	dmtc2	t0,t1,7
