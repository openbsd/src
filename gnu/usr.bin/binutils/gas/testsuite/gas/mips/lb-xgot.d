#objdump: -dr
#name: MIPS lb-xgot
#as: -mips1 -KPIC -xgot
#source: lb-pic.s

# Test the lb macro with -KPIC -xgot.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lb \$a0,0\(\$zero\)
0+0004 <[^>]*> lb \$a0,1\(\$zero\)
0+0008 <[^>]*> lui \$a0,0x1
0+000c <[^>]*> lb \$a0,-32768\(\$a0\)
0+0010 <[^>]*> lb \$a0,-32768\(\$zero\)
0+0014 <[^>]*> lui \$a0,0x1
0+0018 <[^>]*> lb \$a0,0\(\$a0\)
0+001c <[^>]*> lui \$a0,0x2
0+0020 <[^>]*> lb \$a0,-23131\(\$a0\)
0+0024 <[^>]*> lb \$a0,0\(\$a1\)
0+0028 <[^>]*> lb \$a0,1\(\$a1\)
0+002c <[^>]*> lui \$a0,0x1
0+0030 <[^>]*> addu \$a0,\$a0,\$a1
0+0034 <[^>]*> lb \$a0,-32768\(\$a0\)
0+0038 <[^>]*> lb \$a0,-32768\(\$a1\)
0+003c <[^>]*> lui \$a0,0x1
0+0040 <[^>]*> addu \$a0,\$a0,\$a1
0+0044 <[^>]*> lb \$a0,0\(\$a0\)
0+0048 <[^>]*> lui \$a0,0x2
0+004c <[^>]*> addu \$a0,\$a0,\$a1
0+0050 <[^>]*> lb \$a0,-23131\(\$a0\)
0+0054 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0054 R_MIPS_GOT16 .data
...
0+005c <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+005c R_MIPS_LO16 .data
...
0+0064 <[^>]*> lb \$a0,0\(\$a0\)
0+0068 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0068 R_MIPS_GOT_HI16 big_external_data_label
0+006c <[^>]*> addu \$a0,\$a0,\$gp
0+0070 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0070 R_MIPS_GOT_LO16 big_external_data_label
...
0+0078 <[^>]*> lb \$a0,0\(\$a0\)
0+007c <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+007c R_MIPS_GOT_HI16 small_external_data_label
0+0080 <[^>]*> addu \$a0,\$a0,\$gp
0+0084 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0084 R_MIPS_GOT_LO16 small_external_data_label
...
0+008c <[^>]*> lb \$a0,0\(\$a0\)
0+0090 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0090 R_MIPS_GOT_HI16 big_external_common
0+0094 <[^>]*> addu \$a0,\$a0,\$gp
0+0098 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0098 R_MIPS_GOT_LO16 big_external_common
...
0+00a0 <[^>]*> lb \$a0,0\(\$a0\)
0+00a4 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+00a4 R_MIPS_GOT_HI16 small_external_common
0+00a8 <[^>]*> addu \$a0,\$a0,\$gp
0+00ac <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+00ac R_MIPS_GOT_LO16 small_external_common
...
0+00b4 <[^>]*> lb \$a0,0\(\$a0\)
0+00b8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00b8 R_MIPS_GOT16 .bss
...
0+00c0 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+00c0 R_MIPS_LO16 .bss
...
0+00c8 <[^>]*> lb \$a0,0\(\$a0\)
0+00cc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00cc R_MIPS_GOT16 .bss
...
0+00d4 <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+00d4 R_MIPS_LO16 .bss
...
0+00dc <[^>]*> lb \$a0,0\(\$a0\)
0+00e0 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00e0 R_MIPS_GOT16 .data
...
0+00e8 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+00e8 R_MIPS_LO16 .data
...
0+00f0 <[^>]*> lb \$a0,1\(\$a0\)
0+00f4 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+00f4 R_MIPS_GOT_HI16 big_external_data_label
0+00f8 <[^>]*> addu \$a0,\$a0,\$gp
0+00fc <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+00fc R_MIPS_GOT_LO16 big_external_data_label
...
0+0104 <[^>]*> lb \$a0,1\(\$a0\)
0+0108 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0108 R_MIPS_GOT_HI16 small_external_data_label
0+010c <[^>]*> addu \$a0,\$a0,\$gp
0+0110 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0110 R_MIPS_GOT_LO16 small_external_data_label
...
0+0118 <[^>]*> lb \$a0,1\(\$a0\)
0+011c <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+011c R_MIPS_GOT_HI16 big_external_common
0+0120 <[^>]*> addu \$a0,\$a0,\$gp
0+0124 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0124 R_MIPS_GOT_LO16 big_external_common
...
0+012c <[^>]*> lb \$a0,1\(\$a0\)
0+0130 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0130 R_MIPS_GOT_HI16 small_external_common
0+0134 <[^>]*> addu \$a0,\$a0,\$gp
0+0138 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0138 R_MIPS_GOT_LO16 small_external_common
...
0+0140 <[^>]*> lb \$a0,1\(\$a0\)
0+0144 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0144 R_MIPS_GOT16 .bss
...
0+014c <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+014c R_MIPS_LO16 .bss
...
0+0154 <[^>]*> lb \$a0,1\(\$a0\)
0+0158 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0158 R_MIPS_GOT16 .bss
...
0+0160 <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+0160 R_MIPS_LO16 .bss
...
0+0168 <[^>]*> lb \$a0,1\(\$a0\)
0+016c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+016c R_MIPS_GOT16 .data
...
0+0174 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0174 R_MIPS_LO16 .data
...
0+017c <[^>]*> addu \$a0,\$a0,\$a1
0+0180 <[^>]*> lb \$a0,0\(\$a0\)
0+0184 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0184 R_MIPS_GOT_HI16 big_external_data_label
0+0188 <[^>]*> addu \$a0,\$a0,\$gp
0+018c <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+018c R_MIPS_GOT_LO16 big_external_data_label
...
0+0194 <[^>]*> addu \$a0,\$a0,\$a1
0+0198 <[^>]*> lb \$a0,0\(\$a0\)
0+019c <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+019c R_MIPS_GOT_HI16 small_external_data_label
0+01a0 <[^>]*> addu \$a0,\$a0,\$gp
0+01a4 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01a4 R_MIPS_GOT_LO16 small_external_data_label
...
0+01ac <[^>]*> addu \$a0,\$a0,\$a1
0+01b0 <[^>]*> lb \$a0,0\(\$a0\)
0+01b4 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+01b4 R_MIPS_GOT_HI16 big_external_common
0+01b8 <[^>]*> addu \$a0,\$a0,\$gp
0+01bc <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01bc R_MIPS_GOT_LO16 big_external_common
...
0+01c4 <[^>]*> addu \$a0,\$a0,\$a1
0+01c8 <[^>]*> lb \$a0,0\(\$a0\)
0+01cc <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+01cc R_MIPS_GOT_HI16 small_external_common
0+01d0 <[^>]*> addu \$a0,\$a0,\$gp
0+01d4 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01d4 R_MIPS_GOT_LO16 small_external_common
...
0+01dc <[^>]*> addu \$a0,\$a0,\$a1
0+01e0 <[^>]*> lb \$a0,0\(\$a0\)
0+01e4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01e4 R_MIPS_GOT16 .bss
...
0+01ec <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+01ec R_MIPS_LO16 .bss
...
0+01f4 <[^>]*> addu \$a0,\$a0,\$a1
0+01f8 <[^>]*> lb \$a0,0\(\$a0\)
0+01fc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01fc R_MIPS_GOT16 .bss
...
0+0204 <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+0204 R_MIPS_LO16 .bss
...
0+020c <[^>]*> addu \$a0,\$a0,\$a1
0+0210 <[^>]*> lb \$a0,0\(\$a0\)
0+0214 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0214 R_MIPS_GOT16 .data
...
0+021c <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+021c R_MIPS_LO16 .data
...
0+0224 <[^>]*> addu \$a0,\$a0,\$a1
0+0228 <[^>]*> lb \$a0,1\(\$a0\)
0+022c <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+022c R_MIPS_GOT_HI16 big_external_data_label
0+0230 <[^>]*> addu \$a0,\$a0,\$gp
0+0234 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0234 R_MIPS_GOT_LO16 big_external_data_label
...
0+023c <[^>]*> addu \$a0,\$a0,\$a1
0+0240 <[^>]*> lb \$a0,1\(\$a0\)
0+0244 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0244 R_MIPS_GOT_HI16 small_external_data_label
0+0248 <[^>]*> addu \$a0,\$a0,\$gp
0+024c <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+024c R_MIPS_GOT_LO16 small_external_data_label
...
0+0254 <[^>]*> addu \$a0,\$a0,\$a1
0+0258 <[^>]*> lb \$a0,1\(\$a0\)
0+025c <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+025c R_MIPS_GOT_HI16 big_external_common
0+0260 <[^>]*> addu \$a0,\$a0,\$gp
0+0264 <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0264 R_MIPS_GOT_LO16 big_external_common
...
0+026c <[^>]*> addu \$a0,\$a0,\$a1
0+0270 <[^>]*> lb \$a0,1\(\$a0\)
0+0274 <[^>]*> lui \$a0,0x0
[ 	]*RELOC: 0+0274 R_MIPS_GOT_HI16 small_external_common
0+0278 <[^>]*> addu \$a0,\$a0,\$gp
0+027c <[^>]*> lw \$a0,0\(\$a0\)
[ 	]*RELOC: 0+027c R_MIPS_GOT_LO16 small_external_common
...
0+0284 <[^>]*> addu \$a0,\$a0,\$a1
0+0288 <[^>]*> lb \$a0,1\(\$a0\)
0+028c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+028c R_MIPS_GOT16 .bss
...
0+0294 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0294 R_MIPS_LO16 .bss
...
0+029c <[^>]*> addu \$a0,\$a0,\$a1
0+02a0 <[^>]*> lb \$a0,1\(\$a0\)
0+02a4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02a4 R_MIPS_GOT16 .bss
...
0+02ac <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+02ac R_MIPS_LO16 .bss
...
0+02b4 <[^>]*> addu \$a0,\$a0,\$a1
0+02b8 <[^>]*> lb \$a0,1\(\$a0\)
...
