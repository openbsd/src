#objdump: -dr
#name: MIPS ulh-svr4pic
#as: -mips1 -KPIC
#source: ulh-pic.s

# Test the unaligned load and store macros with -KPIC.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0000 R_MIPS_GOT16 .data
...
0+0008 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0008 R_MIPS_LO16 .data
0+000c <[^>]*> lb \$a0,0\(\$at\)
0+0010 <[^>]*> lbu \$at,1\(\$at\)
0+0014 <[^>]*> sll \$a0,\$a0,0x8
0+0018 <[^>]*> or \$a0,\$a0,\$at
0+001c <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+001c R_MIPS_GOT16 big_external_data_label
...
0+0024 <[^>]*> lbu \$a0,0\(\$at\)
0+0028 <[^>]*> lbu \$at,1\(\$at\)
0+002c <[^>]*> sll \$a0,\$a0,0x8
0+0030 <[^>]*> or \$a0,\$a0,\$at
0+0034 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0034 R_MIPS_GOT16 small_external_data_label
...
0+003c <[^>]*> lwl \$a0,0\(\$at\)
0+0040 <[^>]*> lwr \$a0,3\(\$at\)
0+0044 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0044 R_MIPS_GOT16 big_external_common
...
0+004c <[^>]*> sb \$a0,1\(\$at\)
0+0050 <[^>]*> srl \$a0,\$a0,0x8
0+0054 <[^>]*> sb \$a0,0\(\$at\)
0+0058 <[^>]*> lbu \$at,1\(\$at\)
0+005c <[^>]*> sll \$a0,\$a0,0x8
0+0060 <[^>]*> or \$a0,\$a0,\$at
0+0064 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0064 R_MIPS_GOT16 small_external_common
...
0+006c <[^>]*> swl \$a0,0\(\$at\)
0+0070 <[^>]*> swr \$a0,3\(\$at\)
0+0074 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0074 R_MIPS_GOT16 .bss
...
0+007c <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+007c R_MIPS_LO16 .bss
0+0080 <[^>]*> lb \$a0,0\(\$at\)
0+0084 <[^>]*> lbu \$at,1\(\$at\)
0+0088 <[^>]*> sll \$a0,\$a0,0x8
0+008c <[^>]*> or \$a0,\$a0,\$at
0+0090 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0090 R_MIPS_GOT16 .bss
...
0+0098 <[^>]*> addiu \$at,\$at,1000
[ 	]*RELOC: 0+0098 R_MIPS_LO16 .bss
0+009c <[^>]*> lbu \$a0,0\(\$at\)
0+00a0 <[^>]*> lbu \$at,1\(\$at\)
0+00a4 <[^>]*> sll \$a0,\$a0,0x8
0+00a8 <[^>]*> or \$a0,\$a0,\$at
0+00ac <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+00ac R_MIPS_GOT16 .data
...
0+00b4 <[^>]*> addiu \$at,\$at,1
[ 	]*RELOC: 0+00b4 R_MIPS_LO16 .data
0+00b8 <[^>]*> lwl \$a0,0\(\$at\)
0+00bc <[^>]*> lwr \$a0,3\(\$at\)
0+00c0 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+00c0 R_MIPS_GOT16 big_external_data_label
...
0+00c8 <[^>]*> sb \$a0,1\(\$at\)
0+00cc <[^>]*> srl \$a0,\$a0,0x8
0+00d0 <[^>]*> sb \$a0,0\(\$at\)
0+00d4 <[^>]*> lbu \$at,1\(\$at\)
0+00d8 <[^>]*> sll \$a0,\$a0,0x8
0+00dc <[^>]*> or \$a0,\$a0,\$at
0+00e0 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+00e0 R_MIPS_GOT16 small_external_data_label
...
0+00e8 <[^>]*> swl \$a0,0\(\$at\)
0+00ec <[^>]*> swr \$a0,3\(\$at\)
0+00f0 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+00f0 R_MIPS_GOT16 big_external_common
...
0+00f8 <[^>]*> lb \$a0,0\(\$at\)
0+00fc <[^>]*> lbu \$at,1\(\$at\)
0+0100 <[^>]*> sll \$a0,\$a0,0x8
0+0104 <[^>]*> or \$a0,\$a0,\$at
0+0108 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0108 R_MIPS_GOT16 small_external_common
...
0+0110 <[^>]*> lbu \$a0,0\(\$at\)
0+0114 <[^>]*> lbu \$at,1\(\$at\)
0+0118 <[^>]*> sll \$a0,\$a0,0x8
0+011c <[^>]*> or \$a0,\$a0,\$at
0+0120 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0120 R_MIPS_GOT16 .bss
...
0+0128 <[^>]*> addiu \$at,\$at,1
[ 	]*RELOC: 0+0128 R_MIPS_LO16 .bss
0+012c <[^>]*> lwl \$a0,0\(\$at\)
0+0130 <[^>]*> lwr \$a0,3\(\$at\)
0+0134 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0134 R_MIPS_GOT16 .bss
...
0+013c <[^>]*> addiu \$at,\$at,1001
[ 	]*RELOC: 0+013c R_MIPS_LO16 .bss
0+0140 <[^>]*> sb \$a0,1\(\$at\)
0+0144 <[^>]*> srl \$a0,\$a0,0x8
0+0148 <[^>]*> sb \$a0,0\(\$at\)
0+014c <[^>]*> lbu \$at,1\(\$at\)
0+0150 <[^>]*> sll \$a0,\$a0,0x8
0+0154 <[^>]*> or \$a0,\$a0,\$at
...
