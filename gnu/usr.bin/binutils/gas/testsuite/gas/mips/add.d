#objdump: -dr
#name: MIPS add

# Test the add macro.

.*: +file format .*mips.*

No symbols in .*
Disassembly of section .text:
0+0000 addi \$a0,\$a0,0
0+0004 addi \$a0,\$a0,1
0+0008 li \$at,32768
0+000c add \$a0,\$a0,\$at
0+0010 addi \$a0,\$a0,-32768
0+0014 lui \$at,1
0+0018 add \$a0,\$a0,\$at
0+001c lui \$at,1
0+0020 ori \$at,\$at,42405
0+0024 add \$a0,\$a0,\$at
0+0028 addiu \$a0,\$a0,1
...
