#objdump: -dr
#name: MIPS abs

# Test the abs macro.

.*: +file format .*mips.*

No symbols in .*
Disassembly of section .text:
0+0000 bgez \$a0,0+000c
...
0+0008 neg \$a0,\$a0
0+000c bgez \$a1,0+0018
0+0010 move \$a0,\$a1
0+0014 neg \$a0,\$a1
...
