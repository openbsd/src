#objdump: -dr
#name: MIPS lifloat
#as: -mips1

# Test the li.d and li.s macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lui \$at,0
[ 	]*RELOC: 0+0000 [A-Z0-9_]*HI[A-Z0-9_]* .ro?data.*
0+0004 <[^>]*> lw \$a0,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0004 [A-Z0-9_]*LO[A-Z0-9_]* .ro?data.*
0+0008 <[^>]*> lw \$a1,[-0-9]+\(\$at\)
[ 	]*RELOC: 0+0008 [A-Z0-9_]*LO[A-Z0-9_]* .ro?data.*
0+000c <[^>]*> lwc1 \$f[45],[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+000c [A-Z0-9_]*LITERAL[A-Z0-9_]* .lit8.*
0+0010 <[^>]*> lwc1 \$f[45],[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+0010 [A-Z0-9_]*LITERAL[A-Z0-9_]* .lit8.*
0+0014 <[^>]*> lui \$a0,16256
0+0018 <[^>]*> lwc1 \$f4,[-0-9]+\(\$gp\)
[ 	]*RELOC: 0+0018 [A-Z0-9_]*LITERAL[A-Z0-9_]* .lit4.*
...
