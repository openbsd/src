#objdump: -dr
#name: MIPS uld
#as: -mips3

# Test the uld macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> ldl \$a0,[07]\(\$zero\)
0+0004 <[^>]*> ldr \$a0,[07]\(\$zero\)
0+0008 <[^>]*> ldl \$a0,[18]\(\$zero\)
0+000c <[^>]*> ldr \$a0,[18]\(\$zero\)
0+0010 <[^>]*> li \$at,32768
0+0014 <[^>]*> ldl \$a0,[07]\(\$at\)
0+0018 <[^>]*> ldr \$a0,[07]\(\$at\)
0+001c <[^>]*> ldl \$a0,-3276[18]\(\$zero\)
0+0020 <[^>]*> ldr \$a0,-3276[18]\(\$zero\)
0+0024 <[^>]*> lui \$at,1
0+0028 <[^>]*> ldl \$a0,[07]\(\$at\)
0+002c <[^>]*> ldr \$a0,[07]\(\$at\)
0+0030 <[^>]*> lui \$at,1
0+0034 <[^>]*> ori \$at,\$at,42405
0+0038 <[^>]*> ldl \$a0,[07]\(\$at\)
0+003c <[^>]*> ldr \$a0,[07]\(\$at\)
0+0040 <[^>]*> ldl \$a0,[07]\(\$a1\)
0+0044 <[^>]*> ldr \$a0,[07]\(\$a1\)
0+0048 <[^>]*> ldl \$a0,[18]\(\$a1\)
0+004c <[^>]*> ldr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> ldl \$a0,[07]\(\$at\)
0+005c <[^>]*> ldr \$a0,[07]\(\$at\)
0+0060 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> ldl \$a0,[07]\(\$at\)
0+006c <[^>]*> ldr \$a0,[07]\(\$at\)
0+0070 <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> ldl \$a0,[07]\(\$at\)
0+0078 <[^>]*> ldr \$a0,[07]\(\$at\)
0+007c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> ldl \$a0,[07]\(\$at\)
0+0088 <[^>]*> ldr \$a0,[07]\(\$at\)
0+008c <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> ldl \$a0,[07]\(\$at\)
0+0094 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> ldl \$a0,[07]\(\$at\)
0+00a4 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00a8 <[^>]*> daddiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> ldl \$a0,[07]\(\$at\)
0+00b0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> ldl \$a0,[07]\(\$at\)
0+00c0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> ldl \$a0,[07]\(\$at\)
0+00d0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00dc <[^>]*> ldl \$a0,[07]\(\$at\)
0+00e0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00ec <[^>]*> ldl \$a0,[07]\(\$at\)
0+00f0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+00f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00f8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00fc <[^>]*> ldl \$a0,[07]\(\$at\)
0+0100 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0104 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0104 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0108 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0108 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+010c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0110 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0114 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0118 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+011c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0120 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0124 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0124 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0128 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0130 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0134 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0134 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+013c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0140 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0144 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0148 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+014c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0150 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0154 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0154 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0158 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+015c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0160 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0164 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0164 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0168 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+016c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0170 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0174 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0178 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+017c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0180 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0184 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0184 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0188 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+018c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0190 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0194 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0194 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0198 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0198 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+019c <[^>]*> ldl \$a0,[07]\(\$at\)
0+01a0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01ac <[^>]*> ldl \$a0,[07]\(\$at\)
0+01b0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01bc <[^>]*> ldl \$a0,[07]\(\$at\)
0+01c0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01cc <[^>]*> ldl \$a0,[07]\(\$at\)
0+01d0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01dc <[^>]*> ldl \$a0,[07]\(\$at\)
0+01e0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01e8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01ec <[^>]*> ldl \$a0,[07]\(\$at\)
0+01f0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+01f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01f8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01fc <[^>]*> ldl \$a0,[07]\(\$at\)
0+0200 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0208 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+020c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0210 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0214 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0214 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0218 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+021c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0220 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0224 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0224 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0228 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+022c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0230 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0238 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+023c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0240 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0244 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0244 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0248 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+024c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0250 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0254 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0254 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0258 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+025c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0260 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0270 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0274 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0274 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0278 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+027c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0280 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0284 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0284 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0288 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+028c <[^>]*> ldl \$a0,[07]\(\$at\)
0+0290 <[^>]*> ldr \$a0,[07]\(\$at\)
0+0294 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0298 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+029c <[^>]*> ldl \$a0,[07]\(\$at\)
0+02a0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+02a4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02a8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02ac <[^>]*> ldl \$a0,[07]\(\$at\)
0+02b0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+02b4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02bc <[^>]*> ldl \$a0,[07]\(\$at\)
0+02c0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+02c4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02cc <[^>]*> ldl \$a0,[07]\(\$at\)
0+02d0 <[^>]*> ldr \$a0,[07]\(\$at\)
0+02d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02d4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> ldl \$a0,[07]\(\$at\)
0+02e0 <[^>]*> ldr \$a0,[07]\(\$at\)
...
