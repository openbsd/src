#objdump: -d
#name: cas

# Test parsing of the operands of the cas instruction

.*: +file format .*

Disassembly of section .text:
0+000 <foo> casw %d0,%d1,%a0@
0+004 <foo\+4> casw %d0,%d1,%a0@
0+008 <foo\+8> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+00e <foo\+e> cas2w %d0,%d2,%d3,%d4,@\(%d0\),@\(%d1\)
0+014 <foo\+14> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+01a <foo\+1a> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+020 <foo\+20> cas2w %d0,%d2,%d3,%d4,@\(%d0\),@\(%d1\)
0+026 <foo\+26> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+02c <foo\+2c> cas2w %d0,%d2,%d3,%d4,@\(%d0\),@\(%d1\)
0+032 <foo\+32> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+038 <foo\+38> cas2w %d0,%d2,%d3,%d4,%a0@,%a1@
0+03e <foo\+3e> cas2w %d0,%d2,%d3,%d4,@\(%d0\),@\(%d1\)
