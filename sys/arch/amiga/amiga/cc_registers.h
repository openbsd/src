/*	$NetBSD: cc_registers.h,v 1.3 1994/10/26 02:01:38 cgd Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ! defined (_CC_REGISTERS_H)
#define _CC_REGISTERS_H

#define R_BLTDDAT 0x000
#define R_DMACONR 0x002
#define R_VPOSR 0x004
#define R_VHPOSR 0x006
#define R_DSKDATR 0x008
#define R_JOY0DAT 0x00A
#define R_JOY1DAT 0x00C
#define R_CLXDAT 0x00E
#define R_ADKCONR 0x010
#define R_POT0DAT 0x012
#define R_POT1DAT 0x014
#define R_POTINP 0x016
#define R_SERDATR 0x018
#define R_DSKBYTR 0x01A
#define R_INTENAR 0x01C
#define R_INTREQR 0x01E
#define R_DSKPTH 0x020
#define R_DSKPTL 0x022
#define R_DSKLEN 0x024
#define R_DSKDAT 0x026
#define R_REFPTR 0x028
#define R_VPOSW 0x02A
#define R_VHPOSW 0x02C
#define R_COPCON 0x02E
#define R_SERDAT 0x030
#define R_SERPER 0x032
#define R_POTGO 0x034
#define R_JOYTEST 0x036
#define R_STREQU 0x038
#define R_STRVBL 0x03A
#define R_STRHOR 0x03C
#define R_STRLONG 0x03E
#define R_BLTCON0 0x040
#define R_BLTCON1 0x042
#define R_BLTAFWM 0x044
#define R_BLTALWM 0x046
#define R_BLTCPTH 0x048
#define R_BLTCPTL 0x04A
#define R_BLTBPTH 0x04C
#define R_BLTBPTL 0x04E
#define R_BLTAPTH 0x050
#define R_BLTAPTL 0x052
#define R_BLTDPTH 0x054
#define R_BLTDPTL 0x056
#define R_BLTSIZE 0x058
#define R_BLTCON0L 0x05B
#define R_BLTSIZV 0x05C
#define R_BLTSIZH 0x05E
#define R_BLTCMOD 0x060
#define R_BLTBMOD 0x062
#define R_BLTAMOD 0x064
#define R_BLTDMOD 0x066
#define R_BLTCDAT 0x070
#define R_BLTBDAT 0x072
#define R_BLTADAT 0x074
#define R_DENISEID 0x07C
#define R_DSKSYNC 0x07E
#define R_COP1LCH 0x080
#define R_COP1LCL 0x082
#define R_COP2LCH 0x084
#define R_COP2LCL 0x086
#define R_COPJMP1 0x088
#define R_COPJMP2 0x08A
#define R_COPINS 0x08C
#define R_DIWSTRT 0x08E
#define R_DIWSTART 0x08E
#define R_DIWSTOP 0x090
#define R_DIWSTOP 0x090
#define R_DDFSTRT 0x092
#define R_DDFSTART 0x092
#define R_DDFSTOP 0x094
#define R_DMACON 0x096
#define R_CLXCON 0x098
#define R_INTENA 0x09A
#define R_INTREQ 0x09C
#define R_ADKCON 0x09E
#define R_AUD0H 0x0A0
#define R_AUD0L 0X0A2
#define R_AC0_LEN 0x0A4
#define R_AC0_PER 0x0A6
#define R_AC0_VOL 0x0A8
#define R_AC0_DAT 0x0AA
#define R_AUD1H 0x0B0
#define R_AUD1L 0x0B2
#define R_AC1_LEN 0x0B4
#define R_AC1_PER 0x0B6
#define R_AC1_VOL 0x0B8
#define R_AC1_DAT 0x0BA
#define R_AUD2H 0x0C0
#define R_AUD2L 0x0C2
#define R_AC2_LEN 0x0C4
#define R_AC2_PER 0x0C6
#define R_AC2_VOL 0x0C8
#define R_AC2_DAT 0x0CA
#define R_AUD3H 0x0D0
#define R_AUD3L 0x0D2
#define R_AC3_LEN 0x0D4
#define R_AC3_PER 0x0D6
#define R_AC3_VOL 0x0D8
#define R_AC3_DAT 0x0DA
#define R_BPL0PTH 0x0E0
#define R_BPL0PTL 0x0E2
#define R_BPL1PTH 0x0E4
#define R_BPL1PTL 0x0E6
#define R_BPL2PTH 0x0E8
#define R_BPL2PTL 0x0EA
#define R_BPL3PTH 0x0EC
#define R_BPL3PTL 0x0EE
#define R_BPL4PTH 0x0F0
#define R_BPL4PTL 0x0F2
#define R_BPL5PTH 0x0F4
#define R_BPL5PTL 0x0F6
#define R_BPL6PTH 0x0F8
#define R_BPL6PTL 0x0FA
#define R_BPL7PTH 0x0FC
#define R_BPL7PTL 0x0FE
#define R_BPLCON0 0x100
#define R_BPLCON1 0x102
#define R_BPLCON2 0x104
#define R_BPLCON3 0x106
#define R_BPL1MOD 0x108
#define R_BPLMOD1 0x108
#define R_BPL2MOD 0x10A
#define R_BPLMOD2 0x10A
#define R_BPLCON4 0x10C
#define R_CLXCON2 0x10E
#define R_BPL0DAT 0x110
#define R_BPL1DAT 0x112
#define R_BPL2DAT 0x114
#define R_BPL3DAT 0x116
#define R_BPL4DAT 0x118
#define R_BPL5DAT 0x11A
#define R_BPL6DAT 0x11C
#define R_BPL7DAT 0x11E
#define R_SPR0PTH 0x120
#define R_SPR0PTL 0x122
#define R_SPR1PTH 0x124
#define R_SPR1PTL 0x126
#define R_SPR2PTH 0x128
#define R_SPR2PTL 0x12A
#define R_SPR3PTH 0x12C
#define R_SPR3PTL 0x12E
#define R_SPR4PTH 0x130
#define R_SPR4PTL 0x132
#define R_SPR5PTH 0x134
#define R_SPR5PTL 0x136
#define R_SPR6PTH 0x138
#define R_SPR6PTL 0x13A
#define R_SPR7PTH 0x13C
#define R_SPR7PTL 0x13E
#define R_SPR0_POS 0x140
#define R_SPR0_CTL 0x142
#define R_SPR0_DATAA 0x144
#define R_SPR0_DATAB 0x146
#define R_SPR1_POS 0x148
#define R_SPR1_CTL 0x14A
#define R_SPR1_DATAA 0x14C
#define R_SPR1_DATAB 0x14E
#define R_SPR2_POS 0x150
#define R_SPR2_CTL 0x152
#define R_SPR2_DATAA 0x154
#define R_SPR2_DATAB 0x156
#define R_SPR3_POS 0x158
#define R_SPR3_CTL 0x15A
#define R_SPR3_DATAA 0x15C
#define R_SPR3_DATAB 0x15E
#define R_SPR4_POS 0x160
#define R_SPR4_CTL 0x162
#define R_SPR4_DATAA 0x164
#define R_SPR4_DATAB 0x166
#define R_SPR5_POS 0x168
#define R_SPR5_CTL 0x16A
#define R_SPR5_DATAA 0x16C
#define R_SPR5_DATAB 0x16E
#define R_SPR6_POS 0x170
#define R_SPR6_CTL 0x172
#define R_SPR6_DATAA 0x174
#define R_SPR6_DATAB 0x176
#define R_SPR7_POS 0x178
#define R_SPR7_CTL 0x17A
#define R_SPR7_DATAA 0x17C
#define R_SPR7_DATAB 0x17E
#define R_COLOR00 0x180
#define R_COLOR01 0x182
#define R_COLOR02 0x184
#define R_COLOR03 0x186
#define R_COLOR04 0x188
#define R_COLOR05 0x18A
#define R_COLOR06 0x18C
#define R_COLOR07 0x18E
#define R_COLOR08 0x190
#define R_COLOR09 0x192
#define R_COLOR0A 0x194
#define R_COLOR0B 0x196
#define R_COLOR0C 0x198
#define R_COLOR0D 0x19A
#define R_COLOR0E 0x19C
#define R_COLOR0F 0x19E
#define R_COLOR10 0x1A0
#define R_COLOR11 0x1A2
#define R_COLOR12 0x1A4
#define R_COLOR13 0x1A6
#define R_COLOR14 0x1A8
#define R_COLOR15 0x1AA
#define R_COLOR16 0x1AC
#define R_COLOR17 0x1AE
#define R_COLOR18 0x1B0
#define R_COLOR19 0x1B2
#define R_COLOR1A 0x1B4
#define R_COLOR1B 0x1B6
#define R_COLOR1C 0x1B8
#define R_COLOR1D 0x1BA
#define R_COLOR1E 0x1BC
#define R_COLOR1F 0x1BE
#define R_HTOTAL 0x1C0
#define R_HSSTOP 0x1C2
#define R_HBSTRT 0x1C4
#define R_HBSTOP 0x1C6
#define R_VTOTAL 0x1C8
#define R_VSSTOP 0x1CA
#define R_VBSTRT 0x1CC
#define R_VBSTOP 0x1CE
#define R_SPRHSTRT 0x1D0
#define R_SPRHSTOP 0x1D2
#define R_BPLHSTRT 0x1D4
#define R_BPLHSTOP 0x1D6
#define R_HHPOSW 0x1D8
#define R_HHPOSR 0x1DA
#define R_BEAMCON0 0x1DC
#define R_HSSTRT 0x1DE
#define R_VSSTRT 0x1E0
#define R_HCENTER 0x1E2
#define R_DIWHIGH 0x1E4
#define R_FMODE 0x1FC

#endif /* _CC_REGISTERS_H */
