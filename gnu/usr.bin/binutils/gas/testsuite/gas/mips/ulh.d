#objdump: -dr
#name: MIPS ulh
#as: -mips1

# Test the ulh macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lb \$a0,[01]\(\$zero\)
0+0004 <[^>]*> lbu \$at,[01]\(\$zero\)
0+0008 <[^>]*> sll \$a0,\$a0,0x8
0+000c <[^>]*> or \$a0,\$a0,\$at
0+0010 <[^>]*> lb \$a0,[12]\(\$zero\)
0+0014 <[^>]*> lbu \$at,[12]\(\$zero\)
0+0018 <[^>]*> sll \$a0,\$a0,0x8
0+001c <[^>]*> or \$a0,\$a0,\$at
0+0020 <[^>]*> li \$at,32768
0+0024 <[^>]*> lb \$a0,[01]\(\$at\)
0+0028 <[^>]*> lbu \$at,[01]\(\$at\)
0+002c <[^>]*> sll \$a0,\$a0,0x8
0+0030 <[^>]*> or \$a0,\$a0,\$at
0+0034 <[^>]*> lb \$a0,-3276[78]\(\$zero\)
0+0038 <[^>]*> lbu \$at,-3276[78]\(\$zero\)
0+003c <[^>]*> sll \$a0,\$a0,0x8
0+0040 <[^>]*> or \$a0,\$a0,\$at
0+0044 <[^>]*> lui \$at,1
0+0048 <[^>]*> lb \$a0,[01]\(\$at\)
0+004c <[^>]*> lbu \$at,[01]\(\$at\)
0+0050 <[^>]*> sll \$a0,\$a0,0x8
0+0054 <[^>]*> or \$a0,\$a0,\$at
0+0058 <[^>]*> lui \$at,1
0+005c <[^>]*> ori \$at,\$at,42405
0+0060 <[^>]*> lb \$a0,[01]\(\$at\)
0+0064 <[^>]*> lbu \$at,[01]\(\$at\)
0+0068 <[^>]*> sll \$a0,\$a0,0x8
0+006c <[^>]*> or \$a0,\$a0,\$at
0+0070 <[^>]*> lb \$a0,[01]\(\$a1\)
0+0074 <[^>]*> lbu \$at,[01]\(\$a1\)
0+0078 <[^>]*> sll \$a0,\$a0,0x8
0+007c <[^>]*> or \$a0,\$a0,\$at
0+0080 <[^>]*> lb \$a0,[12]\(\$a1\)
0+0084 <[^>]*> lbu \$at,[12]\(\$a1\)
0+0088 <[^>]*> sll \$a0,\$a0,0x8
0+008c <[^>]*> or \$a0,\$a0,\$at
0+0090 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0090 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0094 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0094 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0098 <[^>]*> lb \$a0,[01]\(\$at\)
0+009c <[^>]*> lbu \$at,[01]\(\$at\)
0+00a0 <[^>]*> sll \$a0,\$a0,0x8
0+00a4 <[^>]*> or \$a0,\$a0,\$at
0+00a8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00ac <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00b0 <[^>]*> lb \$a0,[01]\(\$at\)
0+00b4 <[^>]*> lbu \$at,[01]\(\$at\)
0+00b8 <[^>]*> sll \$a0,\$a0,0x8
0+00bc <[^>]*> or \$a0,\$a0,\$at
0+00c0 <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00c4 <[^>]*> lb \$a0,[01]\(\$at\)
0+00c8 <[^>]*> lbu \$at,[01]\(\$at\)
0+00cc <[^>]*> sll \$a0,\$a0,0x8
0+00d0 <[^>]*> or \$a0,\$a0,\$at
0+00d4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00dc <[^>]*> lb \$a0,[01]\(\$at\)
0+00e0 <[^>]*> lbu \$at,[01]\(\$at\)
0+00e4 <[^>]*> sll \$a0,\$a0,0x8
0+00e8 <[^>]*> or \$a0,\$a0,\$at
0+00ec <[^>]*> addiu \$at,\$gp,0
[ 	]*RELOC: 0+00ec [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00f0 <[^>]*> lb \$a0,[01]\(\$at\)
0+00f4 <[^>]*> lbu \$at,[01]\(\$at\)
0+00f8 <[^>]*> sll \$a0,\$a0,0x8
0+00fc <[^>]*> or \$a0,\$a0,\$at
0+0100 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0100 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0104 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0104 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0108 <[^>]*> lb \$a0,[01]\(\$at\)
0+010c <[^>]*> lbu \$at,[01]\(\$at\)
0+0110 <[^>]*> sll \$a0,\$a0,0x8
0+0114 <[^>]*> or \$a0,\$a0,\$at
0+0118 <[^>]*> addiu \$at,\$gp,[-0-9]+
[ 	]*RELOC: 0+0118 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+011c <[^>]*> lb \$a0,[01]\(\$at\)
0+0120 <[^>]*> lbu \$at,[01]\(\$at\)
0+0124 <[^>]*> sll \$a0,\$a0,0x8
0+0128 <[^>]*> or \$a0,\$a0,\$at
0+012c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+012c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0130 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0130 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0134 <[^>]*> lb \$a0,[01]\(\$at\)
0+0138 <[^>]*> lbu \$at,[01]\(\$at\)
0+013c <[^>]*> sll \$a0,\$a0,0x8
0+0140 <[^>]*> or \$a0,\$a0,\$at
0+0144 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0148 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+014c <[^>]*> lb \$a0,[01]\(\$at\)
0+0150 <[^>]*> lbu \$at,[01]\(\$at\)
0+0154 <[^>]*> sll \$a0,\$a0,0x8
0+0158 <[^>]*> or \$a0,\$a0,\$at
0+015c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+015c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0160 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0160 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0164 <[^>]*> lb \$a0,[01]\(\$at\)
0+0168 <[^>]*> lbu \$at,[01]\(\$at\)
0+016c <[^>]*> sll \$a0,\$a0,0x8
0+0170 <[^>]*> or \$a0,\$a0,\$at
0+0174 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0178 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+017c <[^>]*> lb \$a0,[01]\(\$at\)
0+0180 <[^>]*> lbu \$at,[01]\(\$at\)
0+0184 <[^>]*> sll \$a0,\$a0,0x8
0+0188 <[^>]*> or \$a0,\$a0,\$at
0+018c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+018c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0190 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0190 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0194 <[^>]*> lb \$a0,[01]\(\$at\)
0+0198 <[^>]*> lbu \$at,[01]\(\$at\)
0+019c <[^>]*> sll \$a0,\$a0,0x8
0+01a0 <[^>]*> or \$a0,\$a0,\$at
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01a8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01ac <[^>]*> lb \$a0,[01]\(\$at\)
0+01b0 <[^>]*> lbu \$at,[01]\(\$at\)
0+01b4 <[^>]*> sll \$a0,\$a0,0x8
0+01b8 <[^>]*> or \$a0,\$a0,\$at
0+01bc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01bc [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01c0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01c4 <[^>]*> lb \$a0,[01]\(\$at\)
0+01c8 <[^>]*> lbu \$at,[01]\(\$at\)
0+01cc <[^>]*> sll \$a0,\$a0,0x8
0+01d0 <[^>]*> or \$a0,\$a0,\$at
0+01d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01d8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01dc <[^>]*> lb \$a0,[01]\(\$at\)
0+01e0 <[^>]*> lbu \$at,[01]\(\$at\)
0+01e4 <[^>]*> sll \$a0,\$a0,0x8
0+01e8 <[^>]*> or \$a0,\$a0,\$at
0+01ec <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01f0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+01f0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01f4 <[^>]*> lb \$a0,[01]\(\$at\)
0+01f8 <[^>]*> lbu \$at,[01]\(\$at\)
0+01fc <[^>]*> sll \$a0,\$a0,0x8
0+0200 <[^>]*> or \$a0,\$a0,\$at
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0208 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+020c <[^>]*> lb \$a0,[01]\(\$at\)
0+0210 <[^>]*> lbu \$at,[01]\(\$at\)
0+0214 <[^>]*> sll \$a0,\$a0,0x8
0+0218 <[^>]*> or \$a0,\$a0,\$at
0+021c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+021c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0220 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0220 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0224 <[^>]*> lb \$a0,[01]\(\$at\)
0+0228 <[^>]*> lbu \$at,[01]\(\$at\)
0+022c <[^>]*> sll \$a0,\$a0,0x8
0+0230 <[^>]*> or \$a0,\$a0,\$at
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0238 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+023c <[^>]*> lb \$a0,[01]\(\$at\)
0+0240 <[^>]*> lbu \$at,[01]\(\$at\)
0+0244 <[^>]*> sll \$a0,\$a0,0x8
0+0248 <[^>]*> or \$a0,\$a0,\$at
0+024c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0250 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0250 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0254 <[^>]*> lb \$a0,[01]\(\$at\)
0+0258 <[^>]*> lbu \$at,[01]\(\$at\)
0+025c <[^>]*> sll \$a0,\$a0,0x8
0+0260 <[^>]*> or \$a0,\$a0,\$at
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> lb \$a0,[01]\(\$at\)
0+0270 <[^>]*> lbu \$at,[01]\(\$at\)
0+0274 <[^>]*> sll \$a0,\$a0,0x8
0+0278 <[^>]*> or \$a0,\$a0,\$at
0+027c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+027c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0280 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0280 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0284 <[^>]*> lb \$a0,[01]\(\$at\)
0+0288 <[^>]*> lbu \$at,[01]\(\$at\)
0+028c <[^>]*> sll \$a0,\$a0,0x8
0+0290 <[^>]*> or \$a0,\$a0,\$at
0+0294 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0298 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+029c <[^>]*> lb \$a0,[01]\(\$at\)
0+02a0 <[^>]*> lbu \$at,[01]\(\$at\)
0+02a4 <[^>]*> sll \$a0,\$a0,0x8
0+02a8 <[^>]*> or \$a0,\$a0,\$at
0+02ac <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02ac [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02b0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02b0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+02b4 <[^>]*> lb \$a0,[01]\(\$at\)
0+02b8 <[^>]*> lbu \$at,[01]\(\$at\)
0+02bc <[^>]*> sll \$a0,\$a0,0x8
0+02c0 <[^>]*> or \$a0,\$a0,\$at
0+02c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02c8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02cc <[^>]*> lb \$a0,[01]\(\$at\)
0+02d0 <[^>]*> lbu \$at,[01]\(\$at\)
0+02d4 <[^>]*> sll \$a0,\$a0,0x8
0+02d8 <[^>]*> or \$a0,\$a0,\$at
0+02dc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02dc [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02e0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02e0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02e4 <[^>]*> lb \$a0,[01]\(\$at\)
0+02e8 <[^>]*> lbu \$at,[01]\(\$at\)
0+02ec <[^>]*> sll \$a0,\$a0,0x8
0+02f0 <[^>]*> or \$a0,\$a0,\$at
0+02f4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02f4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02f8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+02f8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02fc <[^>]*> lb \$a0,[01]\(\$at\)
0+0300 <[^>]*> lbu \$at,[01]\(\$at\)
0+0304 <[^>]*> sll \$a0,\$a0,0x8
0+0308 <[^>]*> or \$a0,\$a0,\$at
0+030c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+030c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0310 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0310 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0314 <[^>]*> lb \$a0,[01]\(\$at\)
0+0318 <[^>]*> lbu \$at,[01]\(\$at\)
0+031c <[^>]*> sll \$a0,\$a0,0x8
0+0320 <[^>]*> or \$a0,\$a0,\$at
0+0324 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0324 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0328 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0328 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+032c <[^>]*> lb \$a0,[01]\(\$at\)
0+0330 <[^>]*> lbu \$at,[01]\(\$at\)
0+0334 <[^>]*> sll \$a0,\$a0,0x8
0+0338 <[^>]*> or \$a0,\$a0,\$at
0+033c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+033c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0340 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0340 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0344 <[^>]*> lb \$a0,[01]\(\$at\)
0+0348 <[^>]*> lbu \$at,[01]\(\$at\)
0+034c <[^>]*> sll \$a0,\$a0,0x8
0+0350 <[^>]*> or \$a0,\$a0,\$at
0+0354 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0354 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0358 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0358 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+035c <[^>]*> lb \$a0,[01]\(\$at\)
0+0360 <[^>]*> lbu \$at,[01]\(\$at\)
0+0364 <[^>]*> sll \$a0,\$a0,0x8
0+0368 <[^>]*> or \$a0,\$a0,\$at
0+036c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+036c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0370 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0370 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0374 <[^>]*> lb \$a0,[01]\(\$at\)
0+0378 <[^>]*> lbu \$at,[01]\(\$at\)
0+037c <[^>]*> sll \$a0,\$a0,0x8
0+0380 <[^>]*> or \$a0,\$a0,\$at
0+0384 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0384 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0388 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0388 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+038c <[^>]*> lb \$a0,[01]\(\$at\)
0+0390 <[^>]*> lbu \$at,[01]\(\$at\)
0+0394 <[^>]*> sll \$a0,\$a0,0x8
0+0398 <[^>]*> or \$a0,\$a0,\$at
0+039c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+039c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+03a0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+03a0 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+03a4 <[^>]*> lb \$a0,[01]\(\$at\)
0+03a8 <[^>]*> lbu \$at,[01]\(\$at\)
0+03ac <[^>]*> sll \$a0,\$a0,0x8
0+03b0 <[^>]*> or \$a0,\$a0,\$at
0+03b4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03b4 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+03b8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+03b8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+03bc <[^>]*> lb \$a0,[01]\(\$at\)
0+03c0 <[^>]*> lbu \$at,[01]\(\$at\)
0+03c4 <[^>]*> sll \$a0,\$a0,0x8
0+03c8 <[^>]*> or \$a0,\$a0,\$at
0+03cc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03cc [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+03d0 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+03d0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+03d4 <[^>]*> lb \$a0,[01]\(\$at\)
0+03d8 <[^>]*> lbu \$at,[01]\(\$at\)
0+03dc <[^>]*> sll \$a0,\$a0,0x8
0+03e0 <[^>]*> or \$a0,\$a0,\$at
0+03e4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+03e8 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+03e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+03ec <[^>]*> lb \$a0,[01]\(\$at\)
0+03f0 <[^>]*> lbu \$at,[01]\(\$at\)
0+03f4 <[^>]*> sll \$a0,\$a0,0x8
0+03f8 <[^>]*> or \$a0,\$a0,\$at
0+03fc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03fc [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0400 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0400 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0404 <[^>]*> lb \$a0,[01]\(\$at\)
0+0408 <[^>]*> lbu \$at,[01]\(\$at\)
0+040c <[^>]*> sll \$a0,\$a0,0x8
0+0410 <[^>]*> or \$a0,\$a0,\$at
0+0414 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0414 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0418 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0418 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+041c <[^>]*> lb \$a0,[01]\(\$at\)
0+0420 <[^>]*> lbu \$at,[01]\(\$at\)
0+0424 <[^>]*> sll \$a0,\$a0,0x8
0+0428 <[^>]*> or \$a0,\$a0,\$at
0+042c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+042c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0430 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0430 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0434 <[^>]*> lb \$a0,[01]\(\$at\)
0+0438 <[^>]*> lbu \$at,[01]\(\$at\)
0+043c <[^>]*> sll \$a0,\$a0,0x8
0+0440 <[^>]*> or \$a0,\$a0,\$at
0+0444 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0444 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0448 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0448 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+044c <[^>]*> lb \$a0,[01]\(\$at\)
0+0450 <[^>]*> lbu \$at,[01]\(\$at\)
0+0454 <[^>]*> sll \$a0,\$a0,0x8
0+0458 <[^>]*> or \$a0,\$a0,\$at
0+045c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+045c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0460 <[^>]*> addiu \$at,\$at,[-0-9]+
[ 	]*RELOC: 0+0460 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0464 <[^>]*> lb \$a0,[01]\(\$at\)
0+0468 <[^>]*> lbu \$at,[01]\(\$at\)
0+046c <[^>]*> sll \$a0,\$a0,0x8
0+0470 <[^>]*> or \$a0,\$a0,\$at
0+0474 <[^>]*> lbu \$a0,[01]\(\$zero\)
0+0478 <[^>]*> lbu \$at,[01]\(\$zero\)
0+047c <[^>]*> sll \$a0,\$a0,0x8
0+0480 <[^>]*> or \$a0,\$a0,\$at
...
