#objdump: -dr
#name: MIPS rol

# Test the rol and ror macros.

.*: +file format .*mips.*

No symbols in .*
Disassembly of section .text:
0+0000 negu \$at,\$a1
0+0004 srlv \$at,\$a0,\$at
0+0008 sllv \$a0,\$a0,\$a1
0+000c or \$a0,\$a0,\$at
0+0010 negu \$at,\$a2
0+0014 srlv \$at,\$a1,\$at
0+0018 sllv \$a0,\$a1,\$a2
0+001c or \$a0,\$a0,\$at
0+0020 sll \$at,\$a0,0x1
0+0024 srl \$a0,\$a0,0x1f
0+0028 or \$a0,\$a0,\$at
0+002c sll \$at,\$a1,0x1
0+0030 srl \$a0,\$a1,0x1f
0+0034 or \$a0,\$a0,\$at
0+0038 negu \$at,\$a1
0+003c sllv \$at,\$a0,\$at
0+0040 srlv \$a0,\$a0,\$a1
0+0044 or \$a0,\$a0,\$at
0+0048 negu \$at,\$a2
0+004c sllv \$at,\$a1,\$at
0+0050 srlv \$a0,\$a1,\$a2
0+0054 or \$a0,\$a0,\$at
0+0058 srl \$at,\$a0,0x1
0+005c sll \$a0,\$a0,0x1f
0+0060 or \$a0,\$a0,\$at
0+0064 srl \$at,\$a1,0x1
0+0068 sll \$a0,\$a1,0x1f
0+006c or \$a0,\$a0,\$at
