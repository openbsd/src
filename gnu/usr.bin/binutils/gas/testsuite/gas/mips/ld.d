#objdump: -dr
#name: MIPS ld

# Test the ld macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw \$a0,0\(\$zero\)
0+0004 <[^>]*> lw \$a1,4\(\$zero\)
0+0008 <[^>]*> lw \$a0,1\(\$zero\)
0+000c <[^>]*> lw \$a1,5\(\$zero\)
0+0010 <[^>]*> lui \$at,1
0+0014 <[^>]*> lw \$a0,-32768\(\$at\)
0+0018 <[^>]*> lw \$a1,-32764\(\$at\)
0+001c <[^>]*> lw \$a0,-32768\(\$zero\)
0+0020 <[^>]*> lw \$a1,-32764\(\$zero\)
0+0024 <[^>]*> lui \$at,1
0+0028 <[^>]*> lw \$a0,0\(\$at\)
0+002c <[^>]*> lw \$a1,4\(\$at\)
0+0030 <[^>]*> lui \$at,2
0+0034 <[^>]*> lw \$a0,-23131\(\$at\)
0+0038 <[^>]*> lw \$a1,-23127\(\$at\)
...
0+0040 <[^>]*> lw \$a0,0\(\$a1\)
0+0044 <[^>]*> lw \$a1,4\(\$a1\)
...
0+004c <[^>]*> lw \$a0,1\(\$a1\)
0+0050 <[^>]*> lw \$a1,5\(\$a1\)
0+0054 <[^>]*> lui \$at,1
0+0058 <[^>]*> addu \$at,\$a1,\$at
0+005c <[^>]*> lw \$a0,-32768\(\$at\)
0+0060 <[^>]*> lw \$a1,-32764\(\$at\)
...
0+0068 <[^>]*> lw \$a0,-32768\(\$a1\)
0+006c <[^>]*> lw \$a1,-32764\(\$a1\)
0+0070 <[^>]*> lui \$at,1
0+0074 <[^>]*> addu \$at,\$a1,\$at
0+0078 <[^>]*> lw \$a0,0\(\$at\)
0+007c <[^>]*> lw \$a1,4\(\$at\)
0+0080 <[^>]*> lui \$at,2
0+0084 <[^>]*> addu \$at,\$a1,\$at
0+0088 <[^>]*> lw \$a0,-23131\(\$at\)
0+008c <[^>]*> lw \$a1,-23127\(\$at\)
0+0090 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0090 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0094 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0094 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0098 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0098 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+009c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+009c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00a0 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00a4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00a4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00a8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00ac <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00ac [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00b0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00b4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00bc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00bc [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00c0 <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00c4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00c8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00cc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00d0 <[^>]*> lw \$a0,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00d4 <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00d8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00dc <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00dc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00e0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00e4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00e8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00ec <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00ec [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00f0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+00f4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00f8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+00fc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+00fc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0100 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0100 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0104 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0104 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0108 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0108 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+010c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+010c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0110 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0110 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0114 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0114 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0118 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0118 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+011c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+011c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0120 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0120 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0124 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0124 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0128 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+012c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+012c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0130 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0130 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0134 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0134 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0138 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+013c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0140 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0140 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0144 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0148 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+014c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0150 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0150 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0154 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0154 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0158 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+015c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+015c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0160 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0160 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0164 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0164 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0168 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+016c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0170 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0170 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0174 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0178 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+017c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0180 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0180 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0184 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0184 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0188 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+018c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+018c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0190 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0190 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0194 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0194 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0198 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+019c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+019c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01a0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01a0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01a4 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01a8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01ac <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01b0 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01b0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01b4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01bc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01bc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01c0 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01c4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01c8 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01cc <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01cc [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01d0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01d0 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01d4 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01d8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01dc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01dc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01e0 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01e0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01e4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01e8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01ec <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01f0 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01f0 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01f4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01f8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01fc <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0200 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0200 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0204 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0208 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+020c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0210 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0210 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0214 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0214 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0218 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+021c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+021c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0220 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0220 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0224 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0224 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0228 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+022c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+022c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0230 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0230 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0234 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0238 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+023c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0240 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0240 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0244 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0244 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0248 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+024c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0250 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0250 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0254 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0254 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0258 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+025c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+025c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0260 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0260 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0264 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0268 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+026c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0270 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0270 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0274 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0274 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0278 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+027c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+027c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0280 <[^>]*> addu \$at,\$a1,\$at
0+0284 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0284 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0288 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+028c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+028c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0290 <[^>]*> addu \$at,\$a1,\$at
0+0294 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0294 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0298 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0298 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
...
0+02a0 <[^>]*> addu \$at,\$a1,\$gp
0+02a4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+02a4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+02a8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+02ac <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02ac [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02b0 <[^>]*> addu \$at,\$a1,\$at
0+02b4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+02b4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
...
0+02c0 <[^>]*> addu \$at,\$a1,\$gp
0+02c4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+02c4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+02c8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+02cc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02cc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02d0 <[^>]*> addu \$at,\$a1,\$at
0+02d4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02d4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02d8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
...
0+02e0 <[^>]*> addu \$at,\$a1,\$gp
0+02e4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02e4 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+02e8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02e8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+02ec <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02ec [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+02f0 <[^>]*> addu \$at,\$a1,\$at
0+02f4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02f4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02f8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02f8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02fc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+02fc [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0300 <[^>]*> addu \$at,\$a1,\$at
0+0304 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0304 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0308 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0308 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+030c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+030c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0310 <[^>]*> addu \$at,\$a1,\$at
0+0314 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0314 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0318 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0318 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+031c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+031c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0320 <[^>]*> addu \$at,\$a1,\$at
0+0324 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0324 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0328 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0328 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+032c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+032c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0330 <[^>]*> addu \$at,\$a1,\$at
0+0334 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0334 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0338 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0338 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+033c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+033c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0340 <[^>]*> addu \$at,\$a1,\$at
0+0344 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0344 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0348 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0348 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+034c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+034c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0350 <[^>]*> addu \$at,\$a1,\$at
0+0354 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0354 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0358 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0358 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+035c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+035c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0360 <[^>]*> addu \$at,\$a1,\$at
0+0364 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0364 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0368 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0368 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+036c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+036c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0370 <[^>]*> addu \$at,\$a1,\$at
0+0374 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0374 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0378 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0378 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+037c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+037c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0380 <[^>]*> addu \$at,\$a1,\$at
0+0384 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0384 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0388 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0388 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+038c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+038c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0390 <[^>]*> addu \$at,\$a1,\$at
0+0394 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0394 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0398 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0398 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+039c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+039c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+03a0 <[^>]*> addu \$at,\$a1,\$at
0+03a4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03a4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+03a8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03a8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+03ac <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03ac [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+03b0 <[^>]*> addu \$at,\$a1,\$at
0+03b4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03b4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+03b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03b8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+03bc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+03bc [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+03c0 <[^>]*> addu \$at,\$a1,\$at
0+03c4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03c4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+03c8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03c8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+03cc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+03cc [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+03d0 <[^>]*> addu \$at,\$a1,\$at
0+03d4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03d4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+03d8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03d8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+03dc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+03dc [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+03e0 <[^>]*> addu \$at,\$a1,\$at
0+03e4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03e4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+03e8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+03ec <[^>]*> lui \$at,0
[ 	]*RELOC: 0+03ec [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+03f0 <[^>]*> addu \$at,\$a1,\$at
0+03f4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03f4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+03f8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+03fc <[^>]*> lui \$at,0
[ 	]*RELOC: 0+03fc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0400 <[^>]*> addu \$at,\$a1,\$at
0+0404 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0404 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0408 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0408 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+040c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+040c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0410 <[^>]*> addu \$at,\$a1,\$at
0+0414 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0414 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0418 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0418 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+041c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+041c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0420 <[^>]*> addu \$at,\$a1,\$at
0+0424 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0424 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0428 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0428 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+042c <[^>]*> lui \$at,0
[ 	]*RELOC: 0+042c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0430 <[^>]*> addu \$at,\$a1,\$at
0+0434 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0434 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0438 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0438 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+043c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+043c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0440 <[^>]*> addu \$at,\$a1,\$at
0+0444 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0444 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0448 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0448 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+044c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+044c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0450 <[^>]*> addu \$at,\$a1,\$at
0+0454 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0454 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0458 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0458 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+045c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+045c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0460 <[^>]*> addu \$at,\$a1,\$at
0+0464 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0464 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0468 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0468 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+046c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+046c [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0470 <[^>]*> addu \$at,\$a1,\$at
0+0474 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0474 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0478 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0478 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+047c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+047c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0480 <[^>]*> addu \$at,\$a1,\$at
0+0484 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0484 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0488 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0488 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+048c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+048c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0490 <[^>]*> addu \$at,\$a1,\$at
0+0494 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0494 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0498 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0498 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+049c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+049c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+04a0 <[^>]*> addu \$at,\$a1,\$at
0+04a4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04a4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+04a8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04a8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+04ac <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04ac [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+04b0 <[^>]*> addu \$at,\$a1,\$at
0+04b4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04b4 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+04b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04b8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+04bc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04bc [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+04c0 <[^>]*> addu \$at,\$a1,\$at
0+04c4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04c4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+04c8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+04cc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04cc [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+04d0 <[^>]*> addu \$at,\$a1,\$at
0+04d4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04d4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+04d8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04d8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+04dc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04dc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+04e0 <[^>]*> addu \$at,\$a1,\$at
0+04e4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04e4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+04e8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+04ec <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04ec [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+04f0 <[^>]*> addu \$at,\$a1,\$at
0+04f4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04f4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+04f8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04f8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+04fc <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+04fc [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0500 <[^>]*> addu \$at,\$a1,\$at
0+0504 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0504 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0508 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0508 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+050c <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+050c [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0510 <[^>]*> addu \$at,\$a1,\$at
0+0514 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0514 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0518 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0518 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+051c <[^>]*> lwc1 \$f[45],0\(\$zero\)
0+0520 <[^>]*> lwc1 \$f[45],4\(\$zero\)
0+0524 <[^>]*> lwc1 \$f[45],1\(\$zero\)
0+0528 <[^>]*> lwc1 \$f[45],5\(\$zero\)
0+052c <[^>]*> lui \$at,1
0+0530 <[^>]*> lwc1 \$f[45],-32768\(\$at\)
0+0534 <[^>]*> lwc1 \$f[45],-32764\(\$at\)
0+0538 <[^>]*> lwc1 \$f[45],-32768\(\$zero\)
0+053c <[^>]*> lwc1 \$f[45],-32764\(\$zero\)
0+0540 <[^>]*> lwc1 \$f[45],0\(\$a1\)
0+0544 <[^>]*> lwc1 \$f[45],4\(\$a1\)
0+0548 <[^>]*> lwc1 \$f[45],1\(\$a1\)
0+054c <[^>]*> lwc1 \$f[45],5\(\$a1\)
0+0550 <[^>]*> lui \$at,1
0+0554 <[^>]*> addu \$at,\$a1,\$at
0+0558 <[^>]*> lwc1 \$f[45],-32768\(\$at\)
0+055c <[^>]*> lwc1 \$f[45],-32764\(\$at\)
0+0560 <[^>]*> lwc1 \$f[45],-32768\(\$a1\)
0+0564 <[^>]*> lwc1 \$f[45],-32764\(\$a1\)
0+0568 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+0568 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+056c <[^>]*> addu \$at,\$a1,\$at
0+0570 <[^>]*> lwc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0570 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0574 <[^>]*> lwc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0574 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
...
0+057c <[^>]*> swc1 \$f[45],0\(\$zero\)
0+0580 <[^>]*> swc1 \$f[45],4\(\$zero\)
0+0584 <[^>]*> swc1 \$f[45],1\(\$zero\)
0+0588 <[^>]*> swc1 \$f[45],5\(\$zero\)
0+058c <[^>]*> lui \$at,1
0+0590 <[^>]*> swc1 \$f[45],-32768\(\$at\)
0+0594 <[^>]*> swc1 \$f[45],-32764\(\$at\)
0+0598 <[^>]*> swc1 \$f[45],-32768\(\$zero\)
0+059c <[^>]*> swc1 \$f[45],-32764\(\$zero\)
0+05a0 <[^>]*> swc1 \$f[45],0\(\$a1\)
0+05a4 <[^>]*> swc1 \$f[45],4\(\$a1\)
0+05a8 <[^>]*> swc1 \$f[45],1\(\$a1\)
0+05ac <[^>]*> swc1 \$f[45],5\(\$a1\)
0+05b0 <[^>]*> lui \$at,1
0+05b4 <[^>]*> addu \$at,\$a1,\$at
0+05b8 <[^>]*> swc1 \$f[45],-32768\(\$at\)
0+05bc <[^>]*> swc1 \$f[45],-32764\(\$at\)
0+05c0 <[^>]*> swc1 \$f[45],-32768\(\$a1\)
0+05c4 <[^>]*> swc1 \$f[45],-32764\(\$a1\)
0+05c8 <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+05c8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+05cc <[^>]*> addu \$at,\$a1,\$at
0+05d0 <[^>]*> swc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05d0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+05d4 <[^>]*> swc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05d4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+05d8 <[^>]*> sw \$a0,0\(\$zero\)
0+05dc <[^>]*> sw \$a1,4\(\$zero\)
0+05e0 <[^>]*> lui \$a0,[-0-9]+
[ 	]*RELOC: 0+05e0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+05e4 <[^>]*> daddu \$a0,\$a0,\$a1
0+05e8 <[^>]*> ld \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+05e8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+05ec <[^>]*> lui \$at,[-0-9]+
[ 	]*RELOC: 0+05ec [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+05f0 <[^>]*> daddu \$at,\$at,\$a1
0+05f4 <[^>]*> sd \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05f4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
...
