#objdump: -dr
#name: MIPS bgeu

# Test the bgeu macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> sltu \$at,\$a0,\$a1
0+0004 <[^>]*> beqz \$at,0+0000 <text_label>
...
0+000c <[^>]*> beq \$zero,\$a1,0+0000 <text_label>
...
0+0014 <[^>]*> bnez \$a0,0+0000 <text_label>
...
0+001c <[^>]*> sltiu \$at,\$a0,2
0+0020 <[^>]*> beqz \$at,0+0000 <text_label>
...
0+0028 <[^>]*> li \$at,32768
0+002c <[^>]*> sltu \$at,\$a0,\$at
0+0030 <[^>]*> beqz \$at,0+0000 <text_label>
...
0+0038 <[^>]*> sltiu \$at,\$a0,-32768
0+003c <[^>]*> beqz \$at,0+0000 <text_label>
...
0+0044 <[^>]*> lui \$at,1
0+0048 <[^>]*> sltu \$at,\$a0,\$at
0+004c <[^>]*> beqz \$at,0+0000 <text_label>
...
0+0054 <[^>]*> lui \$at,1
0+0058 <[^>]*> ori \$at,\$at,42405
0+005c <[^>]*> sltu \$at,\$a0,\$at
0+0060 <[^>]*> beqz \$at,0+0000 <text_label>
...
0+0068 <[^>]*> sltu \$at,\$a1,\$a0
0+006c <[^>]*> bnez \$at,0+0000 <text_label>
...
0+0074 <[^>]*> bnez \$a0,0+0000 <text_label>
...
0+007c <[^>]*> bnez \$a0,0+0000 <text_label>
...
0+0084 <[^>]*> sltu \$at,\$a0,\$a1
0+0088 <[^>]*> beqzl \$at,0+0000 <text_label>
...
0+0090 <[^>]*> sltu \$at,\$a1,\$a0
0+0094 <[^>]*> bnezl \$at,0+0000 <text_label>
...
