#objdump: -dr
#name: st/sr

# Test the st/sr insn.

.*: +file format elf32-.*arc

Disassembly of section .text:
00000000 10008000	st r0,\[r1\]
00000004 10030a01	st r5,\[r6,1\]
00000008 10040fff	st r7,\[r8,-1\]
0000000c 100512ff	st r9,\[r10,255\]
00000010 10061700	st r11,\[r12,-256\]
00000014 101f2600	st r19,\[0\]
		RELOC: 00000018 R_ARC_32 foo
0000001c 101f2800	st r20,\[4\]
		RELOC: 00000020 R_ARC_32 foo
00000024 105f0000	stb r0,\[0\]
0000002c 109f0000	stw r0,\[0\]
00000034 111f0000	st.a r0,\[0\]
0000003c 141f0000	st.di r0,\[0\]
00000044 15400000	stb.a.di r0,\[r0\]
00000048 12008000	sr r0,\[r1\]
0000004c 121f8400	sr r2,\[status\]
00000050 121f0600	sr r3,\[305419896\]
