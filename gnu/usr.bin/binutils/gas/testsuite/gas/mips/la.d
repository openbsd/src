#objdump: -dr
#name: MIPS la
#as: -mips1

# Test the la macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> li \$a0,0
0+0004 <[^>]*> li \$a0,1
0+0008 <[^>]*> li \$a0,32768
0+000c <[^>]*> li \$a0,-32768
0+0010 <[^>]*> lui \$a0,1
0+0014 <[^>]*> lui \$a0,1
0+0018 <[^>]*> ori \$a0,\$a0,42405
0+001c <[^>]*> li \$a0,0
0+0020 <[^>]*> addu \$a0,\$a0,\$a1
0+0024 <[^>]*> li \$a0,1
0+0028 <[^>]*> addu \$a0,\$a0,\$a1
0+002c <[^>]*> li \$a0,32768
0+0030 <[^>]*> addu \$a0,\$a0,\$a1
0+0034 <[^>]*> li \$a0,-32768
0+0038 <[^>]*> addu \$a0,\$a0,\$a1
0+003c <[^>]*> lui \$a0,1
0+0040 <[^>]*> addu \$a0,\$a0,\$a1
0+0044 <[^>]*> lui \$a0,1
0+0048 <[^>]*> ori \$a0,\$a0,42405
0+004c <[^>]*> addu \$a0,\$a0,\$a1
0+0050 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0058 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+005c <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+005c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0060 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0060 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0064 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0064 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0068 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0068 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+006c <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+006c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0070 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0074 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0074 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0078 <[^>]*> addiu \$a0,\$gp,[-0-9]+
[ 	]*RELOC: 0+0078 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+007c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0080 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0084 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0084 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0088 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0088 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+008c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0090 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0090 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0094 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0094 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0098 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+009c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+009c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00a0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00a4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00a4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00a8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00ac <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00ac [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00b0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00b4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00bc [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00c4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00c8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00cc <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00cc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00d0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00d4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00d8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00dc <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00dc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00e0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00e4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00e8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00ec <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00ec [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00f0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00f4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00f8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00fc <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00fc [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0100 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0100 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0104 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0104 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0108 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0108 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+010c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+010c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0110 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0110 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0114 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0118 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+011c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+011c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0120 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0120 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0124 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0124 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0128 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+012c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0130 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0130 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0134 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0134 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0138 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0138 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+013c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0140 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0140 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0144 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0148 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+014c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+014c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0150 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0150 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0154 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0154 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0158 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+015c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+015c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0160 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0160 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0164 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0164 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0168 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+016c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+016c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0170 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0170 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0174 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0178 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+017c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+017c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0180 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0180 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0184 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0184 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0188 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+018c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+018c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0190 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0190 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0194 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0194 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0198 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0198 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+019c <[^>]*> addu \$a0,\$a0,\$a1
0+01a0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01a0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a4 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> addu \$a0,\$a0,\$a1
0+01ac <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+01ac [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+01b0 <[^>]*> addu \$a0,\$a0,\$a1
0+01b4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01b8 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01bc <[^>]*> addu \$a0,\$a0,\$a1
0+01c0 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+01c4 <[^>]*> addu \$a0,\$a0,\$a1
0+01c8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01cc <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+01cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01d0 <[^>]*> addu \$a0,\$a0,\$a1
0+01d4 <[^>]*> addiu \$a0,\$gp,[-0-9]+
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+01d8 <[^>]*> addu \$a0,\$a0,\$a1
0+01dc <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01dc [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01e0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+01e0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01e4 <[^>]*> addu \$a0,\$a0,\$a1
0+01e8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01ec <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01f0 <[^>]*> addu \$a0,\$a0,\$a1
0+01f4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01f8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01fc <[^>]*> addu \$a0,\$a0,\$a1
0+0200 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0200 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0204 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0208 <[^>]*> addu \$a0,\$a0,\$a1
0+020c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+020c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0210 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0210 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0214 <[^>]*> addu \$a0,\$a0,\$a1
0+0218 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0218 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+021c <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+021c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0220 <[^>]*> addu \$a0,\$a0,\$a1
0+0224 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0224 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0228 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0228 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+022c <[^>]*> addu \$a0,\$a0,\$a1
0+0230 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0230 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0234 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0238 <[^>]*> addu \$a0,\$a0,\$a1
0+023c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+023c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0240 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0240 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0244 <[^>]*> addu \$a0,\$a0,\$a1
0+0248 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0248 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+024c <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0250 <[^>]*> addu \$a0,\$a0,\$a1
0+0254 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0254 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0258 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+025c <[^>]*> addu \$a0,\$a0,\$a1
0+0260 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0260 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0264 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0268 <[^>]*> addu \$a0,\$a0,\$a1
0+026c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+026c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0270 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0270 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0274 <[^>]*> addu \$a0,\$a0,\$a1
0+0278 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+027c <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+027c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0280 <[^>]*> addu \$a0,\$a0,\$a1
0+0284 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0284 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0288 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+028c <[^>]*> addu \$a0,\$a0,\$a1
0+0290 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0290 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0294 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0294 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0298 <[^>]*> addu \$a0,\$a0,\$a1
0+029c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+029c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02a0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02a0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+02a4 <[^>]*> addu \$a0,\$a0,\$a1
0+02a8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02ac <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02b0 <[^>]*> addu \$a0,\$a0,\$a1
0+02b4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02b8 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02bc <[^>]*> addu \$a0,\$a0,\$a1
0+02c0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02c0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c4 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> addu \$a0,\$a0,\$a1
0+02cc <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02cc [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d0 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02d0 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02d4 <[^>]*> addu \$a0,\$a0,\$a1
0+02d8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+02dc <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+02dc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02e0 <[^>]*> addu \$a0,\$a0,\$a1
0+02e4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+02e8 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+02e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02ec <[^>]*> addu \$a0,\$a0,\$a1
0+02f0 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02f0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02f4 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+02f4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+02f8 <[^>]*> addu \$a0,\$a0,\$a1
0+02fc <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02fc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0300 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0300 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0304 <[^>]*> addu \$a0,\$a0,\$a1
0+0308 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0308 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+030c <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+030c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0310 <[^>]*> addu \$a0,\$a0,\$a1
0+0314 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0314 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0318 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0318 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+031c <[^>]*> addu \$a0,\$a0,\$a1
0+0320 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0320 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0324 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0324 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0328 <[^>]*> addu \$a0,\$a0,\$a1
0+032c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+032c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0330 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0330 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0334 <[^>]*> addu \$a0,\$a0,\$a1
0+0338 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0338 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+033c <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+033c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0340 <[^>]*> addu \$a0,\$a0,\$a1
0+0344 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0344 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0348 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0348 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+034c <[^>]*> addu \$a0,\$a0,\$a1
0+0350 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0350 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0354 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0354 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0358 <[^>]*> addu \$a0,\$a0,\$a1
0+035c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+035c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0360 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0360 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0364 <[^>]*> addu \$a0,\$a0,\$a1
0+0368 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0368 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+036c <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+036c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0370 <[^>]*> addu \$a0,\$a0,\$a1
0+0374 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0374 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0378 <[^>]*> addiu \$a0,\$a0,[-0-9]+
[ 	]*RELOC: 0+0378 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+037c <[^>]*> addu \$a0,\$a0,\$a1
