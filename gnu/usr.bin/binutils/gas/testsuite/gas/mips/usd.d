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
0+0010 <[^>]*> li \$at,0x8000
0+0014 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0018 <[^>]*> sdr \$a0,[07]\(\$at\)
0+001c <[^>]*> sdl \$a0,-3276[18]\(\$zero\)
0+0020 <[^>]*> sdr \$a0,-3276[18]\(\$zero\)
0+0024 <[^>]*> lui \$at,0x1
0+0028 <[^>]*> sdl \$a0,[07]\(\$at\)
0+002c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0030 <[^>]*> lui \$at,0x1
0+0034 <[^>]*> ori \$at,\$at,0xa5a5
0+0038 <[^>]*> sdl \$a0,[07]\(\$at\)
0+003c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0040 <[^>]*> sdl \$a0,[07]\(\$a1\)
0+0044 <[^>]*> sdr \$a0,[07]\(\$a1\)
0+0048 <[^>]*> sdl \$a0,[18]\(\$a1\)
0+004c <[^>]*> sdr \$a0,[-0-9]+\(\$a1\)
0+0050 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0050 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0054 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0054 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0058 <[^>]*> sdl \$a0,[07]\(\$at\)
0+005c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0060 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0060 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0064 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0068 <[^>]*> sdl \$a0,[07]\(\$at\)
0+006c <[^>]*> sdr \$a0,[07]\(\$at\)
0+0070 <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0074 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0078 <[^>]*> sdr \$a0,[07]\(\$at\)
0+007c <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+007c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0080 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0080 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0084 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0088 <[^>]*> sdr \$a0,[07]\(\$at\)
0+008c <[^>]*> daddiu \$at,\$gp,0
[ 	]*RELOC: 0+008c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0090 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0094 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0098 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+009c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00a0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+00a4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00a8 <[^>]*> daddiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ac <[^>]*> sdl \$a0,[07]\(\$at\)
0+00b0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00b4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00b8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00bc <[^>]*> sdl \$a0,[07]\(\$at\)
0+00c0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00c4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> sdl \$a0,[07]\(\$at\)
0+00d0 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00d4 <[^>]*> daddiu \$at,\$gp,1
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00d8 <[^>]*> sdl \$a0,[07]\(\$at\)
0+00dc <[^>]*> sdr \$a0,[07]\(\$at\)
0+00e0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00e4 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00e8 <[^>]*> sdl \$a0,[07]\(\$at\)
0+00ec <[^>]*> sdr \$a0,[07]\(\$at\)
0+00f0 <[^>]*> daddiu \$at,\$gp,1
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00f4 <[^>]*> sdl \$a0,[07]\(\$at\)
0+00f8 <[^>]*> sdr \$a0,[07]\(\$at\)
0+00fc <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00fc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0100 <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0100 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0104 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0108 <[^>]*> sdr \$a0,[07]\(\$at\)
0+010c <[^>]*> daddiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+010c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0110 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0114 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0118 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+011c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+011c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0120 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0124 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0128 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+012c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+012c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0130 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0134 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0138 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0140 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0144 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0148 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+014c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0150 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0154 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0158 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+015c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+015c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0160 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0164 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0168 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+016c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0170 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0174 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0178 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+017c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0180 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0184 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0188 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0188 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+018c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+018c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0190 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0194 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0198 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+019c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+019c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01a0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01a4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01a8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01ac <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01b0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01b4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01b8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01bc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01bc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01c0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01c4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01c8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01cc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01cc [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01d0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01d4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01d8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01dc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01dc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01e0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01e4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01e8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01ec <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01f0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+01f4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+01f8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01fc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01fc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0200 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0204 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0208 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+020c <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0210 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0214 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0218 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0218 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+021c <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+021c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0220 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0224 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0228 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+022c <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+022c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0230 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0234 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0238 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0238 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+023c <[^>]*> daddiu \$at,\$at,0
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0240 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0244 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0248 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0248 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+024c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0250 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0254 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0258 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+025c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+025c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0260 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0264 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0268 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+026c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0270 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0274 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0278 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0278 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+027c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+027c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0280 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0284 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0288 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0288 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+028c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+028c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0290 <[^>]*> sdl \$a0,[07]\(\$at\)
0+0294 <[^>]*> sdr \$a0,[07]\(\$at\)
0+0298 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+029c <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+029c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02a0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+02a4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02a8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02ac <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02ac [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02b0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+02b4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02b8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02bc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02bc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02c0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+02c4 <[^>]*> sdr \$a0,[07]\(\$at\)
0+02c8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02cc <[^>]*> daddiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02cc [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02d0 <[^>]*> sdl \$a0,[07]\(\$at\)
0+02d4 <[^>]*> sdr \$a0,[07]\(\$at\)
...
