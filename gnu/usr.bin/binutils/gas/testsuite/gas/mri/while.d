#objdump: -d
#name: MRI structured while
#as: -M

# Test MRI structure while pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> bccs 0+004 <foo\+4>
0+002 <foo\+2> bras 0+000 <foo>
0+004 <foo\+4> clrw %d1
0+006 <foo\+6> cmpiw #10,%d1
0+00a <foo\+a> blts 0+010 <foo\+10>
0+00c <foo\+c> addqw #1,%d1
0+00e <foo\+e> bras 0+006 <foo\+6>
0+010 <foo\+10> nop
0+012 <foo\+12> nop
