#objdump: -dr
#name: MIPS usw
#as: -mips1

# Test the usw macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> swl \$a0,[03]\(\$zero\)
0+0004 <[^>]*> swr \$a0,[03]\(\$zero\)
0+0008 <[^>]*> swl \$a0,[14]\(\$zero\)
0+000c <[^>]*> swr \$a0,[14]\(\$zero\)
0+0010 <[^>]*> li \$at,32768
0+0014 <[^>]*> swl \$a0,[03]\(\$at\)
0+0018 <[^>]*> swr \$a0,[03]\(\$at\)
0+001c <[^>]*> swl \$a0,-3276[58]\(\$zero\)
0+0020 <[^>]*> swr \$a0,-3276[58]\(\$zero\)
0+0024 <[^>]*> lui \$at,1
0+0028 <[^>]*> swl \$a0,[03]\(\$at\)
0+002c <[^>]*> swr \$a0,[03]\(\$at\)
0+0030 <[^>]*> lui \$at,1
0+0034 <[^>]*> ori \$at,\$at,42405
0+0038 <[^>]*> swl \$a0,[03]\(\$at\)
0+003c <[^>]*> swr \$a0,[03]\(\$at\)
0+0040 <[^>]*> swl \$a0,[03]\(\$a1\)
0+0044 <[^>]*> swr \$a0,[03]\(\$a1\)
0+0048 <[^>]*> swl \$a0,[14]\(\$a1\)
0+004c <[^>]*> swr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> swl \$a0,[03]\(\$at\)
0+005c <[^>]*> swr \$a0,[03]\(\$at\)
0+0060 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> swl \$a0,[03]\(\$at\)
0+006c <[^>]*> swr \$a0,[03]\(\$at\)
0+0070 <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> swl \$a0,[03]\(\$at\)
0+0078 <[^>]*> swr \$a0,[03]\(\$at\)
0+007c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> swl \$a0,[03]\(\$at\)
0+0088 <[^>]*> swr \$a0,[03]\(\$at\)
0+008c <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> swl \$a0,[03]\(\$at\)
0+0094 <[^>]*> swr \$a0,[03]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> swl \$a0,[03]\(\$at\)
0+00a4 <[^>]*> swr \$a0,[03]\(\$at\)
0+00a8 <[^>]*> addiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> swl \$a0,[03]\(\$at\)
0+00b0 <[^>]*> swr \$a0,[03]\(\$at\)
0+00b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> swl \$a0,[03]\(\$at\)
0+00c0 <[^>]*> swr \$a0,[03]\(\$at\)
0+00c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> swl \$a0,[03]\(\$at\)
0+00d0 <[^>]*> swr \$a0,[03]\(\$at\)
0+00d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00dc <[^>]*> swl \$a0,[03]\(\$at\)
0+00e0 <[^>]*> swr \$a0,[03]\(\$at\)
0+00e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00ec <[^>]*> swl \$a0,[03]\(\$at\)
0+00f0 <[^>]*> swr \$a0,[03]\(\$at\)
0+00f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00f8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00fc <[^>]*> swl \$a0,[03]\(\$at\)
0+0100 <[^>]*> swr \$a0,[03]\(\$at\)
0+0104 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0104 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0108 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0108 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+010c <[^>]*> swl \$a0,[03]\(\$at\)
0+0110 <[^>]*> swr \$a0,[03]\(\$at\)
0+0114 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0118 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+011c <[^>]*> swl \$a0,[03]\(\$at\)
0+0120 <[^>]*> swr \$a0,[03]\(\$at\)
0+0124 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0124 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0128 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> swl \$a0,[03]\(\$at\)
0+0130 <[^>]*> swr \$a0,[03]\(\$at\)
0+0134 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0134 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+013c <[^>]*> swl \$a0,[03]\(\$at\)
0+0140 <[^>]*> swr \$a0,[03]\(\$at\)
0+0144 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0148 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+014c <[^>]*> swl \$a0,[03]\(\$at\)
0+0150 <[^>]*> swr \$a0,[03]\(\$at\)
0+0154 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0154 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0158 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+015c <[^>]*> swl \$a0,[03]\(\$at\)
0+0160 <[^>]*> swr \$a0,[03]\(\$at\)
0+0164 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0164 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0168 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+016c <[^>]*> swl \$a0,[03]\(\$at\)
0+0170 <[^>]*> swr \$a0,[03]\(\$at\)
0+0174 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0178 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+017c <[^>]*> swl \$a0,[03]\(\$at\)
0+0180 <[^>]*> swr \$a0,[03]\(\$at\)
0+0184 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0184 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0188 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+018c <[^>]*> swl \$a0,[03]\(\$at\)
0+0190 <[^>]*> swr \$a0,[03]\(\$at\)
0+0194 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0194 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0198 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0198 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+019c <[^>]*> swl \$a0,[03]\(\$at\)
0+01a0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01ac <[^>]*> swl \$a0,[03]\(\$at\)
0+01b0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01bc <[^>]*> swl \$a0,[03]\(\$at\)
0+01c0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01cc <[^>]*> swl \$a0,[03]\(\$at\)
0+01d0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01dc <[^>]*> swl \$a0,[03]\(\$at\)
0+01e0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01e8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01ec <[^>]*> swl \$a0,[03]\(\$at\)
0+01f0 <[^>]*> swr \$a0,[03]\(\$at\)
0+01f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01f8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01fc <[^>]*> swl \$a0,[03]\(\$at\)
0+0200 <[^>]*> swr \$a0,[03]\(\$at\)
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0208 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+020c <[^>]*> swl \$a0,[03]\(\$at\)
0+0210 <[^>]*> swr \$a0,[03]\(\$at\)
0+0214 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0214 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0218 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+021c <[^>]*> swl \$a0,[03]\(\$at\)
0+0220 <[^>]*> swr \$a0,[03]\(\$at\)
0+0224 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0224 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0228 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+022c <[^>]*> swl \$a0,[03]\(\$at\)
0+0230 <[^>]*> swr \$a0,[03]\(\$at\)
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0238 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+023c <[^>]*> swl \$a0,[03]\(\$at\)
0+0240 <[^>]*> swr \$a0,[03]\(\$at\)
0+0244 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0244 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0248 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+024c <[^>]*> swl \$a0,[03]\(\$at\)
0+0250 <[^>]*> swr \$a0,[03]\(\$at\)
0+0254 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0254 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0258 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+025c <[^>]*> swl \$a0,[03]\(\$at\)
0+0260 <[^>]*> swr \$a0,[03]\(\$at\)
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> swl \$a0,[03]\(\$at\)
0+0270 <[^>]*> swr \$a0,[03]\(\$at\)
0+0274 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0274 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0278 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+027c <[^>]*> swl \$a0,[03]\(\$at\)
0+0280 <[^>]*> swr \$a0,[03]\(\$at\)
0+0284 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0284 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0288 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+028c <[^>]*> swl \$a0,[03]\(\$at\)
0+0290 <[^>]*> swr \$a0,[03]\(\$at\)
0+0294 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0298 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+029c <[^>]*> swl \$a0,[03]\(\$at\)
0+02a0 <[^>]*> swr \$a0,[03]\(\$at\)
0+02a4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02a8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02ac <[^>]*> swl \$a0,[03]\(\$at\)
0+02b0 <[^>]*> swr \$a0,[03]\(\$at\)
0+02b4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02bc <[^>]*> swl \$a0,[03]\(\$at\)
0+02c0 <[^>]*> swr \$a0,[03]\(\$at\)
0+02c4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02cc <[^>]*> swl \$a0,[03]\(\$at\)
0+02d0 <[^>]*> swr \$a0,[03]\(\$at\)
0+02d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02d4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> swl \$a0,[03]\(\$at\)
0+02e0 <[^>]*> swr \$a0,[03]\(\$at\)
...
