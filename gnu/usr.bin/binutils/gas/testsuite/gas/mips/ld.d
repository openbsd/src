#objdump: -dr
#name: MIPS ld

# Test the ld macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw \$a0,0\(\$zero\)
0+0004 <[^>]*> lw \$a1,4\(\$zero\)
0+0008 <[^>]*> lw \$a0,1\(\$zero\)
0+000c <[^>]*> lw \$a1,5\(\$zero\)
0+0010 <[^>]*> lui \$at,0x1
0+0014 <[^>]*> lw \$a0,-32768\(\$at\)
0+0018 <[^>]*> lw \$a1,-32764\(\$at\)
0+001c <[^>]*> lw \$a0,-32768\(\$zero\)
0+0020 <[^>]*> lw \$a1,-32764\(\$zero\)
0+0024 <[^>]*> lui \$at,0x1
0+0028 <[^>]*> lw \$a0,0\(\$at\)
0+002c <[^>]*> lw \$a1,4\(\$at\)
0+0030 <[^>]*> lui \$at,0x2
0+0034 <[^>]*> lw \$a0,-23131\(\$at\)
0+0038 <[^>]*> lw \$a1,-23127\(\$at\)
...
0+0040 <[^>]*> lw \$a0,0\(\$a1\)
0+0044 <[^>]*> lw \$a1,4\(\$a1\)
...
0+004c <[^>]*> lw \$a0,1\(\$a1\)
0+0050 <[^>]*> lw \$a1,5\(\$a1\)
0+0054 <[^>]*> lui \$at,0x1
0+0058 <[^>]*> addu \$at,\$a1,\$at
0+005c <[^>]*> lw \$a0,-32768\(\$at\)
0+0060 <[^>]*> lw \$a1,-32764\(\$at\)
...
0+0068 <[^>]*> lw \$a0,-32768\(\$a1\)
0+006c <[^>]*> lw \$a1,-32764\(\$a1\)
0+0070 <[^>]*> lui \$at,0x1
0+0074 <[^>]*> addu \$at,\$a1,\$at
0+0078 <[^>]*> lw \$a0,0\(\$at\)
0+007c <[^>]*> lw \$a1,4\(\$at\)
0+0080 <[^>]*> lui \$at,0x2
0+0084 <[^>]*> addu \$at,\$a1,\$at
0+0088 <[^>]*> lw \$a0,-23131\(\$at\)
0+008c <[^>]*> lw \$a1,-23127\(\$at\)
0+0090 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0090 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0094 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0094 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0098 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0098 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+009c <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+009c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00a0 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+00a0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00a4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00a4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00a8 <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00a8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00ac <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00ac [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00b0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00b0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00b4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+00b4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00b8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+00bc <[^>]*> lw \$a0,0\(\$gp\)
[ 	]*RELOC: 0+00bc [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00c0 <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00c0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+00c4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00c4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+00c8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00cc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+00d0 <[^>]*> lw \$a0,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00d0 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00d4 <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+00d4 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+00d8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00d8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+00dc <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00dc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00e0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00e0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+00e4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00e4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+00e8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00e8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00ec <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00ec [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+00f0 <[^>]*> lw \$a0,1\(\$gp\)
[ 	]*RELOC: 0+00f0 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00f4 <[^>]*> lw \$a1,5\(\$gp\)
[ 	]*RELOC: 0+00f4 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+00f8 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+00f8 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+00fc <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+00fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0100 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0100 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0104 <[^>]*> lw \$a0,1\(\$gp\)
[ 	]*RELOC: 0+0104 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0108 <[^>]*> lw \$a1,5\(\$gp\)
[ 	]*RELOC: 0+0108 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+010c <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+010c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0110 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0110 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0114 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0114 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0118 <[^>]*> lw \$a0,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+0118 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+011c <[^>]*> lw \$a1,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+011c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0120 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0120 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0124 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0124 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0128 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0128 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+012c <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+012c [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0130 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0130 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0134 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0134 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0138 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0138 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+013c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+013c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0140 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0140 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0144 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0144 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0148 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0148 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+014c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+014c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0150 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0150 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0154 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0154 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0158 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0158 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+015c <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+015c [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0160 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0160 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0164 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0164 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0168 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0168 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+016c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+016c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0170 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0170 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0174 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0174 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0178 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0178 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+017c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+017c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0180 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0180 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0184 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0184 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0188 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0188 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+018c <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+018c [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0190 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0190 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0194 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0194 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0198 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0198 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+019c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+019c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01a0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01a0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01a4 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01a4 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01a8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01a8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01ac <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01ac [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+01b0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01b0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+01b4 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01b4 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01b8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01b8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+01bc <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+01bc [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+01c0 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01c0 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01c4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01c4 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+01c8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01c8 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+01cc <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01cc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01d0 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01d0 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+01d4 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01d4 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+01d8 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01d8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01dc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01dc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+01e0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01e0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+01e4 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01e4 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01e8 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01e8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+01ec <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01ec [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+01f0 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01f0 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01f4 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+01f4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+01f8 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+01f8 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+01fc <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+01fc [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0200 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0200 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0204 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0204 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0208 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0208 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+020c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+020c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0210 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0210 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0214 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0214 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0218 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0218 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+021c <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+021c [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0220 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0220 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0224 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0224 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0228 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0228 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+022c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+022c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0230 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0230 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0234 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0234 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0238 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0238 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+023c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+023c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0240 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0240 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0244 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0244 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0248 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0248 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+024c <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+024c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0250 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0250 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0254 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0254 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0258 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0258 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+025c <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+025c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0260 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0260 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0264 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0264 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0268 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0268 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+026c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+026c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0270 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0270 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0274 <[^>]*> addu \$at,\$a1,\$at
0+0278 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0278 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+027c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+027c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0280 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0280 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0284 <[^>]*> addu \$at,\$a1,\$at
0+0288 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0288 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+028c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+028c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
...
0+0294 <[^>]*> addu \$at,\$a1,\$gp
0+0298 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0298 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+029c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+029c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+02a0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+02a0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+02a4 <[^>]*> addu \$at,\$a1,\$at
0+02a8 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+02a8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+02ac <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02ac [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
...
0+02b4 <[^>]*> addu \$at,\$a1,\$gp
0+02b8 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+02b8 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+02bc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02bc [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+02c0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+02c0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+02c4 <[^>]*> addu \$at,\$a1,\$at
0+02c8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02c8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+02cc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02cc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
...
0+02d4 <[^>]*> addu \$at,\$a1,\$gp
0+02d8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02d8 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+02dc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02dc [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+02e0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+02e0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+02e4 <[^>]*> addu \$at,\$a1,\$at
0+02e8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02e8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02ec <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02ec [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+02f0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+02f0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+02f4 <[^>]*> addu \$at,\$a1,\$at
0+02f8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02f8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+02fc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+02fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
...
0+0304 <[^>]*> addu \$at,\$a1,\$gp
0+0308 <[^>]*> lw \$a0,1\(\$at\)
[ 	]*RELOC: 0+0308 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+030c <[^>]*> lw \$a1,5\(\$at\)
[ 	]*RELOC: 0+030c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_data_label
0+0310 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0310 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0314 <[^>]*> addu \$at,\$a1,\$at
0+0318 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0318 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+031c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+031c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
...
0+0324 <[^>]*> addu \$at,\$a1,\$gp
0+0328 <[^>]*> lw \$a0,1\(\$at\)
[ 	]*RELOC: 0+0328 [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+032c <[^>]*> lw \$a1,5\(\$at\)
[ 	]*RELOC: 0+032c [A-Z0-9_]*GPREL[A-Z0-9_]* small_external_common
0+0330 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0330 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0334 <[^>]*> addu \$at,\$a1,\$at
0+0338 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0338 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+033c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+033c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
...
0+0344 <[^>]*> addu \$at,\$a1,\$gp
0+0348 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0348 [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+034c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+034c [A-Z0-9_]*GPREL[A-Z0-9_]* .sbss.*
0+0350 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0350 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0354 <[^>]*> addu \$at,\$a1,\$at
0+0358 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0358 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+035c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+035c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0360 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0360 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0364 <[^>]*> addu \$at,\$a1,\$at
0+0368 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0368 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+036c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+036c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0370 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0370 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0374 <[^>]*> addu \$at,\$a1,\$at
0+0378 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0378 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+037c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+037c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0380 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0380 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0384 <[^>]*> addu \$at,\$a1,\$at
0+0388 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0388 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+038c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+038c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0390 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0390 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0394 <[^>]*> addu \$at,\$a1,\$at
0+0398 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0398 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+039c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+039c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+03a0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+03a0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+03a4 <[^>]*> addu \$at,\$a1,\$at
0+03a8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03a8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+03ac <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03ac [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+03b0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+03b0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+03b4 <[^>]*> addu \$at,\$a1,\$at
0+03b8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03b8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+03bc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03bc [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+03c0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+03c0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+03c4 <[^>]*> addu \$at,\$a1,\$at
0+03c8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03c8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+03cc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03cc [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+03d0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+03d0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+03d4 <[^>]*> addu \$at,\$a1,\$at
0+03d8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03d8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+03dc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03dc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+03e0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+03e0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+03e4 <[^>]*> addu \$at,\$a1,\$at
0+03e8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03e8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+03ec <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03ec [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+03f0 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+03f0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+03f4 <[^>]*> addu \$at,\$a1,\$at
0+03f8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03f8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+03fc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+03fc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0400 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0400 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0404 <[^>]*> addu \$at,\$a1,\$at
0+0408 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0408 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+040c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+040c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0410 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0410 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0414 <[^>]*> addu \$at,\$a1,\$at
0+0418 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0418 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+041c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+041c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0420 <[^>]*> lui \$at,0x0
[ 	]*RELOC: 0+0420 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0424 <[^>]*> addu \$at,\$a1,\$at
0+0428 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0428 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+042c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+042c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0430 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0430 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+0434 <[^>]*> addu \$at,\$a1,\$at
0+0438 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0438 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+043c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+043c [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+0440 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0440 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+0444 <[^>]*> addu \$at,\$a1,\$at
0+0448 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0448 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+044c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+044c [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+0450 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0450 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+0454 <[^>]*> addu \$at,\$a1,\$at
0+0458 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0458 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+045c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+045c [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+0460 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0460 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+0464 <[^>]*> addu \$at,\$a1,\$at
0+0468 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0468 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+046c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+046c [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+0470 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0470 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0474 <[^>]*> addu \$at,\$a1,\$at
0+0478 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0478 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+047c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+047c [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0480 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0480 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+0484 <[^>]*> addu \$at,\$a1,\$at
0+0488 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0488 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+048c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+048c [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0490 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0490 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0494 <[^>]*> addu \$at,\$a1,\$at
0+0498 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0498 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+049c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+049c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+04a0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04a0 [A-Z0-9_]*HI[A-Z0-9_]* .data.*
0+04a4 <[^>]*> addu \$at,\$a1,\$at
0+04a8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04a8 [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+04ac <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04ac [A-Z0-9_]*LO[A-Z0-9_]* .data.*
0+04b0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04b0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_data_label
0+04b4 <[^>]*> addu \$at,\$a1,\$at
0+04b8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04b8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+04bc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04bc [A-Z0-9_]*LO[A-Z0-9_]* big_external_data_label
0+04c0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04c0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_data_label
0+04c4 <[^>]*> addu \$at,\$a1,\$at
0+04c8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04c8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+04cc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04cc [A-Z0-9_]*LO[A-Z0-9_]* small_external_data_label
0+04d0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04d0 [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+04d4 <[^>]*> addu \$at,\$a1,\$at
0+04d8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04d8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+04dc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04dc [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+04e0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04e0 [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+04e4 <[^>]*> addu \$at,\$a1,\$at
0+04e8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04e8 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+04ec <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04ec [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+04f0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+04f0 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+04f4 <[^>]*> addu \$at,\$a1,\$at
0+04f8 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04f8 [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+04fc <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+04fc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+0500 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+0500 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+0504 <[^>]*> addu \$at,\$a1,\$at
0+0508 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0508 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+050c <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+050c [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
0+0510 <[^>]*> lwc1 \$f[45],0\(\$zero\)
0+0514 <[^>]*> lwc1 \$f[45],4\(\$zero\)
0+0518 <[^>]*> lwc1 \$f[45],1\(\$zero\)
0+051c <[^>]*> lwc1 \$f[45],5\(\$zero\)
0+0520 <[^>]*> lui \$at,0x1
0+0524 <[^>]*> lwc1 \$f[45],-32768\(\$at\)
0+0528 <[^>]*> lwc1 \$f[45],-32764\(\$at\)
0+052c <[^>]*> lwc1 \$f[45],-32768\(\$zero\)
0+0530 <[^>]*> lwc1 \$f[45],-32764\(\$zero\)
0+0534 <[^>]*> lwc1 \$f[45],0\(\$a1\)
0+0538 <[^>]*> lwc1 \$f[45],4\(\$a1\)
0+053c <[^>]*> lwc1 \$f[45],1\(\$a1\)
0+0540 <[^>]*> lwc1 \$f[45],5\(\$a1\)
0+0544 <[^>]*> lui \$at,0x1
0+0548 <[^>]*> addu \$at,\$a1,\$at
0+054c <[^>]*> lwc1 \$f[45],-32768\(\$at\)
0+0550 <[^>]*> lwc1 \$f[45],-32764\(\$at\)
0+0554 <[^>]*> lwc1 \$f[45],-32768\(\$a1\)
0+0558 <[^>]*> lwc1 \$f[45],-32764\(\$a1\)
0+055c <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+055c [A-Z0-9_]*HI[A-Z0-9_]* small_external_common
0+0560 <[^>]*> addu \$at,\$a1,\$at
0+0564 <[^>]*> lwc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0564 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
0+0568 <[^>]*> lwc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0568 [A-Z0-9_]*LO[A-Z0-9_]* small_external_common
...
0+0570 <[^>]*> swc1 \$f[45],0\(\$zero\)
0+0574 <[^>]*> swc1 \$f[45],4\(\$zero\)
0+0578 <[^>]*> swc1 \$f[45],1\(\$zero\)
0+057c <[^>]*> swc1 \$f[45],5\(\$zero\)
0+0580 <[^>]*> lui \$at,0x1
0+0584 <[^>]*> swc1 \$f[45],-32768\(\$at\)
0+0588 <[^>]*> swc1 \$f[45],-32764\(\$at\)
0+058c <[^>]*> swc1 \$f[45],-32768\(\$zero\)
0+0590 <[^>]*> swc1 \$f[45],-32764\(\$zero\)
0+0594 <[^>]*> swc1 \$f[45],0\(\$a1\)
0+0598 <[^>]*> swc1 \$f[45],4\(\$a1\)
0+059c <[^>]*> swc1 \$f[45],1\(\$a1\)
0+05a0 <[^>]*> swc1 \$f[45],5\(\$a1\)
0+05a4 <[^>]*> lui \$at,0x1
0+05a8 <[^>]*> addu \$at,\$a1,\$at
0+05ac <[^>]*> swc1 \$f[45],-32768\(\$at\)
0+05b0 <[^>]*> swc1 \$f[45],-32764\(\$at\)
0+05b4 <[^>]*> swc1 \$f[45],-32768\(\$a1\)
0+05b8 <[^>]*> swc1 \$f[45],-32764\(\$a1\)
0+05bc <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+05bc [A-Z0-9_]*HI[A-Z0-9_]* big_external_common
0+05c0 <[^>]*> addu \$at,\$a1,\$at
0+05c4 <[^>]*> swc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05c4 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+05c8 <[^>]*> swc1 \$f[45],[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05c8 [A-Z0-9_]*LO[A-Z0-9_]* big_external_common
0+05cc <[^>]*> sw \$a0,0\(\$zero\)
0+05d0 <[^>]*> sw \$a1,4\(\$zero\)
0+05d4 <[^>]*> lui \$a0,[-0-9x]+
[ 	]*RELOC: 0+05d4 [A-Z0-9_]*HI[A-Z0-9_]* .bss.*
0+05d8 <[^>]*> daddu \$a0,\$a0,\$a1
0+05dc <[^>]*> ld \$a0,[-0-9]+\(\$a0\)
[ 	]*RELOC: 0+05dc [A-Z0-9_]*LO[A-Z0-9_]* .bss.*
0+05e0 <[^>]*> lui \$at,[-0-9x]+
[ 	]*RELOC: 0+05e0 [A-Z0-9_]*HI[A-Z0-9_]* .sbss.*
0+05e4 <[^>]*> daddu \$at,\$at,\$a1
0+05e8 <[^>]*> sd \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+05e8 [A-Z0-9_]*LO[A-Z0-9_]* .sbss.*
...
