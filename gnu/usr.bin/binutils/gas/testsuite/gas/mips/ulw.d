#objdump: -dr
#name: MIPS ulw
#as: -mips1

# Test the ulw macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lwl \$a0,[03]\(\$zero\)
0+0004 <[^>]*> lwr \$a0,[03]\(\$zero\)
0+0008 <[^>]*> lwl \$a0,[14]\(\$zero\)
0+000c <[^>]*> lwr \$a0,[14]\(\$zero\)
0+0010 <[^>]*> li \$at,32768
0+0014 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0018 <[^>]*> lwr \$a0,[03]\(\$at\)
0+001c <[^>]*> lwl \$a0,-3276[58]\(\$zero\)
0+0020 <[^>]*> lwr \$a0,-3276[58]\(\$zero\)
0+0024 <[^>]*> lui \$at,1
0+0028 <[^>]*> lwl \$a0,[03]\(\$at\)
0+002c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0030 <[^>]*> lui \$at,1
0+0034 <[^>]*> ori \$at,\$at,42405
0+0038 <[^>]*> lwl \$a0,[03]\(\$at\)
0+003c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0040 <[^>]*> lwl \$a0,[03]\(\$a1\)
0+0044 <[^>]*> lwr \$a0,[03]\(\$a1\)
0+0048 <[^>]*> lwl \$a0,[14]\(\$a1\)
0+004c <[^>]*> lwr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> lwl \$a0,[03]\(\$at\)
0+005c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0060 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> lwl \$a0,[03]\(\$at\)
0+006c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0070 <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0078 <[^>]*> lwr \$a0,[03]\(\$at\)
0+007c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0088 <[^>]*> lwr \$a0,[03]\(\$at\)
0+008c <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0094 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+00a4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00a8 <[^>]*> addiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> lwl \$a0,[03]\(\$at\)
0+00b0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> lwl \$a0,[03]\(\$at\)
0+00c0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> lwl \$a0,[03]\(\$at\)
0+00d0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00dc <[^>]*> lwl \$a0,[03]\(\$at\)
0+00e0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00ec <[^>]*> lwl \$a0,[03]\(\$at\)
0+00f0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00f8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00fc <[^>]*> lwl \$a0,[03]\(\$at\)
0+0100 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0104 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0104 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0108 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0108 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+010c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0110 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0114 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0118 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+011c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0120 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0124 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0124 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0128 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0130 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0134 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0134 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+013c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0140 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0144 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0148 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+014c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0150 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0154 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0154 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0158 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+015c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0160 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0164 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0164 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0168 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+016c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0170 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0174 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0178 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+017c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0180 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0184 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0184 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0188 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+018c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0190 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0194 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0194 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0198 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0198 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+019c <[^>]*> lwl \$a0,[03]\(\$at\)
0+01a0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01ac <[^>]*> lwl \$a0,[03]\(\$at\)
0+01b0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01bc <[^>]*> lwl \$a0,[03]\(\$at\)
0+01c0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01cc <[^>]*> lwl \$a0,[03]\(\$at\)
0+01d0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01dc <[^>]*> lwl \$a0,[03]\(\$at\)
0+01e0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01e8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01ec <[^>]*> lwl \$a0,[03]\(\$at\)
0+01f0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01f8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01fc <[^>]*> lwl \$a0,[03]\(\$at\)
0+0200 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0208 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+020c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0210 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0214 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0214 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0218 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+021c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0220 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0224 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0224 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0228 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+022c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0230 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0238 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+023c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0240 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0244 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0244 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0248 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+024c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0250 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0254 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0254 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0258 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+025c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0260 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0270 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0274 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0274 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0278 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+027c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0280 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0284 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0284 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0288 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+028c <[^>]*> lwl \$a0,[03]\(\$at\)
0+0290 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0294 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0298 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+029c <[^>]*> lwl \$a0,[03]\(\$at\)
0+02a0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02a4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02a8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02ac <[^>]*> lwl \$a0,[03]\(\$at\)
0+02b0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02b4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02bc <[^>]*> lwl \$a0,[03]\(\$at\)
0+02c0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02c4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02cc <[^>]*> lwl \$a0,[03]\(\$at\)
0+02d0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02d4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> lwl \$a0,[03]\(\$at\)
0+02e0 <[^>]*> lwr \$a0,[03]\(\$at\)
...
