#objdump: -dr
#name: MIPS sb
#as: -mips1

# Test the sb macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> sb \$a0,0\(\$zero\)
0+0004 <[^>]*> sb \$a0,1\(\$zero\)
0+0008 <[^>]*> lui \$at,1
0+000c <[^>]*> sb \$a0,-32768\(\$at\)
0+0010 <[^>]*> sb \$a0,-32768\(\$zero\)
0+0014 <[^>]*> lui \$at,1
0+0018 <[^>]*> sb \$a0,0\(\$at\)
0+001c <[^>]*> lui \$at,2
0+0020 <[^>]*> sb \$a0,-23131\(\$at\)
0+0024 <[^>]*> sb \$a0,0\(\$a1\)
0+0028 <[^>]*> sb \$a0,1\(\$a1\)
0+002c <[^>]*> lui \$at,1
0+0030 <[^>]*> addu \$at,\$at,\$a1
0+0034 <[^>]*> sb \$a0,-32768\(\$at\)
0+0038 <[^>]*> sb \$a0,-32768\(\$a1\)
0+003c <[^>]*> lui \$at,1
0+0040 <[^>]*> addu \$at,\$at,\$a1
0+0044 <[^>]*> sb \$a0,0\(\$at\)
0+0048 <[^>]*> lui \$at,2
0+004c <[^>]*> addu \$at,\$at,\$a1
0+0050 <[^>]*> sb \$a0,-23131\(\$at\)
0+0054 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0054 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0058 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0058 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+005c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+005c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0060 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+0060 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0064 <[^>]*> sb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0064 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0068 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0068 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+006c <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+006c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0070 <[^>]*> sb \$a0,0\(\$gp\)
[ 	]*RELOC: 0+0070 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0074 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0074 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0078 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0078 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+007c <[^>]*> sb \$a0,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+007c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0080 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0080 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0084 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0084 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0088 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0088 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+008c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+008c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0090 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0090 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0094 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0094 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0098 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0098 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+009c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+009c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00a0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00a4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00a4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00a8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00ac <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00ac [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00b0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00b4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00b8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00bc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00bc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00c0 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00c4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00c8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00cc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00cc [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00d0 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00d4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00d8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+00dc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00dc [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+00e0 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00e4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00e8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+00ec <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00ec [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+00f0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00f4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00f8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00fc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0100 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0100 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0104 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0104 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0108 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0108 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+010c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+010c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0110 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0110 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0114 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0114 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0118 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0118 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+011c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+011c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0120 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0120 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0124 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0124 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0128 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0128 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+012c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+012c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0130 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0130 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0134 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+0134 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0140 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0140 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0144 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+0144 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0148 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0148 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+014c <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0150 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0150 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0154 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0154 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0158 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0158 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+015c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+015c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0160 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0160 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0164 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0164 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0168 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+016c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0170 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0170 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0174 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0174 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0178 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0178 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+017c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0180 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0180 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0184 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0184 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0188 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0188 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+018c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+018c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0190 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0190 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0194 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0194 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0198 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+019c <[^>]*> addu \$at,\$at,\$a1
0+01a0 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01a0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01a8 <[^>]*> addu \$at,\$at,\$a1
0+01ac <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01b0 <[^>]*> addu \$at,\$a1,\$gp
0+01b4 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+01b8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01bc <[^>]*> addu \$at,\$at,\$a1
0+01c0 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01c4 <[^>]*> addu \$at,\$a1,\$gp
0+01c8 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+01cc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01cc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01d0 <[^>]*> addu \$at,\$at,\$a1
0+01d4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01d8 <[^>]*> addu \$at,\$a1,\$gp
0+01dc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01dc [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+01e0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01e0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01e4 <[^>]*> addu \$at,\$at,\$a1
0+01e8 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01ec <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01ec [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01f0 <[^>]*> addu \$at,\$at,\$a1
0+01f4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01f8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01fc <[^>]*> addu \$at,\$at,\$a1
0+0200 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0200 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0204 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0208 <[^>]*> addu \$at,\$at,\$a1
0+020c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0210 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0210 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0214 <[^>]*> addu \$at,\$at,\$a1
0+0218 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+021c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+021c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0220 <[^>]*> addu \$at,\$at,\$a1
0+0224 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0224 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0228 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+022c <[^>]*> addu \$at,\$at,\$a1
0+0230 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0230 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0238 <[^>]*> addu \$at,\$at,\$a1
0+023c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0240 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0240 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0244 <[^>]*> addu \$at,\$at,\$a1
0+0248 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+024c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0250 <[^>]*> addu \$at,\$at,\$a1
0+0254 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0254 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0258 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+025c <[^>]*> addu \$at,\$at,\$a1
0+0260 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0260 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0268 <[^>]*> addu \$at,\$at,\$a1
0+026c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0270 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0270 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0274 <[^>]*> addu \$at,\$at,\$a1
0+0278 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+027c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+027c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0280 <[^>]*> addu \$at,\$at,\$a1
0+0284 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0284 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0288 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0288 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+028c <[^>]*> addu \$at,\$at,\$a1
0+0290 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0290 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0294 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0294 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0298 <[^>]*> addu \$at,\$at,\$a1
0+029c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+029c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02a0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02a0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02a4 <[^>]*> addu \$at,\$at,\$a1
0+02a8 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+02ac <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02ac [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02b0 <[^>]*> addu \$at,\$at,\$a1
0+02b4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02b8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+02bc <[^>]*> addu \$at,\$at,\$a1
0+02c0 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02c0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+02c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c8 <[^>]*> addu \$at,\$at,\$a1
0+02cc <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02d0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02d0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+02d4 <[^>]*> addu \$at,\$at,\$a1
0+02d8 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02dc [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+02e0 <[^>]*> addu \$at,\$at,\$a1
0+02e4 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02e4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02e8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02e8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+02ec <[^>]*> addu \$at,\$at,\$a1
0+02f0 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+02f0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02f4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+02f4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+02f8 <[^>]*> addu \$at,\$at,\$a1
0+02fc <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+02fc [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0300 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0300 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0304 <[^>]*> addu \$at,\$at,\$a1
0+0308 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+0308 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+030c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+030c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0310 <[^>]*> addu \$at,\$at,\$a1
0+0314 <[^>]*> sb \$a0,0\(\$at\)
[ 	]*RELOC: 0+0314 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0318 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0318 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+031c <[^>]*> addu \$at,\$at,\$a1
0+0320 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0320 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0324 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0324 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0328 <[^>]*> addu \$at,\$at,\$a1
0+032c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+032c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0330 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0330 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0334 <[^>]*> addu \$at,\$at,\$a1
0+0338 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0338 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+033c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+033c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0340 <[^>]*> addu \$at,\$at,\$a1
0+0344 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0344 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0348 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0348 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+034c <[^>]*> addu \$at,\$at,\$a1
0+0350 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0350 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0354 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0354 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0358 <[^>]*> addu \$at,\$at,\$a1
0+035c <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+035c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0360 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0360 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0364 <[^>]*> addu \$at,\$at,\$a1
0+0368 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0368 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+036c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+036c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0370 <[^>]*> addu \$at,\$at,\$a1
0+0374 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0374 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0378 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0378 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+037c <[^>]*> addu \$at,\$at,\$a1
0+0380 <[^>]*> sb \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0380 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0384 <[^>]*> sw \$a0,0\(\$zero\)
0+0388 <[^>]*> sw \$a1,4\(\$zero\)
0+038c <[^>]*> sh \$a0,0\(\$zero\)
0+0390 <[^>]*> sw \$a0,0\(\$zero\)
0+0394 <[^>]*> sc \$a0,0\(\$zero\)
0+0398 <[^>]*> swc1 \$f4,0\(\$zero\)
0+039c <[^>]*> swc2 \$4,0\(\$zero\)
0+03a0 <[^>]*> swc3 \$4,0\(\$zero\)
0+03a4 <[^>]*> swc1 \$f4,0\(\$zero\)
0+03a8 <[^>]*> swl \$a0,0\(\$zero\)
0+03ac <[^>]*> swr \$a0,0\(\$zero\)
