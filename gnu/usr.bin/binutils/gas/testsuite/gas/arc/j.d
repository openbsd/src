#objdump: -dr
#name: j

# Test the j insn.

.*: +file format elf32-.*arc

Disassembly of section .text:
00000000 <text_label> 38000000	j r0
00000004 <text_label\+4> 38000020	j.d r0
00000008 <text_label\+8> 38000040	j.jd r0
0000000c <text_label\+c> 38000000	j r0
00000010 <text_label\+10> 38008000	j r1
00000014 <text_label\+14> 38008020	j.d r1
00000018 <text_label\+18> 38008040	j.jd r1
0000001c <text_label\+1c> 38008000	j r1
00000020 <text_label\+20> 381f0000	j 0
		RELOC: 00000024 R_ARC_32 .text
00000028 <text_label\+28> 381f0000	j 0
		RELOC: 0000002c R_ARC_32 .text
00000030 <text_label\+30> 381f0000	j 0
		RELOC: 00000034 R_ARC_32 .text
00000038 <text_label\+38> 381f0001	jeq 0
		RELOC: 0000003c R_ARC_32 .text
00000040 <text_label\+40> 381f0001	jeq 0
		RELOC: 00000044 R_ARC_32 .text
00000048 <text_label\+48> 381f0002	jne 0
		RELOC: 0000004c R_ARC_32 .text
00000050 <text_label\+50> 381f0002	jne 0
		RELOC: 00000054 R_ARC_32 .text
00000058 <text_label\+58> 381f0003	jp 0
		RELOC: 0000005c R_ARC_32 .text
00000060 <text_label\+60> 381f0003	jp 0
		RELOC: 00000064 R_ARC_32 .text
00000068 <text_label\+68> 381f0004	jn 0
		RELOC: 0000006c R_ARC_32 .text
00000070 <text_label\+70> 381f0004	jn 0
		RELOC: 00000074 R_ARC_32 .text
00000078 <text_label\+78> 381f0005	jc 0
		RELOC: 0000007c R_ARC_32 .text
00000080 <text_label\+80> 381f0005	jc 0
		RELOC: 00000084 R_ARC_32 .text
00000088 <text_label\+88> 381f0005	jc 0
		RELOC: 0000008c R_ARC_32 .text
00000090 <text_label\+90> 381f0006	jnc 0
		RELOC: 00000094 R_ARC_32 .text
00000098 <text_label\+98> 381f0006	jnc 0
		RELOC: 0000009c R_ARC_32 .text
000000a0 <text_label\+a0> 381f0006	jnc 0
		RELOC: 000000a4 R_ARC_32 .text
000000a8 <text_label\+a8> 381f0007	jv 0
		RELOC: 000000ac R_ARC_32 .text
000000b0 <text_label\+b0> 381f0007	jv 0
		RELOC: 000000b4 R_ARC_32 .text
000000b8 <text_label\+b8> 381f0008	jnv 0
		RELOC: 000000bc R_ARC_32 .text
000000c0 <text_label\+c0> 381f0008	jnv 0
		RELOC: 000000c4 R_ARC_32 .text
000000c8 <text_label\+c8> 381f0009	jgt 0
		RELOC: 000000cc R_ARC_32 .text
000000d0 <text_label\+d0> 381f000a	jge 0
		RELOC: 000000d4 R_ARC_32 .text
000000d8 <text_label\+d8> 381f000b	jlt 0
		RELOC: 000000dc R_ARC_32 .text
000000e0 <text_label\+e0> 381f000c	jle 0
		RELOC: 000000e4 R_ARC_32 .text
000000e8 <text_label\+e8> 381f000d	jhi 0
		RELOC: 000000ec R_ARC_32 .text
000000f0 <text_label\+f0> 381f000e	jls 0
		RELOC: 000000f4 R_ARC_32 .text
000000f8 <text_label\+f8> 381f000f	jpnz 0
		RELOC: 000000fc R_ARC_32 .text
00000100 <text_label\+100> 381f0000	j 0
		RELOC: 00000104 R_ARC_32 external_text_label
00000108 <text_label\+108> 381f0000	j 0
