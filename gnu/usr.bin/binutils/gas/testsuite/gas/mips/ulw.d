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
0+0010 <[^>]*> li \$at,0x8000
0+0014 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0018 <[^>]*> lwr \$a0,[03]\(\$at\)
0+001c <[^>]*> lwl \$a0,-3276[58]\(\$zero\)
0+0020 <[^>]*> lwr \$a0,-3276[58]\(\$zero\)
0+0024 <[^>]*> lui \$at,0x1
0+0028 <[^>]*> lwl \$a0,[03]\(\$at\)
0+002c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0030 <[^>]*> lui \$at,0x1
0+0034 <[^>]*> ori \$at,\$at,0xa5a5
0+0038 <[^>]*> lwl \$a0,[03]\(\$at\)
0+003c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0040 <[^>]*> lwl \$a0,[03]\(\$a1\)
0+0044 <[^>]*> lwr \$a0,[03]\(\$a1\)
0+0048 <[^>]*> lwl \$a0,[14]\(\$a1\)
0+004c <[^>]*> lwr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> lwl \$a0,[03]\(\$at\)
0+005c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0060 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> lwl \$a0,[03]\(\$at\)
0+006c <[^>]*> lwr \$a0,[03]\(\$at\)
0+0070 <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0078 <[^>]*> lwr \$a0,[03]\(\$at\)
0+007c <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0088 <[^>]*> lwr \$a0,[03]\(\$at\)
0+008c <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0094 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+00a4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00a8 <[^>]*> addiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> lwl \$a0,[03]\(\$at\)
0+00b0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00b4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> lwl \$a0,[03]\(\$at\)
0+00c0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00c4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> lwl \$a0,[03]\(\$at\)
0+00d0 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00d4 <[^>]*> addiu \$at,\$gp,1
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> lwl \$a0,[03]\(\$at\)
0+00dc <[^>]*> lwr \$a0,[03]\(\$at\)
0+00e0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e4 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> lwl \$a0,[03]\(\$at\)
0+00ec <[^>]*> lwr \$a0,[03]\(\$at\)
0+00f0 <[^>]*> addiu \$at,\$gp,1
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00f4 <[^>]*> lwl \$a0,[03]\(\$at\)
0+00f8 <[^>]*> lwr \$a0,[03]\(\$at\)
0+00fc <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00fc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0100 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0100 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0104 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0108 <[^>]*> lwr \$a0,[03]\(\$at\)
0+010c <[^>]*> addiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+010c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0110 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0114 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0118 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+011c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+011c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0120 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0124 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0128 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+012c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+012c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0130 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0134 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0138 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0140 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0144 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0148 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+014c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0150 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0154 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0158 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+015c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+015c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0160 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0164 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0168 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+016c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0170 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0174 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0178 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+017c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0180 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0184 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0188 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0188 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+018c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+018c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0190 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0194 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0198 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+019c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+019c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01a0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01a4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01a8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01ac <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01b0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01b4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01b8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01bc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01bc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01c0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01c4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01c8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01cc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01cc [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01d0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01d4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01d8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01dc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01dc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01e0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01e4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01e8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01ec <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01f0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+01f4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+01f8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01fc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01fc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0200 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0204 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0208 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+020c <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0210 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0214 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0218 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0218 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+021c <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+021c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0220 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0224 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0228 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+022c <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+022c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0230 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0234 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0238 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0238 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+023c <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0240 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0244 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0248 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0248 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+024c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0250 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0254 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0258 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+025c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+025c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0260 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0264 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0268 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+026c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0270 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0274 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0278 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+027c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+027c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0280 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0284 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0288 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+028c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+028c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0290 <[^>]*> lwl \$a0,[03]\(\$at\)
0+0294 <[^>]*> lwr \$a0,[03]\(\$at\)
0+0298 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+029c <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+029c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02a0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+02a4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02a8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02ac <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02ac [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02b0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+02b4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02b8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02bc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02bc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02c0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+02c4 <[^>]*> lwr \$a0,[03]\(\$at\)
0+02c8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02cc <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02cc [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02d0 <[^>]*> lwl \$a0,[03]\(\$at\)
0+02d4 <[^>]*> lwr \$a0,[03]\(\$at\)
...
