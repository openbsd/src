#objdump: -d
#name: MRI structured for
#as: -M

# Test MRI structured for pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> clrw %d1
0+002 <foo\+2> movew #1,%d0
0+006 <foo\+6> cmpiw #10,%d0
0+00a <foo\+a> blts 0+016 <foo\+16>
0+00c <foo\+c> addw %d0,%d1
0+00e <foo\+e> bvcs 0+012 <foo\+12>
0+010 <foo\+10> bras 0+016 <foo\+16>
0+012 <foo\+12> addqw #2,%d0
0+014 <foo\+14> bras 0+006 <foo\+6>
0+016 <foo\+16> clrw %d1
0+018 <foo\+18> movew #10,%d0
0+01c <foo\+1c> cmpiw #1,%d0
0+020 <foo\+20> bgts 0+030 <foo\+30>
0+022 <foo\+22> cmpiw #100,%d1
0+026 <foo\+26> bgts 0+02a <foo\+2a>
0+028 <foo\+28> bras 0+02c <foo\+2c>
0+02a <foo\+2a> addw %d0,%d1
0+02c <foo\+2c> subqw #1,%d0
0+02e <foo\+2e> bras 0+01c <foo\+1c>
0+030 <foo\+30> nop
0+032 <foo\+32> nop
