.*: Assembler messages:
.*:10: Warning:.*
.*:12: Warning:.*
.*:19: Warning:.*
.*:22: Warning:.*
.*:95: Warning:.*
.*:96: Warning:.*
.*:97: Warning:.*
.*:98: Warning:.*
.*:99: Warning:.*
.*:100: Warning:.*
.*:101: Warning:.*
.*:102: Warning:.*
.*:103: Warning:.*
.*:104: Warning:.*
.*:105: Warning:.*
.*:106: Warning:.*
.*:107: Warning:.*
.*:108: Warning:.*
.*:109: Warning:.*
.*:110: Warning:.*
.*:111: Warning:.*
.*:112: Warning:.*
.*:113: Warning:.*
.*:114: Warning:.*
.*:115: Warning:.*
.*:149: Warning:.*
.*:176: Warning:.*
.*:178: Warning:.*
.*:180: Warning:.*
.*:182: Warning:.*
.*:184: Warning:.*
.*:190: Warning:.*
.*:192: Warning:.*
.*:194: Warning:.*
.*:196: Warning:.*
.*:198: Warning:.*
   1                       	.psize 0
   2                       	.text
   3                       	# test various segment reg insns
   4 0000 1E                		push	%ds
   5 0001 1E                		pushl	%ds
   6 0002 1F                		pop	%ds
   7 0003 1F                		popl	%ds
   8 0004 8CD8              		mov	%ds,%eax
   9 0006 8CD8              		movl	%ds,%eax
  10 0008 8CD8              		movl	%ds,%ax
.*Warning:.*
  11 000a 8ED8              		mov	%eax,%ds
  12 000c 8ED8              		movl	%ax,%ds
.*Warning:.*
  13 000e 8ED8              		movl	%eax,%ds
  14                       	
  15 0010 661E              		pushw	%ds
  16 0012 661F              		popw	%ds
  17 0014 668CD8            		mov	%ds,%ax
  18 0017 668CD8            		movw	%ds,%ax
  19 001a 668CD8            		movw	%ds,%eax
.*Warning:.*
  20 001d 8ED8              		mov	%ax,%ds
  21 001f 8ED8              		movw	%ax,%ds
  22 0021 8ED8              		movw	%eax,%ds
.*Warning:.*
  23                       	
  24                       	# test various pushes
  25 0023 6A0A              		pushl	\$10
  26 0025 666A0A            		pushw	\$10
  27 0028 6A0A              		push	\$10
  28 002a 68E80300 00       		pushl	\$1000
  29 002f 6668E803          		pushw	\$1000
  30 0033 68E80300 00       		push	\$1000
  31 0038 FF355700 0000     		pushl	1f
  32 003e 66FF3557 000000   		pushw	1f
  33 0045 FF355700 0000     		push	1f
  34 004b FFB30C00 0000     		push	\(1f-.\)\(%ebx\)
  35 0051 FF350600 0000     		push	1f-.
  36                       	# these, and others like them should have no operand size prefix
  37 0057 0F00D1            	1:	lldt	%cx
  38 005a 0F01F0            		lmsw	%ax
  39                       	
  40                       	# Just to make sure these don't become illegal due to over-enthusiastic
  41                       	# register checking
  42 005d 660FBEF8          		movsbw	%al,%di
  43 0061 0FBEC8            		movsbl	%al,%ecx
  44 0064 0FBFC8            		movswl	%ax,%ecx
  45 0067 660FB6F8          		movzbw	%al,%di
  46 006b 0FB6C8            		movzbl	%al,%ecx
  47 006e 0FB7C8            		movzwl	%ax,%ecx
  48                       	
  49 0071 EC                		in	%dx,%al
  50 0072 66ED              		in	%dx,%ax
  51 0074 ED                		in	%dx,%eax
  52 0075 EC                		in	\(%dx\),%al
  53 0076 66ED              		in	\(%dx\),%ax
  54 0078 ED                		in	\(%dx\),%eax
  55 0079 EC                		inb	%dx,%al
  56 007a 66ED              		inw	%dx,%ax
  57 007c ED                		inl	%dx,%eax
  58 007d EC                		inb	%dx
  59 007e 66ED              		inw	%dx
  60 0080 ED                		inl	%dx
  61 0081 E4FF              		inb	\$255
  62 0083 66E502            		inw	\$2
  63 0086 E504              		inl	\$4
  64 0088 66E50D            		in	\$13, %ax
  65 008b EE                		out	%al,%dx
  66 008c 66EF              		out	%ax,%dx
  67 008e EF                		out	%eax,%dx
  68 008f EE                		out	%al,\(%dx\)
  69 0090 66EF              		out	%ax,\(%dx\)
  70 0092 EF                		out	%eax,\(%dx\)
  71 0093 EE                		outb	%al,%dx
  72 0094 66EF              		outw	%ax,%dx
  73 0096 EF                		outl	%eax,%dx
  74 0097 EE                		outb	%dx
  75 0098 66EF              		outw	%dx
  76 009a EF                		outl	%dx
  77 009b E6FF              		outb	\$255
  78 009d 66E702            		outw	\$2
  79 00a0 E704              		outl	\$4
  80 00a2 66E70D            		out	%ax, \$13
  81                       	# These are used in AIX.
  82 00a5 66ED              		inw	\(%dx\)
  83 00a7 66EF              		outw	\(%dx\)
  84                       	
  85 00a9 A4                		movsb
  86 00aa 66A7              		cmpsw
  87 00ac AF                		scasl
  88 00ad D7                		xlatb
  89 00ae 2EA5              		movsl	%cs:\(%esi\),%es:\(%edi\)
  90 00b0 0F9303            		setae	\(%ebx\)
  91 00b3 0F9303            		setaeb	\(%ebx\)
  92 00b6 0F93C0            		setae	%al
  93                       	
  94                       	#these should give warnings
  95 00b9 0C01              		orb	\$1,%ax
.*Warning:.*
  96 00bb 0C01              		orb	\$1,%eax
.*Warning:.*
  97 00bd 80CB01            		orb	\$1,%bx
.*Warning:.*
  98 00c0 80CB01            		orb	\$1,%ebx
.*Warning:.*
  99 00c3 D9C1              		fldl	%st\(1\)
.*Warning:.*
 100 00c5 DDD2              		fstl	%st\(2\)
.*Warning:.*
 101 00c7 DDDB              		fstpl	%st\(3\)
.*Warning:.*
 102 00c9 D8D4              		fcoml	%st\(4\)
.*Warning:.*
 103 00cb D8DD              		fcompl	%st\(5\)
.*Warning:.*
 104 00cd DEC1              		faddp	%st\(1\),%st
.*Warning:.*
 105 00cf DECA              		fmulp	%st\(2\),%st
.*Warning:.*
 106 00d1 DEE3              		fsubp	%st\(3\),%st
.*Warning:.*
 107 00d3 DEEC              		fsubrp	%st\(4\),%st
.*Warning:.*
 108 00d5 DEF5              		fdivp	%st\(5\),%st
.*Warning:.*
 109 00d7 DEFE              		fdivrp	%st\(6\),%st
.*Warning:.*
 110 00d9 DEC1              		fadd
.*Warning:.*
 111 00db DEE1              		fsub
.*Warning:.*
 112 00dd DEC9              		fmul
.*Warning:.*
 113 00df DEF1              		fdiv
.*Warning:.*
 114 00e1 DEE9              		fsubr
.*Warning:.*
 115 00e3 DEF9              		fdivr
.*Warning:.*
 116                       	#these should all be legal
 117 00e5 0FA31556 341200   		btl	%edx, 0x123456
 118 00ec 0FA3D0            		btl	%edx, %eax
 119 00ef 0C01              		orb	\$1,%al
 120 00f1 80CB01            		orb	\$1,%bl
 121 00f4 A1110000 00       		movl	17,%eax
 122 00f9 A1110000 00       		mov	17,%eax
 123 00fe 66ED              		inw	%dx,%ax
 124 0100 ED                		inl	%dx,%eax
 125 0101 66ED              		inw	\(%dx\),%ax
 126 0103 ED                		inl	\(%dx\),%eax
 127 0104 EC                		in	\(%dx\),%al
 128 0105 66ED              		in	\(%dx\),%ax
 129 0107 ED                		in	\(%dx\),%eax
 130 0108 0FB61437          		movzbl	\(%edi,%esi\),%edx
 131 010c 0FB6451C          		movzbl	28\(%ebp\),%eax
 132 0110 0FB6C0            		movzbl	%al,%eax
 133 0113 0FB6F1            		movzbl	%cl,%esi
 134 0116 26D7              		xlat	%es:\(%ebx\)
 135 0118 D7                		xlat
 136 0119 D7                		xlatb
 137 011a DDD8              	1:	fstp	%st\(0\)
 138 011c E2FC              		loop	1b
 139 011e F6F1              		divb    %cl 
 140 0120 66F7F1            		divw    %cx 
 141 0123 F7F1              		divl    %ecx
 142 0125 F6F1              		div	%cl
 143 0127 66F7F1            		div	%cx
 144 012a F7F1              		div	%ecx
 145 012c F6F1              		div	%cl,%al
 146 012e 66F7F1            		div	%cx,%ax
 147 0131 F7F1              		div	%ecx,%eax
 148 0133 8EDE              		mov	%si,%ds
 149 0135 8EDE              		movl	%si,%ds		# warning here
.*Warning:.*
 150 0137 1E                		pushl	%ds
 151 0138 1E                		push	%ds
 152 0139 A0000000 00       		mov	0,%al
 153 013e 66A10000 0100     		mov	0x10000,%ax
 154 0144 89C3              		mov	%eax,%ebx
 155 0146 9C                		pushf
 156 0147 9C                		pushfl
 157 0148 669C              		pushfw
 158 014a 9D                		popf
 159 014b 9D                		popfl
 160 014c 669D              		popfw
 161 014e 89341D00 000000   		mov	%esi,\(,%ebx,1\)
 162 0155 80250000 00007F   		andb	\$~0x80,foo
 163                       	
 164                       	#check 16-bit code auto address prefix
 165                       	.code16gcc
 166 015c 67668D95 00FFFFFF 		leal	-256\(%ebp\),%edx
 167 0164 6788857F FFFFFF   		mov	%al,-129\(%ebp\)
 168 016b 67886580          		mov	%ah,-128\(%ebp\)
 169 016f 67668D9D 20F9FFFF 		leal	-1760\(%ebp\),%ebx
 170 0177 67668984 248C0000 		movl	%eax,140\(%esp\)
 170      00
 171                       	
 172                       	.code32
 173 0180 EB98              		jmp	1b
 174 0182 E9(FCFF|79FE)FF FF       		jmp	xxx
 175 0187 FF250000 0000     		jmp	\*xxx
 176 018d FF250000 0000     		jmp	xxx\(,1\)
.*Warning:.*
 177 0193 FFE7              		jmp	\*%edi
 178 0195 FFE7              		jmp	%edi
.*Warning:.*
 179 0197 FF27              		jmp	\*\(%edi\)
 180 0199 FF27              		jmp	\(%edi\)
.*Warning:.*
 181 019b FF2CBD00 000000   		ljmp	\*xxx\(,%edi,4\)
 182 01a2 FF2CBD00 000000   		ljmp	xxx\(,%edi,4\)
.*Warning:.*
 183 01a9 FF2D0000 0000     		ljmp	\*xxx
 184 01af FF2D0000 0000     		ljmp	xxx\(,1\)
.*Warning:.*
 185 01b5 EA000000 003412   		ljmp	\$0x1234,\$xxx
 186                       	
 187 01bc E859FFFF FF       		call	1b
 188 01c1 E8(FCFF|3AFE)FF FF       		call	xxx
 189 01c6 FF150000 0000     		call	\*xxx
 190 01cc FF150000 0000     		call	xxx\(,1\)
.*Warning:.*
 191 01d2 FFD7              		call	\*%edi
 192 01d4 FFD7              		call	%edi
.*Warning:.*
 193 01d6 FF17              		call	\*\(%edi\)
 194 01d8 FF17              		call	\(%edi\)
.*Warning:.*
 195 01da FF1CBD00 000000   		lcall	\*xxx\(,%edi,4\)
 196 01e1 FF1CBD00 000000   		lcall	xxx\(,%edi,4\)
.*Warning:.*
 197 01e8 FF1D0000 0000     		lcall	\*xxx
 198 01ee FF1D0000 0000     		lcall	xxx\(,1\)
.*Warning:.*
 199 01f4 9A000000 003412   		lcall	\$0x1234,\$xxx
 200                       	
 201                       		# Force a good alignment.
 202 01fb 00000000 00       		.p2align	4,0
