#objdump: -dr
#name: MIPS lifloat-svr4pic
#as: -mips1 -KPIC
#source: lifloat.s

# Test the li.d and li.s macros with -KPIC.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0000 R_MIPS_GOT16 .rodata
...
0+0008 <[^>]*> lw \$a0,0\(\$at\)
[ 	]*RELOC: 0+0008 R_MIPS_LO16 .rodata
0+000c <[^>]*> lw \$a1,4\(\$at\)
[ 	]*RELOC: 0+000c R_MIPS_LO16 .rodata
0+0010 <[^>]*> lw \$at,0\(\$gp\)
[ 	]*RELOC: 0+0010 R_MIPS_GOT16 .rodata
...
0+0018 <[^>]*> lwc1 \$f5,8\(\$at\)
[ 	]*RELOC: 0+0018 R_MIPS_LO16 .rodata
0+001c <[^>]*> lwc1 \$f4,12\(\$at\)
[ 	]*RELOC: 0+001c R_MIPS_LO16 .rodata
0+0020 <[^>]*> lui \$a0,16256
0+0024 <[^>]*> lui \$at,16256
0+0028 <[^>]*> mtc1 \$at,\$f4
...
