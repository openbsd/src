#objdump: -dr
#name: MIPS la-svr4pic
#as: -mips1 -KPIC
#source: la.s

# Test the la macro with -KPIC.

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
0+0050 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0050 R_MIPS_GOT16 .data
...
0+0058 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0058 R_MIPS_LO16 .data
0+005c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+005c R_MIPS_GOT16 big_external_data_label
0+0060 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0060 R_MIPS_GOT16 small_external_data_label
0+0064 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0064 R_MIPS_GOT16 big_external_common
0+0068 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0068 R_MIPS_GOT16 small_external_common
0+006c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+006c R_MIPS_GOT16 .bss
...
0+0074 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0074 R_MIPS_LO16 .bss
0+0078 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0078 R_MIPS_GOT16 .bss
...
0+0080 <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+0080 R_MIPS_LO16 .bss
0+0084 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0084 R_MIPS_GOT16 .data
...
0+008c <[^>]*> addiu \$a0,\$a0,1
[ 	]*RELOC: 0+008c R_MIPS_LO16 .data
0+0090 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0090 R_MIPS_GOT16 big_external_data_label
...
0+0098 <[^>]*> addiu \$a0,\$a0,1
0+009c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+009c R_MIPS_GOT16 small_external_data_label
...
0+00a4 <[^>]*> addiu \$a0,\$a0,1
0+00a8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00a8 R_MIPS_GOT16 big_external_common
...
0+00b0 <[^>]*> addiu \$a0,\$a0,1
0+00b4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00b4 R_MIPS_GOT16 small_external_common
...
0+00bc <[^>]*> addiu \$a0,\$a0,1
0+00c0 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00c0 R_MIPS_GOT16 .bss
...
0+00c8 <[^>]*> addiu \$a0,\$a0,1
[ 	]*RELOC: 0+00c8 R_MIPS_LO16 .bss
0+00cc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00cc R_MIPS_GOT16 .bss
...
0+00d4 <[^>]*> addiu \$a0,\$a0,1001
[ 	]*RELOC: 0+00d4 R_MIPS_LO16 .bss
0+00d8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00d8 R_MIPS_GOT16 .data
0+00dc <[^>]*> lui \$at,1
0+00e0 <[^>]*> addiu \$at,\$at,-32768
[ 	]*RELOC: 0+00e0 R_MIPS_LO16 .data
0+00e4 <[^>]*> addu \$a0,\$a0,\$at
0+00e8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00e8 R_MIPS_GOT16 big_external_data_label
0+00ec <[^>]*> lui \$at,1
0+00f0 <[^>]*> addiu \$at,\$at,-32768
0+00f4 <[^>]*> addu \$a0,\$a0,\$at
0+00f8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00f8 R_MIPS_GOT16 small_external_data_label
0+00fc <[^>]*> lui \$at,1
0+0100 <[^>]*> addiu \$at,\$at,-32768
0+0104 <[^>]*> addu \$a0,\$a0,\$at
0+0108 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0108 R_MIPS_GOT16 big_external_common
0+010c <[^>]*> lui \$at,1
0+0110 <[^>]*> addiu \$at,\$at,-32768
0+0114 <[^>]*> addu \$a0,\$a0,\$at
0+0118 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0118 R_MIPS_GOT16 small_external_common
0+011c <[^>]*> lui \$at,1
0+0120 <[^>]*> addiu \$at,\$at,-32768
0+0124 <[^>]*> addu \$a0,\$a0,\$at
0+0128 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0128 R_MIPS_GOT16 .bss
0+012c <[^>]*> lui \$at,1
0+0130 <[^>]*> addiu \$at,\$at,-32768
[ 	]*RELOC: 0+0130 R_MIPS_LO16 .bss
0+0134 <[^>]*> addu \$a0,\$a0,\$at
0+0138 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0138 R_MIPS_GOT16 .bss
0+013c <[^>]*> lui \$at,1
0+0140 <[^>]*> addiu \$at,\$at,-31768
[ 	]*RELOC: 0+0140 R_MIPS_LO16 .bss
0+0144 <[^>]*> addu \$a0,\$a0,\$at
0+0148 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0148 R_MIPS_GOT16 .data
...
0+0150 <[^>]*> addiu \$a0,\$a0,-32768
[ 	]*RELOC: 0+0150 R_MIPS_LO16 .data
0+0154 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0154 R_MIPS_GOT16 big_external_data_label
...
0+015c <[^>]*> addiu \$a0,\$a0,-32768
0+0160 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0160 R_MIPS_GOT16 small_external_data_label
...
0+0168 <[^>]*> addiu \$a0,\$a0,-32768
0+016c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+016c R_MIPS_GOT16 big_external_common
...
0+0174 <[^>]*> addiu \$a0,\$a0,-32768
0+0178 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0178 R_MIPS_GOT16 small_external_common
...
0+0180 <[^>]*> addiu \$a0,\$a0,-32768
0+0184 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0184 R_MIPS_GOT16 .bss
...
0+018c <[^>]*> addiu \$a0,\$a0,-32768
[ 	]*RELOC: 0+018c R_MIPS_LO16 .bss
0+0190 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0190 R_MIPS_GOT16 .bss
...
0+0198 <[^>]*> addiu \$a0,\$a0,-31768
[ 	]*RELOC: 0+0198 R_MIPS_LO16 .bss
0+019c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+019c R_MIPS_GOT16 .data
0+01a0 <[^>]*> lui \$at,1
0+01a4 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+01a4 R_MIPS_LO16 .data
0+01a8 <[^>]*> addu \$a0,\$a0,\$at
0+01ac <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01ac R_MIPS_GOT16 big_external_data_label
0+01b0 <[^>]*> lui \$at,1
0+01b4 <[^>]*> addiu \$at,\$at,0
0+01b8 <[^>]*> addu \$a0,\$a0,\$at
0+01bc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01bc R_MIPS_GOT16 small_external_data_label
0+01c0 <[^>]*> lui \$at,1
0+01c4 <[^>]*> addiu \$at,\$at,0
0+01c8 <[^>]*> addu \$a0,\$a0,\$at
0+01cc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01cc R_MIPS_GOT16 big_external_common
0+01d0 <[^>]*> lui \$at,1
0+01d4 <[^>]*> addiu \$at,\$at,0
0+01d8 <[^>]*> addu \$a0,\$a0,\$at
0+01dc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01dc R_MIPS_GOT16 small_external_common
0+01e0 <[^>]*> lui \$at,1
0+01e4 <[^>]*> addiu \$at,\$at,0
0+01e8 <[^>]*> addu \$a0,\$a0,\$at
0+01ec <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01ec R_MIPS_GOT16 .bss
0+01f0 <[^>]*> lui \$at,1
0+01f4 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+01f4 R_MIPS_LO16 .bss
0+01f8 <[^>]*> addu \$a0,\$a0,\$at
0+01fc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+01fc R_MIPS_GOT16 .bss
0+0200 <[^>]*> lui \$at,1
0+0204 <[^>]*> addiu \$at,\$at,1000
[ 	]*RELOC: 0+0204 R_MIPS_LO16 .bss
0+0208 <[^>]*> addu \$a0,\$a0,\$at
0+020c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+020c R_MIPS_GOT16 .data
0+0210 <[^>]*> lui \$at,2
0+0214 <[^>]*> addiu \$at,\$at,-23131
[ 	]*RELOC: 0+0214 R_MIPS_LO16 .data
0+0218 <[^>]*> addu \$a0,\$a0,\$at
0+021c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+021c R_MIPS_GOT16 big_external_data_label
0+0220 <[^>]*> lui \$at,2
0+0224 <[^>]*> addiu \$at,\$at,-23131
0+0228 <[^>]*> addu \$a0,\$a0,\$at
0+022c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+022c R_MIPS_GOT16 small_external_data_label
0+0230 <[^>]*> lui \$at,2
0+0234 <[^>]*> addiu \$at,\$at,-23131
0+0238 <[^>]*> addu \$a0,\$a0,\$at
0+023c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+023c R_MIPS_GOT16 big_external_common
0+0240 <[^>]*> lui \$at,2
0+0244 <[^>]*> addiu \$at,\$at,-23131
0+0248 <[^>]*> addu \$a0,\$a0,\$at
0+024c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+024c R_MIPS_GOT16 small_external_common
0+0250 <[^>]*> lui \$at,2
0+0254 <[^>]*> addiu \$at,\$at,-23131
0+0258 <[^>]*> addu \$a0,\$a0,\$at
0+025c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+025c R_MIPS_GOT16 .bss
0+0260 <[^>]*> lui \$at,2
0+0264 <[^>]*> addiu \$at,\$at,-23131
[ 	]*RELOC: 0+0264 R_MIPS_LO16 .bss
0+0268 <[^>]*> addu \$a0,\$a0,\$at
0+026c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+026c R_MIPS_GOT16 .bss
0+0270 <[^>]*> lui \$at,2
0+0274 <[^>]*> addiu \$at,\$at,-22131
[ 	]*RELOC: 0+0274 R_MIPS_LO16 .bss
0+0278 <[^>]*> addu \$a0,\$a0,\$at
0+027c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+027c R_MIPS_GOT16 .data
...
0+0284 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+0284 R_MIPS_LO16 .data
0+0288 <[^>]*> addu \$a0,\$a0,\$a1
0+028c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+028c R_MIPS_GOT16 big_external_data_label
...
0+0294 <[^>]*> addu \$a0,\$a0,\$a1
0+0298 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0298 R_MIPS_GOT16 small_external_data_label
...
0+02a0 <[^>]*> addu \$a0,\$a0,\$a1
0+02a4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02a4 R_MIPS_GOT16 big_external_common
...
0+02ac <[^>]*> addu \$a0,\$a0,\$a1
0+02b0 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02b0 R_MIPS_GOT16 small_external_common
...
0+02b8 <[^>]*> addu \$a0,\$a0,\$a1
0+02bc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02bc R_MIPS_GOT16 .bss
...
0+02c4 <[^>]*> addiu \$a0,\$a0,0
[ 	]*RELOC: 0+02c4 R_MIPS_LO16 .bss
0+02c8 <[^>]*> addu \$a0,\$a0,\$a1
0+02cc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02cc R_MIPS_GOT16 .bss
...
0+02d4 <[^>]*> addiu \$a0,\$a0,1000
[ 	]*RELOC: 0+02d4 R_MIPS_LO16 .bss
0+02d8 <[^>]*> addu \$a0,\$a0,\$a1
0+02dc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02dc R_MIPS_GOT16 .data
...
0+02e4 <[^>]*> addiu \$a0,\$a0,1
[ 	]*RELOC: 0+02e4 R_MIPS_LO16 .data
0+02e8 <[^>]*> addu \$a0,\$a0,\$a1
0+02ec <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02ec R_MIPS_GOT16 big_external_data_label
...
0+02f4 <[^>]*> addiu \$a0,\$a0,1
0+02f8 <[^>]*> addu \$a0,\$a0,\$a1
0+02fc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+02fc R_MIPS_GOT16 small_external_data_label
...
0+0304 <[^>]*> addiu \$a0,\$a0,1
0+0308 <[^>]*> addu \$a0,\$a0,\$a1
0+030c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+030c R_MIPS_GOT16 big_external_common
...
0+0314 <[^>]*> addiu \$a0,\$a0,1
0+0318 <[^>]*> addu \$a0,\$a0,\$a1
0+031c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+031c R_MIPS_GOT16 small_external_common
...
0+0324 <[^>]*> addiu \$a0,\$a0,1
0+0328 <[^>]*> addu \$a0,\$a0,\$a1
0+032c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+032c R_MIPS_GOT16 .bss
...
0+0334 <[^>]*> addiu \$a0,\$a0,1
[ 	]*RELOC: 0+0334 R_MIPS_LO16 .bss
0+0338 <[^>]*> addu \$a0,\$a0,\$a1
0+033c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+033c R_MIPS_GOT16 .bss
...
0+0344 <[^>]*> addiu \$a0,\$a0,1001
[ 	]*RELOC: 0+0344 R_MIPS_LO16 .bss
0+0348 <[^>]*> addu \$a0,\$a0,\$a1
0+034c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+034c R_MIPS_GOT16 .data
0+0350 <[^>]*> lui \$at,1
0+0354 <[^>]*> addiu \$at,\$at,-32768
[ 	]*RELOC: 0+0354 R_MIPS_LO16 .data
0+0358 <[^>]*> addu \$a0,\$a0,\$at
0+035c <[^>]*> addu \$a0,\$a0,\$a1
0+0360 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0360 R_MIPS_GOT16 big_external_data_label
0+0364 <[^>]*> lui \$at,1
0+0368 <[^>]*> addiu \$at,\$at,-32768
0+036c <[^>]*> addu \$a0,\$a0,\$at
0+0370 <[^>]*> addu \$a0,\$a0,\$a1
0+0374 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0374 R_MIPS_GOT16 small_external_data_label
0+0378 <[^>]*> lui \$at,1
0+037c <[^>]*> addiu \$at,\$at,-32768
0+0380 <[^>]*> addu \$a0,\$a0,\$at
0+0384 <[^>]*> addu \$a0,\$a0,\$a1
0+0388 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0388 R_MIPS_GOT16 big_external_common
0+038c <[^>]*> lui \$at,1
0+0390 <[^>]*> addiu \$at,\$at,-32768
0+0394 <[^>]*> addu \$a0,\$a0,\$at
0+0398 <[^>]*> addu \$a0,\$a0,\$a1
0+039c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+039c R_MIPS_GOT16 small_external_common
0+03a0 <[^>]*> lui \$at,1
0+03a4 <[^>]*> addiu \$at,\$at,-32768
0+03a8 <[^>]*> addu \$a0,\$a0,\$at
0+03ac <[^>]*> addu \$a0,\$a0,\$a1
0+03b0 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+03b0 R_MIPS_GOT16 .bss
0+03b4 <[^>]*> lui \$at,1
0+03b8 <[^>]*> addiu \$at,\$at,-32768
[ 	]*RELOC: 0+03b8 R_MIPS_LO16 .bss
0+03bc <[^>]*> addu \$a0,\$a0,\$at
0+03c0 <[^>]*> addu \$a0,\$a0,\$a1
0+03c4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+03c4 R_MIPS_GOT16 .bss
0+03c8 <[^>]*> lui \$at,1
0+03cc <[^>]*> addiu \$at,\$at,-31768
[ 	]*RELOC: 0+03cc R_MIPS_LO16 .bss
0+03d0 <[^>]*> addu \$a0,\$a0,\$at
0+03d4 <[^>]*> addu \$a0,\$a0,\$a1
0+03d8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+03d8 R_MIPS_GOT16 .data
...
0+03e0 <[^>]*> addiu \$a0,\$a0,-32768
[ 	]*RELOC: 0+03e0 R_MIPS_LO16 .data
0+03e4 <[^>]*> addu \$a0,\$a0,\$a1
0+03e8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+03e8 R_MIPS_GOT16 big_external_data_label
...
0+03f0 <[^>]*> addiu \$a0,\$a0,-32768
0+03f4 <[^>]*> addu \$a0,\$a0,\$a1
0+03f8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+03f8 R_MIPS_GOT16 small_external_data_label
...
0+0400 <[^>]*> addiu \$a0,\$a0,-32768
0+0404 <[^>]*> addu \$a0,\$a0,\$a1
0+0408 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0408 R_MIPS_GOT16 big_external_common
...
0+0410 <[^>]*> addiu \$a0,\$a0,-32768
0+0414 <[^>]*> addu \$a0,\$a0,\$a1
0+0418 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0418 R_MIPS_GOT16 small_external_common
...
0+0420 <[^>]*> addiu \$a0,\$a0,-32768
0+0424 <[^>]*> addu \$a0,\$a0,\$a1
0+0428 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0428 R_MIPS_GOT16 .bss
...
0+0430 <[^>]*> addiu \$a0,\$a0,-32768
[ 	]*RELOC: 0+0430 R_MIPS_LO16 .bss
0+0434 <[^>]*> addu \$a0,\$a0,\$a1
0+0438 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0438 R_MIPS_GOT16 .bss
...
0+0440 <[^>]*> addiu \$a0,\$a0,-31768
[ 	]*RELOC: 0+0440 R_MIPS_LO16 .bss
0+0444 <[^>]*> addu \$a0,\$a0,\$a1
0+0448 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0448 R_MIPS_GOT16 .data
0+044c <[^>]*> lui \$at,1
0+0450 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+0450 R_MIPS_LO16 .data
0+0454 <[^>]*> addu \$a0,\$a0,\$at
0+0458 <[^>]*> addu \$a0,\$a0,\$a1
0+045c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+045c R_MIPS_GOT16 big_external_data_label
0+0460 <[^>]*> lui \$at,1
0+0464 <[^>]*> addiu \$at,\$at,0
0+0468 <[^>]*> addu \$a0,\$a0,\$at
0+046c <[^>]*> addu \$a0,\$a0,\$a1
0+0470 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0470 R_MIPS_GOT16 small_external_data_label
0+0474 <[^>]*> lui \$at,1
0+0478 <[^>]*> addiu \$at,\$at,0
0+047c <[^>]*> addu \$a0,\$a0,\$at
0+0480 <[^>]*> addu \$a0,\$a0,\$a1
0+0484 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0484 R_MIPS_GOT16 big_external_common
0+0488 <[^>]*> lui \$at,1
0+048c <[^>]*> addiu \$at,\$at,0
0+0490 <[^>]*> addu \$a0,\$a0,\$at
0+0494 <[^>]*> addu \$a0,\$a0,\$a1
0+0498 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0498 R_MIPS_GOT16 small_external_common
0+049c <[^>]*> lui \$at,1
0+04a0 <[^>]*> addiu \$at,\$at,0
0+04a4 <[^>]*> addu \$a0,\$a0,\$at
0+04a8 <[^>]*> addu \$a0,\$a0,\$a1
0+04ac <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+04ac R_MIPS_GOT16 .bss
0+04b0 <[^>]*> lui \$at,1
0+04b4 <[^>]*> addiu \$at,\$at,0
[ 	]*RELOC: 0+04b4 R_MIPS_LO16 .bss
0+04b8 <[^>]*> addu \$a0,\$a0,\$at
0+04bc <[^>]*> addu \$a0,\$a0,\$a1
0+04c0 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+04c0 R_MIPS_GOT16 .bss
0+04c4 <[^>]*> lui \$at,1
0+04c8 <[^>]*> addiu \$at,\$at,1000
[ 	]*RELOC: 0+04c8 R_MIPS_LO16 .bss
0+04cc <[^>]*> addu \$a0,\$a0,\$at
0+04d0 <[^>]*> addu \$a0,\$a0,\$a1
0+04d4 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+04d4 R_MIPS_GOT16 .data
0+04d8 <[^>]*> lui \$at,2
0+04dc <[^>]*> addiu \$at,\$at,-23131
[ 	]*RELOC: 0+04dc R_MIPS_LO16 .data
0+04e0 <[^>]*> addu \$a0,\$a0,\$at
0+04e4 <[^>]*> addu \$a0,\$a0,\$a1
0+04e8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+04e8 R_MIPS_GOT16 big_external_data_label
0+04ec <[^>]*> lui \$at,2
0+04f0 <[^>]*> addiu \$at,\$at,-23131
0+04f4 <[^>]*> addu \$a0,\$a0,\$at
0+04f8 <[^>]*> addu \$a0,\$a0,\$a1
0+04fc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+04fc R_MIPS_GOT16 small_external_data_label
0+0500 <[^>]*> lui \$at,2
0+0504 <[^>]*> addiu \$at,\$at,-23131
0+0508 <[^>]*> addu \$a0,\$a0,\$at
0+050c <[^>]*> addu \$a0,\$a0,\$a1
0+0510 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0510 R_MIPS_GOT16 big_external_common
0+0514 <[^>]*> lui \$at,2
0+0518 <[^>]*> addiu \$at,\$at,-23131
0+051c <[^>]*> addu \$a0,\$a0,\$at
0+0520 <[^>]*> addu \$a0,\$a0,\$a1
0+0524 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0524 R_MIPS_GOT16 small_external_common
0+0528 <[^>]*> lui \$at,2
0+052c <[^>]*> addiu \$at,\$at,-23131
0+0530 <[^>]*> addu \$a0,\$a0,\$at
0+0534 <[^>]*> addu \$a0,\$a0,\$a1
0+0538 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0538 R_MIPS_GOT16 .bss
0+053c <[^>]*> lui \$at,2
0+0540 <[^>]*> addiu \$at,\$at,-23131
[ 	]*RELOC: 0+0540 R_MIPS_LO16 .bss
0+0544 <[^>]*> addu \$a0,\$a0,\$at
0+0548 <[^>]*> addu \$a0,\$a0,\$a1
0+054c <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+054c R_MIPS_GOT16 .bss
0+0550 <[^>]*> lui \$at,2
0+0554 <[^>]*> addiu \$at,\$at,-22131
[ 	]*RELOC: 0+0554 R_MIPS_LO16 .bss
0+0558 <[^>]*> addu \$a0,\$a0,\$at
0+055c <[^>]*> addu \$a0,\$a0,\$a1
