#as: -Asparclet
#objdump: -dr
#name: sparclet extensions

.*: +file format .*

Disassembly of section .text:
0+0000 <start> rd  %y, %l0
0+0004 <start\+4> rd  %asr1, %l0
0+0008 <start\+8> rd  %asr15, %l0
0+000c <start\+c> rd  %asr17, %l0
0+0010 <start\+10> rd  %asr18, %l0
0+0014 <start\+14> rd  %asr19, %l0
0+0018 <start\+18> rd  %asr20, %l0
0+001c <start\+1c> rd  %asr21, %l0
0+0020 <start\+20> rd  %asr22, %l0
0+0024 <start\+24> mov  %l0, %y
0+0028 <start\+28> mov  %l0, %asr1
0+002c <start\+2c> mov  %l0, %asr15
0+0030 <start\+30> mov  %l0, %asr17
0+0034 <start\+34> mov  %l0, %asr18
0+0038 <start\+38> mov  %l0, %asr19
0+003c <start\+3c> mov  %l0, %asr20
0+0040 <start\+40> mov  %l0, %asr21
0+0044 <start\+44> mov  %l0, %asr22
0+0048 <test_umul> umul  %g1, %g2, %g3
0+004c <test_umul\+4> umul  %g1, %g2, %g3
0+0050 <test_smul> smul  %g1, %g2, %g3
0+0054 <test_smul\+4> smul  %g1, %g2, %g3
0+0058 <test_stbar> stbar 
0+005c <test_stbar\+4> stbar 
0+0060 <test_stbar\+8> unimp  0x1
0+0064 <test_stbar\+c> flush  %l1
0+0068 <test_scan> scan  %l1, -1, %l3
0+006c <test_scan\+4> scan  %l1, 0, %l3
0+0070 <test_scan\+8> scan  %l1, %l1, %l3
0+0074 <test_shuffle> shuffle  %l0, 1, %l1
0+0078 <test_shuffle\+4> shuffle  %l0, 2, %l1
0+007c <test_shuffle\+8> shuffle  %l0, 4, %l1
0+0080 <test_shuffle\+c> shuffle  %l0, 8, %l1
0+0084 <test_shuffle\+10> shuffle  %l0, 0x10, %l1
0+0088 <test_shuffle\+14> shuffle  %l0, 0x18, %l1
0+008c <test_umac> umac  %l1, %l2, %l0
0+0090 <test_umac\+4> umac  %l1, 2, %l0
0+0094 <test_umac\+8> umac  %l1, 2, %l0
0+0098 <test_umacd> umacd  %l2, %l4, %l0
0+009c <test_umacd\+4> umacd  %l2, 3, %l0
0+00a0 <test_umacd\+8> umacd  %l2, 3, %l0
0+00a4 <test_smac> smac  %l1, %l2, %l0
0+00a8 <test_smac\+4> smac  %l1, -42, %l0
0+00ac <test_smac\+8> smac  %l1, -42, %l0
0+00b0 <test_smacd> smacd  %l2, %l4, %l0
0+00b4 <test_smacd\+4> smacd  %l2, 0x7b, %l0
0+00b8 <test_smacd\+8> smacd  %l2, 0x7b, %l0
0+00bc <test_umuld> umuld  %o2, %o4, %o0
0+00c0 <test_umuld\+4> umuld  %o2, 0x234, %o0
0+00c4 <test_umuld\+8> umuld  %o2, 0x567, %o0
0+00c8 <test_smuld> smuld  %i2, %i4, %i0
0+00cc <test_smuld\+4> smuld  %i2, -4096, %i0
0+00d0 <test_smuld\+8> smuld  %i4, 0xfff, %i0
0+00d4 <test_coprocessor> cpush  %l0, %l1
0+00d8 <test_coprocessor\+4> cpush  %l0, 1
0+00dc <test_coprocessor\+8> cpusha  %l0, %l1
0+00e0 <test_coprocessor\+c> cpush  %l0, 1
0+00e4 <test_coprocessor\+10> cpull  %l0
0+00e8 <test_coprocessor\+14> crdcxt  %ccsr, %l0
0+00ec <test_coprocessor\+18> crdcxt  %ccfr, %l0
0+00f0 <test_coprocessor\+1c> crdcxt  %ccpr, %l0
0+00f4 <test_coprocessor\+20> crdcxt  %cccrcr, %l0
0+00f8 <test_coprocessor\+24> cwrcxt  %l0, %ccsr
0+00fc <test_coprocessor\+28> cwrcxt  %l0, %ccfr
0+0100 <test_coprocessor\+2c> cwrcxt  %l0, %ccpr
0+0104 <test_coprocessor\+30> cwrcxt  %l0, %cccrcr
0+0108 <test_coprocessor\+34> cbn  0000010c <test_coprocessor\+38>
.*RELOC: 0+0108 WDISP22 stop\+0xfffffef8
0+010c <test_coprocessor\+38> nop 
0+0110 <test_coprocessor\+3c> cbn,a   00000114 <test_coprocessor\+40>
.*RELOC: 0+0110 WDISP22 stop\+0xfffffef0
0+0114 <test_coprocessor\+40> nop 
0+0118 <test_coprocessor\+44> cbe  0000011c <test_coprocessor\+48>
.*RELOC: 0+0118 WDISP22 stop\+0xfffffee8
0+011c <test_coprocessor\+48> nop 
0+0120 <test_coprocessor\+4c> cbe,a   00000124 <test_coprocessor\+50>
.*RELOC: 0+0120 WDISP22 stop\+0xfffffee0
0+0124 <test_coprocessor\+50> nop 
0+0128 <test_coprocessor\+54> cbf  0000012c <test_coprocessor\+58>
.*RELOC: 0+0128 WDISP22 stop\+0xfffffed8
0+012c <test_coprocessor\+58> nop 
0+0130 <test_coprocessor\+5c> cbf,a   00000134 <test_coprocessor\+60>
.*RELOC: 0+0130 WDISP22 stop\+0xfffffed0
0+0134 <test_coprocessor\+60> nop 
0+0138 <test_coprocessor\+64> cbef  0000013c <test_coprocessor\+68>
.*RELOC: 0+0138 WDISP22 stop\+0xfffffec8
0+013c <test_coprocessor\+68> nop 
0+0140 <test_coprocessor\+6c> cbef,a   00000144 <test_coprocessor\+70>
.*RELOC: 0+0140 WDISP22 stop\+0xfffffec0
0+0144 <test_coprocessor\+70> nop 
0+0148 <test_coprocessor\+74> cbr  0000014c <test_coprocessor\+78>
.*RELOC: 0+0148 WDISP22 stop\+0xfffffeb8
0+014c <test_coprocessor\+78> nop 
0+0150 <test_coprocessor\+7c> cbr,a   00000154 <test_coprocessor\+80>
.*RELOC: 0+0150 WDISP22 stop\+0xfffffeb0
0+0154 <test_coprocessor\+80> nop 
0+0158 <test_coprocessor\+84> cber  0000015c <test_coprocessor\+88>
.*RELOC: 0+0158 WDISP22 stop\+0xfffffea8
0+015c <test_coprocessor\+88> nop 
0+0160 <test_coprocessor\+8c> cber,a   00000164 <test_coprocessor\+90>
.*RELOC: 0+0160 WDISP22 stop\+0xfffffea0
0+0164 <test_coprocessor\+90> nop 
0+0168 <test_coprocessor\+94> cbfr  0000016c <test_coprocessor\+98>
.*RELOC: 0+0168 WDISP22 stop\+0xfffffe98
0+016c <test_coprocessor\+98> nop 
0+0170 <test_coprocessor\+9c> cbfr,a   00000174 <test_coprocessor\+a0>
.*RELOC: 0+0170 WDISP22 stop\+0xfffffe90
0+0174 <test_coprocessor\+a0> nop 
0+0178 <test_coprocessor\+a4> cbefr  0000017c <test_coprocessor\+a8>
.*RELOC: 0+0178 WDISP22 stop\+0xfffffe88
0+017c <test_coprocessor\+a8> nop 
0+0180 <test_coprocessor\+ac> cbefr,a   00000184 <test_coprocessor\+b0>
.*RELOC: 0+0180 WDISP22 stop\+0xfffffe80
0+0184 <test_coprocessor\+b0> nop 
0+0188 <test_coprocessor\+b4> cba  0000018c <test_coprocessor\+b8>
.*RELOC: 0+0188 WDISP22 stop\+0xfffffe78
0+018c <test_coprocessor\+b8> nop 
0+0190 <test_coprocessor\+bc> cba,a   00000194 <test_coprocessor\+c0>
.*RELOC: 0+0190 WDISP22 stop\+0xfffffe70
0+0194 <test_coprocessor\+c0> nop 
0+0198 <test_coprocessor\+c4> cbne  0000019c <test_coprocessor\+c8>
.*RELOC: 0+0198 WDISP22 stop\+0xfffffe68
0+019c <test_coprocessor\+c8> nop 
0+01a0 <test_coprocessor\+cc> cbne,a   000001a4 <test_coprocessor\+d0>
.*RELOC: 0+01a0 WDISP22 stop\+0xfffffe60
0+01a4 <test_coprocessor\+d0> nop 
0+01a8 <test_coprocessor\+d4> cbnf  000001ac <test_coprocessor\+d8>
.*RELOC: 0+01a8 WDISP22 stop\+0xfffffe58
0+01ac <test_coprocessor\+d8> nop 
0+01b0 <test_coprocessor\+dc> cbnf,a   000001b4 <test_coprocessor\+e0>
.*RELOC: 0+01b0 WDISP22 stop\+0xfffffe50
0+01b4 <test_coprocessor\+e0> nop 
0+01b8 <test_coprocessor\+e4> cbnef  000001bc <test_coprocessor\+e8>
.*RELOC: 0+01b8 WDISP22 stop\+0xfffffe48
0+01bc <test_coprocessor\+e8> nop 
0+01c0 <test_coprocessor\+ec> cbnef,a   000001c4 <test_coprocessor\+f0>
.*RELOC: 0+01c0 WDISP22 stop\+0xfffffe40
0+01c4 <test_coprocessor\+f0> nop 
0+01c8 <test_coprocessor\+f4> cbnr  000001cc <test_coprocessor\+f8>
.*RELOC: 0+01c8 WDISP22 stop\+0xfffffe38
0+01cc <test_coprocessor\+f8> nop 
0+01d0 <test_coprocessor\+fc> cbnr,a   000001d4 <test_coprocessor\+100>
.*RELOC: 0+01d0 WDISP22 stop\+0xfffffe30
0+01d4 <test_coprocessor\+100> nop 
0+01d8 <test_coprocessor\+104> cbner  000001dc <test_coprocessor\+108>
.*RELOC: 0+01d8 WDISP22 stop\+0xfffffe28
0+01dc <test_coprocessor\+108> nop 
0+01e0 <test_coprocessor\+10c> cbner,a   000001e4 <test_coprocessor\+110>
.*RELOC: 0+01e0 WDISP22 stop\+0xfffffe20
0+01e4 <test_coprocessor\+110> nop 
0+01e8 <test_coprocessor\+114> cbnfr  000001ec <test_coprocessor\+118>
.*RELOC: 0+01e8 WDISP22 stop\+0xfffffe18
0+01ec <test_coprocessor\+118> nop 
0+01f0 <test_coprocessor\+11c> cbnfr,a   000001f4 <test_coprocessor\+120>
.*RELOC: 0+01f0 WDISP22 stop\+0xfffffe10
0+01f4 <test_coprocessor\+120> nop 
0+01f8 <test_coprocessor\+124> cbnefr  000001fc <test_coprocessor\+128>
.*RELOC: 0+01f8 WDISP22 stop\+0xfffffe08
0+01fc <test_coprocessor\+128> nop 
0+0200 <test_coprocessor\+12c> cbnefr,a   00000204 <test_coprocessor\+130>
.*RELOC: 0+0200 WDISP22 stop\+0xfffffe00
0+0204 <test_coprocessor\+130> nop 
