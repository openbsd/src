#objdump: -dr
#name: MIPS la-empic
#as: -mips1 -membedded-pic

# Test the la macro with -membedded-pic.

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
0+0050 <[^>]*> addiu \$a0,\$gp,-16384
[ 	]*RELOC: 0+0050 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+0054 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0054 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+0058 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0058 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+005c <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+005c [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+0060 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0060 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0064 <[^>]*> addiu \$a0,\$gp,-16384
[ 	]*RELOC: 0+0064 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0068 <[^>]*> addiu \$a0,\$gp,-15384
[ 	]*RELOC: 0+0068 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+006c <[^>]*> addiu \$a0,\$gp,-16383
[ 	]*RELOC: 0+006c [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+0070 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+0074 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+0074 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0078 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+0078 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+007c <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+007c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0080 <[^>]*> addiu \$a0,\$gp,-16383
[ 	]*RELOC: 0+0080 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0084 <[^>]*> addiu \$a0,\$gp,-15383
[ 	]*RELOC: 0+0084 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0088 <[^>]*> addiu \$a0,\$gp,-16384
[ 	]*RELOC: 0+0088 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+008c <[^>]*> addu \$a0,\$a0,\$a1
0+0090 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0090 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+0094 <[^>]*> addu \$a0,\$a0,\$a1
0+0098 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+0098 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+009c <[^>]*> addu \$a0,\$a0,\$a1
0+00a0 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+00a4 <[^>]*> addu \$a0,\$a0,\$a1
0+00a8 <[^>]*> addiu \$a0,\$gp,0
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00ac <[^>]*> addu \$a0,\$a0,\$a1
0+00b0 <[^>]*> addiu \$a0,\$gp,-16384
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00b4 <[^>]*> addu \$a0,\$a0,\$a1
0+00b8 <[^>]*> addiu \$a0,\$gp,-15384
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00bc <[^>]*> addu \$a0,\$a0,\$a1
0+00c0 <[^>]*> addiu \$a0,\$gp,-16383
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+00c4 <[^>]*> addu \$a0,\$a0,\$a1
0+00c8 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+00cc <[^>]*> addu \$a0,\$a0,\$a1
0+00d0 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00d4 <[^>]*> addu \$a0,\$a0,\$a1
0+00d8 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+00dc <[^>]*> addu \$a0,\$a0,\$a1
0+00e0 <[^>]*> addiu \$a0,\$gp,1
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00e4 <[^>]*> addu \$a0,\$a0,\$a1
0+00e8 <[^>]*> addiu \$a0,\$gp,-16383
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00ec <[^>]*> addu \$a0,\$a0,\$a1
0+00f0 <[^>]*> addiu \$a0,\$gp,-15383
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00f4 <[^>]*> addu \$a0,\$a0,\$a1
0+00f8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00f8 RELHI external_text_label
0+00fc <[^>]*> addiu \$a0,\$a0,252
[ 	]*RELOC: 0+00fc RELLO external_text_label
0+0100 <[^>]*> li \$a0,248
...
