#objdump: -dr
#name: MIPS lifloat-empic
#as: -mips1 -membedded-pic
#source: lifloat.s

# Test the li.d and li.s macros with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> addiu \$at,\$gp,-16384
[ 	]*RELOC: 0+0000 [A-Z0-9_]*GPREL[A-Z0-9_]* .rdata.*
0+0004 <[^>]*> lw \$a0,0\(\$at\)
0+0008 <[^>]*> lw \$a1,4\(\$at\)
0+000c <[^>]*> lwc1 \$f[45],-16368\(\$gp\)
[ 	]*RELOC: 0+000c [A-Z0-9_]*LITERAL[A-Z0-9_]* .lit8.*
0+0010 <[^>]*> lwc1 \$f[45],-16364\(\$gp\)
[ 	]*RELOC: 0+0010 [A-Z0-9_]*LITERAL[A-Z0-9_]* .lit8.*
0+0014 <[^>]*> lui \$a0,16256
0+0018 <[^>]*> lui \$at,16256
0+001c <[^>]*> mtc1 \$at,\$f4
