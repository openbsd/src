#objdump: -dr
#name: MIPS beq

# Test the beq macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> beq \$a0,\$a1,0+0000 <text_label>
...
0+0008 <[^>]*> beqz \$a0,0+0000 <text_label>
...
0+0010 <[^>]*> li \$at,1
0+0014 <[^>]*> beq \$a0,\$at,0+0000 <text_label>
...
0+001c <[^>]*> li \$at,32768
0+0020 <[^>]*> beq \$a0,\$at,0+0000 <text_label>
...
0+0028 <[^>]*> li \$at,-32768
0+002c <[^>]*> beq \$a0,\$at,0+0000 <text_label>
...
0+0034 <[^>]*> lui \$at,1
0+0038 <[^>]*> beq \$a0,\$at,0+0000 <text_label>
...
0+0040 <[^>]*> lui \$at,1
0+0044 <[^>]*> ori \$at,\$at,42405
0+0048 <[^>]*> beq \$a0,\$at,0+0000 <text_label>
...
0+0050 <[^>]*> bnez \$a0,0+0000 <text_label>
...
0+0058 <[^>]*> beqzl \$a0,0+0000 <text_label>
...
0+0060 <[^>]*> bnezl \$a0,0+0000 <text_label>
...
0+20068 <[^>]*> j 0+0000 <text_label>
[ 	]*RELOC: 0+20068 (MIPS_JMP|JMPADDR|R_MIPS_26) .text
...
0+20070 <[^>]*> jal 0+0000 <text_label>
[ 	]*RELOC: 0+20070 (MIPS_JMP|JMPADDR|R_MIPS_26) .text
...
