#objdump: -dr
#name: MIPS jal-empic
#as: -mips1 -membedded-pic
#source: jal.s

# Test the jal macro with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> jalr \$t9
...
0+0008 <[^>]*> jalr \$a0,\$t9
...
0+0010 <[^>]*> bal 0+0000 <text_label>
[ 	]*RELOC: 0+0010 PCREL16 .text
...
0+0018 <[^>]*> bal 0+0018 <text_label\+18>
[ 	]*RELOC: 0+0018 PCREL16 external_text_label
...
0+0020 <[^>]*> b 0+0000 <text_label>
[ 	]*RELOC: 0+0020 PCREL16 .text
...
0+0028 <[^>]*> b 0+0028 <text_label\+28>
[ 	]*RELOC: 0+0028 PCREL16 external_text_label
...
