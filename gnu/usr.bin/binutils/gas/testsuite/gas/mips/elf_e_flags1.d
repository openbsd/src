# name: ELF e_flags: nothing special
# source: elf_e_flags.s
# objdump: -fd

.*:.*file format.*mips.*
architecture: mips:.*, flags 0x00000011:
HAS_RELOC, HAS_SYMS
start address 0x0000000000000000

Disassembly of section .text:

0000000000000000 <foo>:
   0:	00850019 	multu	a0,a1
   4:	00001012 	mflo	v0
   8:	03e00008 	jr	ra
   c:	24420001 	addiu	v0,v0,1

0000000000000010 <main>:
  10:	27bdffd8 	addiu	sp,sp,-40
  14:	afbf0020 	sw	ra,32\(sp\)
  18:	0c000000 	jal	0 <foo>
  1c:	00000000 	nop
  20:	0000102[1d] 	move	v0,zero
  24:	8fbf0020 	lw	ra,32\(sp\)
  28:	00000000 	nop
  2c:	03e00008 	jr	ra
  30:	27bd0028 	addiu	sp,sp,40
	...
