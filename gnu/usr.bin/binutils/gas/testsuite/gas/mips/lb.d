#objdump: -dr
#name: MIPS lb
#as: -mips1

# Test the lb macro.

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
0+0054 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0054 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0058 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0058 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+005c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+005c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0060 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0060 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0064 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0068 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0068 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+006c <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+006c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0070 <[^>]*> lb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0074 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0074 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0078 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0078 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+007c <[^>]*> lb \$a0,-16384\(\$gp\)
[ 	]*RELOC: 0+007c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0080 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0080 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0084 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0084 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0088 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0088 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+008c <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+008c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0090 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0090 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0094 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+0094 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0098 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+009c <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00a0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00a4 <[^>]*> lb \$a0,1\(\$a0\)
[ 	]*RELOC: 0+00a4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00a8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00ac <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+00ac [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00b0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00b4 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00b8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00bc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00bc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00c0 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00cc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00cc [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00d0 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00d4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00d8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00dc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00dc [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00e0 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00e4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00e8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00ec <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00ec [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00f0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00f4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00f8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00fc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+00fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0100 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0100 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0104 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0104 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0108 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0108 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+010c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+010c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0110 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0110 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0114 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0114 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0118 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0118 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+011c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+011c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0120 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0120 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0124 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0124 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0128 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+012c <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+012c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0130 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0130 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0134 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0134 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0140 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0140 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0144 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0144 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0148 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+014c <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0150 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0150 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0154 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0154 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0158 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+015c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+015c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0160 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0160 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0164 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0164 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0168 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+016c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0170 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0170 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0174 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0174 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0178 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+017c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0180 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0180 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0184 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0184 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0188 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+018c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+018c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0190 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0190 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0194 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0194 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0198 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+019c <[^>]*> addu \$a0,\$a0,\$a1
0+01a0 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+01a0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01a4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> addu \$a0,\$a0,\$a1
0+01ac <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01b0 <[^>]*> addu \$a0,\$a1,\$gp
0+01b4 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01bc <[^>]*> addu \$a0,\$a0,\$a1
0+01c0 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01c4 <[^>]*> addu \$a0,\$a1,\$gp
0+01c8 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+01cc <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01cc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01d0 <[^>]*> addu \$a0,\$a0,\$a1
0+01d4 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01d8 <[^>]*> addu \$a0,\$a1,\$gp
0+01dc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+01dc [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+01e0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01e0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01e4 <[^>]*> addu \$a0,\$a0,\$a1
0+01e8 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01ec <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01ec [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01f0 <[^>]*> addu \$a0,\$a0,\$a1
0+01f4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01f8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01fc <[^>]*> addu \$a0,\$a0,\$a1
0+0200 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0200 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0204 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0208 <[^>]*> addu \$a0,\$a0,\$a1
0+020c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0210 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0210 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0214 <[^>]*> addu \$a0,\$a0,\$a1
0+0218 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+021c <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+021c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0220 <[^>]*> addu \$a0,\$a0,\$a1
0+0224 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0224 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0228 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+022c <[^>]*> addu \$a0,\$a0,\$a1
0+0230 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0230 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0234 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0238 <[^>]*> addu \$a0,\$a0,\$a1
0+023c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0240 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0240 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0244 <[^>]*> addu \$a0,\$a0,\$a1
0+0248 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+024c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0250 <[^>]*> addu \$a0,\$a0,\$a1
0+0254 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0254 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0258 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+025c <[^>]*> addu \$a0,\$a0,\$a1
0+0260 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0260 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0264 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0268 <[^>]*> addu \$a0,\$a0,\$a1
0+026c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0270 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0270 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0274 <[^>]*> addu \$a0,\$a0,\$a1
0+0278 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+027c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+027c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0280 <[^>]*> addu \$a0,\$a0,\$a1
0+0284 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0284 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0288 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0288 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+028c <[^>]*> addu \$a0,\$a0,\$a1
0+0290 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0290 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0294 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0298 <[^>]*> addu \$a0,\$a0,\$a1
0+029c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+029c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02a0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02a0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02a4 <[^>]*> addu \$a0,\$a0,\$a1
0+02a8 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+02ac <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02ac [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02b0 <[^>]*> addu \$a0,\$a0,\$a1
0+02b4 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02b8 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02bc <[^>]*> addu \$a0,\$a0,\$a1
0+02c0 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+02c0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02c4 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> addu \$a0,\$a0,\$a1
0+02cc <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+02cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02d0 <[^>]*> lui \$a0,0
[ 	]*RELOC: 0+02d0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d4 <[^>]*> addu \$a0,\$a0,\$a1
0+02d8 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02dc [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+02e0 <[^>]*> addu \$a0,\$a0,\$a1
0+02e4 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+02e4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02e8 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02e8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+02ec <[^>]*> addu \$a0,\$a0,\$a1
0+02f0 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+02f0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02f4 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+02f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02f8 <[^>]*> addu \$a0,\$a0,\$a1
0+02fc <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+02fc [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0300 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0300 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0304 <[^>]*> addu \$a0,\$a0,\$a1
0+0308 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0308 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+030c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+030c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0310 <[^>]*> addu \$a0,\$a0,\$a1
0+0314 <[^>]*> lb \$a0,0\(\$a0\)
[ 	]*RELOC: 0+0314 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0318 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0318 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+031c <[^>]*> addu \$a0,\$a0,\$a1
0+0320 <[^>]*> lb \$a0,[0-9]+\(\$a0\)
[ 	]*RELOC: 0+0320 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0324 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0324 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0328 <[^>]*> addu \$a0,\$a0,\$a1
0+032c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+032c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0330 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0330 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0334 <[^>]*> addu \$a0,\$a0,\$a1
0+0338 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0338 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+033c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+033c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0340 <[^>]*> addu \$a0,\$a0,\$a1
0+0344 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0344 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0348 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0348 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+034c <[^>]*> addu \$a0,\$a0,\$a1
0+0350 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0350 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0354 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0354 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0358 <[^>]*> addu \$a0,\$a0,\$a1
0+035c <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+035c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0360 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0360 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0364 <[^>]*> addu \$a0,\$a0,\$a1
0+0368 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0368 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+036c <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+036c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0370 <[^>]*> addu \$a0,\$a0,\$a1
0+0374 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0374 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0378 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+0378 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+037c <[^>]*> addu \$a0,\$a0,\$a1
0+0380 <[^>]*> lb \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+0380 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0384 <[^>]*> lbu \$a0,0\(\$zero\)
0+0388 <[^>]*> lh \$a0,0\(\$zero\)
0+038c <[^>]*> lhu \$a0,0\(\$zero\)
0+0390 <[^>]*> lw \$a0,0\(\$zero\)
0+0394 <[^>]*> lwl \$a0,0\(\$zero\)
0+0398 <[^>]*> lwr \$a0,0\(\$zero\)
0+039c <[^>]*> ll \$a0,0\(\$zero\)
0+03a0 <[^>]*> lwc1 \$f4,0\(\$zero\)
0+03a4 <[^>]*> lwc2 \$4,0\(\$zero\)
0+03a8 <[^>]*> lwc3 \$4,0\(\$zero\)
...
