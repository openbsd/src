#objdump: -dr
#name: MIPS mul

# Test the mul macro.

.*: +file format .*mips.*

No symbols in .*
Disassembly of section .text:
0+0000 multu \$a0,\$a1
0+0004 mflo \$a0
...
0+0010 multu \$a1,\$a2
0+0014 mflo \$a0
0+0018 li \$at,0
...
0+0020 mult \$a1,\$at
0+0024 mflo \$a0
0+0028 li \$at,1
...
0+0030 mult \$a1,\$at
0+0034 mflo \$a0
0+0038 li \$at,32768
...
0+0040 mult \$a1,\$at
0+0044 mflo \$a0
0+0048 li \$at,-32768
...
0+0050 mult \$a1,\$at
0+0054 mflo \$a0
0+0058 lui \$at,1
...
0+0060 mult \$a1,\$at
0+0064 mflo \$a0
0+0068 lui \$at,1
0+006c ori \$at,\$at,42405
0+0070 mult \$a1,\$at
0+0074 mflo \$a0
...
0+0080 mult \$a0,\$a1
0+0084 mflo \$a0
0+0088 sra \$a0,\$a0,0x1f
0+008c mfhi \$at
0+0090 beq \$a0,\$at,0+9c
...
0+0098 break 0x6
0+009c mflo \$a0
...
0+00a8 mult \$a1,\$a2
0+00ac mflo \$a0
0+00b0 sra \$a0,\$a0,0x1f
0+00b4 mfhi \$at
0+00b8 beq \$a0,\$at,0+c4
...
0+00c0 break 0x6
0+00c4 mflo \$a0
...
0+00d0 multu \$a0,\$a1
0+00d4 mfhi \$at
0+00d8 mflo \$a0
0+00dc beqz \$at,0+e8
...
0+00e4 break 0x6
0+00e8 multu \$a1,\$a2
0+00ec mfhi \$at
0+00f0 mflo \$a0
0+00f4 beqz \$at,0+100
...
0+00fc break 0x6
0+0100 dmultu \$a1,\$a2
0+0104 mflo \$a0
0+0108 li \$at,1
...
0+0110 dmult \$a1,\$at
0+0114 mflo \$a0
...
0+0120 dmult \$a1,\$a2
0+0124 mflo \$a0
0+0128 dsra32 \$a0,\$a0,0x1f
0+012c mfhi \$at
0+0130 beq \$a0,\$at,0+13c
...
0+0138 break 0x6
0+013c mflo \$a0
...
0+0148 dmultu \$a1,\$a2
0+014c mfhi \$at
0+0150 mflo \$a0
0+0154 beqz \$at,0+160
...
0+015c break 0x6
