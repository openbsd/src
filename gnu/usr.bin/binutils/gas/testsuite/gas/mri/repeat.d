#objdump: -d
#name: MRI structured repeat
#as: -M

# Test MRI structured repeat pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> bccs 0+000 <foo>
0+002 <foo\+2> clrw %d1
0+004 <foo\+4> addqw #1,%d1
0+006 <foo\+6> cmpiw #10,%d1
0+00a <foo\+a> bgts 0+004 <foo\+4>
0+00c <foo\+c> nop
0+00e <foo\+e> nop
