#objdump: -dr
#name: MIPS lb-empic
#as: -mips1 -membedded-pic
#source: lb-pic.s

# Test the lb macro with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lb \$a0,0\(\$zero\)
0+0004 <[^>]*> lb \$a0,1\(\$zero\)
0+0008 <[^>]*> lui \$a0,1
0+000c <[^>]*> lb \$a0,-32768\(\$a0\)
0+0010 <[^>]*> lb \$a0,-32768\(\$zero\)
0+0014 <[^>]*> lui \$a0,1
0+0018 <[^>]*> lb \$a0,0\(\$a0\)
0+001c <[^>]*> lui \$a0,2
0+0020 <[^>]*> lb \$a0,-23131\(\$a0\)
0+0024 <[^>]*> lb \$a0,0\(\$a1\)
0+0028 <[^>]*> lb \$a0,1\(\$a1\)
0+002c <[^>]*> lui \$a0,1
0+0030 <[^>]*> addu \$a0,\$a0,\$a1
0+0034 <[^>]*> lb \$a0,-32768\(\$a0\)
0+0038 <[^>]*> lb \$a0,-32768\(\$a1\)
0+003c <[^>]*> lui \$a0,1
0+0040 <[^>]*> addu \$a0,\$a0,\$a1
0+0044 <[^>]*> lb \$a0,0\(\$a0\)
0+0048 <[^>]*> lui \$a0,2
0+004c <[^>]*> addu \$a0,\$a0,\$a1
0+0050 <[^>]*> lb \$a0,-23131\(\$a0\)
0+0054 <[^>]*> lb \$a0,-16384\(\$gp\)
[ 	]*RELOC: 0+0054 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+0058 <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0058 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+005c <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+005c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0060 <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0060 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+0064 <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0064 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0068 <[^>]*> lb \$a0,-16384\(\$gp\)
[ 	]*RELOC: 0+0068 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+006c <[^>]*> lb \$a0,-15384\(\$gp\)
[ 	]*RELOC: 0+006c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0070 <[^>]*> lb \$a0,-16383\(\$gp\)
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+0074 <[^>]*> lb \$a0,1\(\$gp\)
[ 	]*RELOC: 0+0074 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+0078 <[^>]*> lb \$a0,1\(\$gp\)
[ 	]*RELOC: 0+0078 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+007c <[^>]*> lb \$a0,1\(\$gp\)
[ 	]*RELOC: 0+007c [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+0080 <[^>]*> lb \$a0,1\(\$gp\)
[ 	]*RELOC: 0+0080 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0084 <[^>]*> lb \$a0,-16383\(\$gp\)
[ 	]*RELOC: 0+0084 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0088 <[^>]*> lb \$a0,-15383\(\$gp\)
[ 	]*RELOC: 0+0088 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+008c <[^>]*> addu \$a0,\$a1,\$gp
0+0090 <[^>]*> lb \$a0,-16384\(\$a0\)
[ 	]*RELOC: 0+0090 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+0094 <[^>]*> addu \$a0,\$a1,\$gp
0+0098 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0098 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+009c <[^>]*> addu \$a0,\$a1,\$gp
0+00a0 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00a4 <[^>]*> addu \$a0,\$a1,\$gp
0+00a8 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+00ac <[^>]*> addu \$a0,\$a1,\$gp
0+00b0 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00b4 <[^>]*> addu \$a0,\$a1,\$gp
0+00b8 <[^>]*> lb \$a0,-16384\(\$a0\)
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00bc <[^>]*> addu \$a0,\$a1,\$gp
0+00c0 <[^>]*> lb \$a0,-15384\(\$a0\)
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00c4 <[^>]*> addu \$a0,\$a1,\$gp
0+00c8 <[^>]*> lb \$a0,-16383\(\$a0\)
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sdata.*
0+00cc <[^>]*> addu \$a0,\$a1,\$gp
0+00d0 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_data_label
0+00d4 <[^>]*> addu \$a0,\$a1,\$gp
0+00d8 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00dc <[^>]*> addu \$a0,\$a1,\$gp
0+00e0 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*GPREL[A-Z0-9_]* big_external_common
0+00e4 <[^>]*> addu \$a0,\$a1,\$gp
0+00e8 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00ec <[^>]*> addu \$a0,\$a1,\$gp
0+00f0 <[^>]*> lb \$a0,-16383\(\$a0\)
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00f4 <[^>]*> addu \$a0,\$a1,\$gp
0+00f8 <[^>]*> lb \$a0,-15383\(\$a0\)
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
...
