#objdump: -d --prefix-addresses
#as: -m68hc11
#name: all_insns

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+000 <L0> aba
0+001 <L1> abx
0+002 <L2> aby
0+004 <L3> adca	#103
0+006 <L4> adca	\*0+000 <L0>
0+008 <L5> adca	105,x
0+00a <L6> adca	0+000 <L0>
0+00d <L7> adca	81,x
0+00f <L8> adcb	#255
0+011 <L9> adcb	\*0+000 <L0>
0+013 <L10> adcb	236,x
0+015 <L11> adcb	0+000 <L0>
0+018 <L12> adcb	205,x
0+01a <L13> adda	#186
0+01c <L14> adda	\*0+000 <L0>
0+01e <L15> adda	242,x
0+020 <L16> adda	0+000 <L0>
0+023 <L17> adda	227,x
0+025 <L18> addb	#70
0+027 <L19> addb	\*0+000 <L0>
0+029 <L20> addb	194,x
0+02b <L21> addb	0+000 <L0>
0+02e <L22> addb	248,x
0+030 <L23> addd	#0000231b <L330\+0x2034>
0+033 <L24> addd	\*0+000 <L0>
0+035 <L25> addd	231,x
0+037 <L26> addd	0+000 <L0>
0+03a <L27> addd	118,x
0+03c <L28> anda	#90
0+03e <L29> anda	\*0+000 <L0>
0+040 <L30> anda	99,x
0+042 <L31> anda	0+000 <L0>
0+045 <L32> anda	159,x
0+047 <L33> andb	#201
0+049 <L34> andb	\*0+000 <L0>
0+04b <L35> andb	102,x
0+04d <L36> andb	0+000 <L0>
0+050 <L37> andb	13,x
0+052 <L38> asl	183,x
0+054 <L39> asl	0+000 <L0>
0+057 <L40> asl	88,x
0+059 <L41> asla
0+05a <L42> aslb
0+05b <L43> asld
0+05c <L44> asr	163,x
0+05e <L45> asr	0+000 <L0>
0+061 <L46> asr	37,x
0+063 <L47> asra
0+064 <L48> asrb
0+065 <L49> bcs	0+06a <L50>
0+067 <L49\+0x2> jmp	0+0f3 <L93>
0+06a <L50> bclr	\*0+000 <L0> #\$00
0+06d <L51> bclr	88,x #\$00
0+070 <L52> bclr	94,x #\$00
0+073 <L53> bcc	0+078 <L54>
0+075 <L53\+0x2> jmp	0+1a8 <L171>
0+078 <L54> bne	0+07d <L55>
0+07a <L54\+0x2> jmp	0+1b6 <L178>
0+07d <L55> blt	0+082 <L56>
0+07f <L55\+0x2> jmp	0+1f5 <L205>
0+082 <L56> ble	0+087 <L57>
0+084 <L56\+0x2> jmp	0+1e4 <L198>
0+087 <L57> bls	0+08c <L58>
0+089 <L57\+0x2> jmp	0+18a <L155>
0+08c <L58> bcs	0+091 <L59>
0+08e <L58\+0x2> jmp	0+1bb <L180>
0+091 <L59> bita	#84
0+093 <L60> bita	\*0+000 <L0>
0+095 <L61> bita	14,x
0+097 <L62> bita	0+000 <L0>
0+09a <L63> bita	116,x
0+09c <L64> bitb	#65
0+09e <L65> bitb	\*0+000 <L0>
0+0a0 <L66> bitb	61,x
0+0a2 <L67> bitb	0+000 <L0>
0+0a5 <L68> bitb	135,x
0+0a7 <L69> ble	0+11d <L112>
0+0a9 <L70> bcc	0+0ae <L71>
0+0ab <L70\+0x2> jmp	0+22e <L233>
0+0ae <L71> bls	0+097 <L62>
0+0b0 <L72> bge	0+0b5 <L73>
0+0b2 <L72\+0x2> jmp	0+197 <L161>
0+0b5 <L73> bmi	0+09e <L65>
0+0b7 <L74> beq	0+0bc <L75>
0+0b9 <L74\+0x2> jmp	0+220 <L225>
0+0bc <L75> bmi	0+0c1 <L76>
0+0be <L75\+0x2> jmp	0+24e <L252>
0+0c1 <L76> bra	0+106 <L103>
0+0c3 <L77> brclr	\*0+000 <L0> #\$00 0+145 <L125\+0x2>
0+0c7 <L78> brclr	151,x #\$00 0+127 <L115>
0+0cb <L79> brclr	107,x #\$00 0+0de <L84\+0x1>
0+0cf <L80> brn	0+082 <L56>
0+0d1 <L81> brset	\*0+000 <L0> #\$00 0+141 <L124>
0+0d5 <L82> brset	176,x #\$00 0+154 <L132>
0+0d9 <L83> brset	50,x #\$00 0+119 <L110\+0x2>
0+0dd <L84> bset	\*0+000 <L0> #\$00
0+0e0 <L85> bset	24,x #\$00
0+0e3 <L86> bset	92,x #\$00
0+0e6 <L87> jsr	0+037 <L26>
0+0e9 <L88> bvs	0+0ee <L89>
0+0eb <L88\+0x2> jmp	0+253 <L254>
0+0ee <L89> bvs	0+0a2 <L67>
0+0f0 <L90> cba
0+0f1 <L91> clc
0+0f2 <L92> cli
0+0f3 <L93> clr	251,x
0+0f5 <L94> clr	0+000 <L0>
0+0f8 <L95> clr	170,x
0+0fa <L96> clra
0+0fb <L97> clrb
0+0fc <L98> clv
0+0fd <L99> cmpa	#58
0+0ff <L100> cmpa	\*0+000 <L0>
0+101 <L101> cmpa	41,x
0+103 <L102> cmpa	0+000 <L0>
0+106 <L103> cmpa	230,x
0+108 <L104> cmpb	#5
0+10a <L105> cmpb	\*0+000 <L0>
0+10c <L106> cmpb	124,x
0+10e <L107> cmpb	0+000 <L0>
0+111 <L108> cmpb	117,x
0+113 <L109> cpd	#0+fd8 <L330\+0xcf1>
0+117 <L110> cpd	\*0+000 <L0>
0+11a <L111> cpd	97,x
0+11d <L112> cpd	0+000 <L0>
0+121 <L113> cpd	249,x
0+124 <L114> cpx	#0000af5c <L330\+0xac75>
0+127 <L115> cpx	\*0+000 <L0>
0+129 <L116> cpx	168,x
0+12b <L117> cpx	0+000 <L0>
0+12e <L118> cpx	15,x
0+130 <L119> cpy	#00004095 <L330\+0x3dae>
0+134 <L120> cpy	\*0+000 <L0>
0+137 <L121> cpy	235,x
0+13a <L122> cpy	0+000 <L0>
0+13e <L123> cpy	179,x
0+141 <L124> com	5,x
0+143 <L125> com	0+000 <L0>
0+146 <L126> com	247,x
0+148 <L127> coma
0+149 <L128> comb
0+14a <L129> cpd	#0000bf00 <L330\+0xbc19>
0+14e <L130> cpd	\*0+000 <L0>
0+151 <L131> cpd	161,x
0+154 <L132> cpd	0+000 <L0>
0+158 <L133> cpd	229,x
0+15b <L134> cpx	#00008fca <L330\+0x8ce3>
0+15e <L135> cpx	\*0+000 <L0>
0+160 <L136> cpx	203,x
0+162 <L137> cpx	0+000 <L0>
0+165 <L138> cpx	72,x
0+167 <L139> cpy	#0+247 <L248>
0+16b <L140> cpy	\*0+000 <L0>
0+16e <L141> cpy	189,x
0+171 <L142> cpy	0+000 <L0>
0+175 <L143> cpy	35,x
0+178 <L144> daa
0+179 <L145> dec	30,x
0+17b <L146> dec	0+000 <L0>
0+17e <L147> dec	28,x
0+180 <L148> deca
0+181 <L149> decb
0+182 <L150> des
0+183 <L151> dex
0+184 <L152> dey
0+186 <L153> eora	#123
0+188 <L154> eora	\*0+000 <L0>
0+18a <L155> eora	197,x
0+18c <L156> eora	0+000 <L0>
0+18f <L157> eora	115,x
0+191 <L158> eorb	#90
0+193 <L159> eorb	\*0+000 <L0>
0+195 <L160> eorb	94,x
0+197 <L161> eorb	0+000 <L0>
0+19a <L162> eorb	121,x
0+19c <L163> fdiv
0+19d <L164> idiv
0+19e <L165> inc	99,x
0+1a0 <L166> inc	0+000 <L0>
0+1a3 <L167> inc	112,x
0+1a5 <L168> inca
0+1a6 <L169> incb
0+1a7 <L170> ins
0+1a8 <L171> inx
0+1a9 <L172> iny
0+1ab <L173> jmp	100,x
0+1ad <L174> jmp	0+000 <L0>
0+1b0 <L175> jmp	17,x
0+1b2 <L176> jsr	\*0+000 <L0>
0+1b4 <L177> jsr	9,x
0+1b6 <L178> jsr	0+000 <L0>
0+1b9 <L179> jsr	170,x
0+1bb <L180> ldaa	#212
0+1bd <L181> ldaa	\*0+000 <L0>
0+1bf <L182> ldaa	242,x
0+1c1 <L183> ldaa	0+000 <L0>
0+1c4 <L184> ldaa	16,x
0+1c6 <L185> ldab	#175
0+1c8 <L186> ldab	\*0+000 <L0>
0+1ca <L187> ldab	51,x
0+1cc <L188> ldab	0+000 <L0>
0+1cf <L189> ldab	227,x
0+1d1 <L190> ldd	#0000c550 <L330\+0xc269>
0+1d4 <L191> ldd	\*0+000 <L0>
0+1d6 <L192> ldd	71,x
0+1d8 <L193> ldd	0+000 <L0>
0+1db <L194> ldd	92,x
0+1dd <L195> lds	#00004fbb <L330\+0x4cd4>
0+1e0 <L196> lds	\*0+000 <L0>
0+1e2 <L197> lds	34,x
0+1e4 <L198> lds	0+000 <L0>
0+1e7 <L199> lds	186,x
0+1e9 <L200> ldx	#0000579b <L330\+0x54b4>
0+1ec <L201> ldx	\*0+000 <L0>
0+1ee <L202> ldx	245,x
0+1f0 <L203> ldx	0+000 <L0>
0+1f3 <L204> ldx	225,x
0+1f5 <L205> ldy	#0000ac1a <L330\+0xa933>
0+1f9 <L206> ldy	\*0+000 <L0>
0+1fc <L207> ldy	127,x
0+1ff <L208> ldy	0+000 <L0>
0+203 <L209> ldy	248,x
0+206 <L210> asl	41,x
0+208 <L211> asl	0+000 <L0>
0+20b <L212> asl	164,x
0+20d <L213> asla
0+20e <L214> aslb
0+20f <L215> asld
0+210 <L216> lsr	27,x
0+212 <L217> lsr	0+000 <L0>
0+215 <L218> lsr	181,x
0+217 <L219> lsra
0+218 <L220> lsrb
0+219 <L221> lsrd
0+21a <L222> mul
0+21b <L223> neg	202,x
0+21d <L224> neg	0+000 <L0>
0+220 <L225> neg	232,x
0+222 <L226> nega
0+223 <L227> negb
0+224 <L228> nop
0+225 <L229> oraa	#152
0+227 <L230> oraa	\*0+000 <L0>
0+229 <L231> oraa	56,x
0+22b <L232> oraa	0+000 <L0>
0+22e <L233> oraa	121,x
0+230 <L234> orab	#77
0+232 <L235> orab	\*0+000 <L0>
0+234 <L236> orab	52,x
0+236 <L237> orab	0+000 <L0>
0+239 <L238> orab	95,x
0+23b <L239> psha
0+23c <L240> pshb
0+23d <L241> pshx
0+23e <L242> pshy
0+240 <L243> pula
0+241 <L244> pulb
0+242 <L245> pulx
0+243 <L246> puly
0+245 <L247> rol	78,x
0+247 <L248> rol	0+000 <L0>
0+24a <L249> rol	250,x
0+24c <L250> rola
0+24d <L251> rolb
0+24e <L252> ror	203,x
0+250 <L253> ror	0+000 <L0>
0+253 <L254> ror	5,x
0+255 <L255> rora
0+256 <L256> rorb
0+257 <L257> rti
0+258 <L258> rts
0+259 <L259> sba
0+25a <L260> sbca	#172
0+25c <L261> sbca	\*0+000 <L0>
0+25e <L262> sbca	33,x
0+260 <L263> sbca	0+000 <L0>
0+263 <L264> sbca	170,x
0+265 <L265> sbcb	#26
0+267 <L266> sbcb	\*0+000 <L0>
0+269 <L267> sbcb	162,x
0+26b <L268> sbcb	0+000 <L0>
0+26e <L269> sbcb	112,x
0+270 <L270> sec
0+271 <L271> sei
0+272 <L272> sev
0+273 <L273> staa	\*0+000 <L0>
0+275 <L274> staa	115,x
0+277 <L275> staa	0+000 <L0>
0+27a <L276> staa	4,x
0+27c <L277> stab	\*0+000 <L0>
0+27e <L278> stab	211,x
0+280 <L279> stab	0+000 <L0>
0+283 <L280> stab	148,x
0+285 <L281> std	\*0+000 <L0>
0+287 <L282> std	175,x
0+289 <L283> std	0+000 <L0>
0+28c <L284> std	240,x
0+28e <L285> stop
0+28f <L286> sts	\*0+000 <L0>
0+291 <L287> sts	158,x
0+293 <L288> sts	0+000 <L0>
0+296 <L289> sts	50,x
0+298 <L290> stx	\*0+000 <L0>
0+29a <L291> stx	73,x
0+29c <L292> stx	0+000 <L0>
0+29f <L293> stx	130,x
0+2a1 <L294> sty	\*0+000 <L0>
0+2a4 <L295> sty	169,x
0+2a7 <L296> sty	0+000 <L0>
0+2ab <L297> sty	112,x
0+2ae <L298> suba	#212
0+2b0 <L299> suba	\*0+000 <L0>
0+2b2 <L300> suba	138,x
0+2b4 <L301> suba	0+000 <L0>
0+2b7 <L302> suba	84,x
0+2b9 <L303> subb	#72
0+2bb <L304> subb	\*0+000 <L0>
0+2bd <L305> subb	10,x
0+2bf <L306> subb	0+000 <L0>
0+2c2 <L307> subb	213,x
0+2c4 <L308> subd	#0000f10e <L330\+0xee27>
0+2c7 <L309> subd	\*0+000 <L0>
0+2c9 <L310> subd	168,x
0+2cb <L311> subd	0+000 <L0>
0+2ce <L312> subd	172,x
0+2d0 <L313> swi
0+2d1 <L314> tab
0+2d2 <L315> tap
0+2d3 <L316> tba
	...
0+2d5 <L318> tpa
0+2d6 <L319> tst	91,x
0+2d8 <L320> tst	0+000 <L0>
0+2db <L321> tst	142,x
0+2dd <L322> tsta
0+2de <L323> tstb
0+2df <L324> tsx
0+2e0 <L325> tsy
0+2e2 <L326> txs
0+2e3 <L327> tys
0+2e5 <L328> wai
0+2e6 <L329> xgdx
0+2e7 <L330> xgdy
