#objdump: -dr
#name: MIPS usd
#as: -mips3

# Test the usd macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> sdl \$a0,[07]\(\$zero\)
0+0004 <[^>]*> sdr \$a0,[07]\(\$zero\)
0+0008 <[^>]*> sdl \$a0,[18]\(\$zero\)
0+000c <[^>]*> sdr \$a0,[18]\(\$zero\)
0+0010 <[^>]*> li \$at,32768
0+0014 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0018 <[^>]*> sdr \$a0,[07]\(\$at\)
0+001c <[^>]*> sdl \$a0,-3276[18]\(\$zero\)
0+0020 <[^>]*> sdr \$a0,-3276[18]\(\$zero\)
0+0024 <[^>]*> lui \$at,1
0+0028 <[^>]*> sdl \$a0,[07]\(\$at\)
0+002c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0030 <[^>]*> lui \$at,1
0+0034 <[^>]*> ori \$at,\$at,42405
0+0038 <[^>]*> sdl \$a0,[07]\(\$at\)
0+003c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0040 <[^>]*> sdl \$a0,[07]\(\$a1\)
0+0044 <[^>]*> sdr \$a0,[07]\(\$a1\)
0+0048 <[^>]*> sdl \$a0,[18]\(\$a1\)
0+004c <[^>]*> sdr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> sdl \$a0,[07]\(\$at\)
0+005c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0060 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> sdl \$a0,[07]\(\$at\)
0+006c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0070 <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0078 <[^>]*> sdr \$a0,[07]\(\$at\)
0+007c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0088 <[^>]*> sdr \$a0,[07]\(\$at\)
0+008c <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0094 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+00a4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00a8 <[^>]*> daddiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> sdl \$a0,[07]\(\$at\)
0+00b0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> sdl \$a0,[07]\(\$at\)
0+00c0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> sdl \$a0,[07]\(\$at\)
0+00d0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00dc <[^>]*> sdl \$a0,[07]\(\$at\)
0+00e0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00ec <[^>]*> sdl \$a0,[07]\(\$at\)
0+00f0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00f8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00fc <[^>]*> sdl \$a0,[07]\(\$at\)
0+0100 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0104 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0104 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0108 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0108 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+010c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0110 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0114 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0118 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+011c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0120 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0124 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0124 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0128 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0130 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0134 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0134 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+013c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0140 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0144 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0148 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+014c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0150 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0154 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0154 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0158 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+015c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0160 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0164 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0164 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0168 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+016c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0170 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0174 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0178 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+017c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0180 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0184 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0184 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0188 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+018c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0190 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0194 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0194 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0198 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0198 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+019c <[^>]*> sdl \$a0,[07]\(\$at\)
0+01a0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01ac <[^>]*> sdl \$a0,[07]\(\$at\)
0+01b0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01b4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01bc <[^>]*> sdl \$a0,[07]\(\$at\)
0+01c0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01cc <[^>]*> sdl \$a0,[07]\(\$at\)
0+01d0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01dc <[^>]*> sdl \$a0,[07]\(\$at\)
0+01e0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01e8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01ec <[^>]*> sdl \$a0,[07]\(\$at\)
0+01f0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01f8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01fc <[^>]*> sdl \$a0,[07]\(\$at\)
0+0200 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0208 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+020c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0210 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0214 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0214 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0218 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+021c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0220 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0224 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0224 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0228 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+022c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0230 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0238 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+023c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0240 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0244 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0244 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0248 <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+024c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0250 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0254 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0254 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0258 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+025c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0260 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0270 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0274 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0274 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0278 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+027c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0280 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0284 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0284 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0288 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+028c <[^>]*> sdl \$a0,[07]\(\$at\)
0+0290 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0294 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0298 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+029c <[^>]*> sdl \$a0,[07]\(\$at\)
0+02a0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02a4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02a8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02ac <[^>]*> sdl \$a0,[07]\(\$at\)
0+02b0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02b4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02bc <[^>]*> sdl \$a0,[07]\(\$at\)
0+02c0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02c4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02cc <[^>]*> sdl \$a0,[07]\(\$at\)
0+02d0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02d4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> sdl \$a0,[07]\(\$at\)
0+02e0 <[^>]*> sdr \$a0,[07]\(\$at\)
...
