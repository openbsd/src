#objdump: -dr
#name: MIPS jal-xgot
#as: -mips1 -KPIC -xgot
#source: jal-svr4pic.s

# Test the jal macro with -KPIC -xgot.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lui \$gp,0x0
[ 	]*RELOC: 0+0000 R_MIPS_HI16 _gp_disp
0+0004 <[^>]*> addiu \$gp,\$gp,0
[ 	]*RELOC: 0+0004 R_MIPS_LO16 _gp_disp
0+0008 <[^>]*> addu \$gp,\$gp,\$t9
0+000c <[^>]*> sw \$gp,0\(\$sp\)
0+0010 <[^>]*> jalr \$t9
...
0+0018 <[^>]*> lw \$gp,0\(\$sp\)
0+001c <[^>]*> jalr \$a0,\$t9
...
0+0024 <[^>]*> lw \$gp,0\(\$sp\)
...
0+002c <[^>]*> lw \$t9,0\(\$gp\)
[ 	]*RELOC: 0+002c R_MIPS_GOT16 .text
...
0+0034 <[^>]*> addiu \$t9,\$t9,0
[ 	]*RELOC: 0+0034 R_MIPS_LO16 .text
0+0038 <[^>]*> jalr \$t9
...
0+0040 <[^>]*> lw \$gp,0\(\$sp\)
0+0044 <[^>]*> lui \$t9,0x0
[ 	]*RELOC: 0+0044 R_MIPS_CALL_HI16 external_text_label
0+0048 <[^>]*> addu \$t9,\$t9,\$gp
0+004c <[^>]*> lw \$t9,0\(\$t9\)
[ 	]*RELOC: 0+004c R_MIPS_CALL_LO16 external_text_label
...
0+0054 <[^>]*> jalr \$t9
...
0+005c <[^>]*> lw \$gp,0\(\$sp\)
0+0060 <[^>]*> b 0+0000 <text_label>
...
