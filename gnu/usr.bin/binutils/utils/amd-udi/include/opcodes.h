/* @(#)opcodes.h	5.19 93/08/10 17:49:11, Srini,AMD */
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 ** 
 **       This file gives the definitions of opcodes in the Am29000
 **       processor.
 **
 **       This file defines the opcodes used in the Am29000 processor.
 **       The opcodes here are defined as the first eight-bit field in
 **       the (32-bit) instruction.  Note that many instructions are
 **       defined with a "mode bit" in the least significant bit of the
 **       opcode field.  In this definition, instructions with
 **       different mode bits are treated as different instructions.
 **       This allows consistent processing of eight bit opcodes.
 **
 **       For instance, AND with the mode bit set to zero (M=0) defines
 **       the third eight bit field in the instruction word as being
 **       the content of a register.  When M=1 the content of the third
 **       field is taken to be an immediate value, zero-extended to 32
 **       bits.
 **
 **       The #define statements below will treat AND as two different
 **       instructions called AND0 and AND1.  As you might suspect,
 **       AND0 is describes the opcode AND when M=0 and AND1 describes
 **       the opcode AND where M=1.
 **
 **       Note1: Wherever the name of an opcode as listed in the User's
 **              Manual confilcts with either a reserved keyword, or a
 **              previously defined opcode, "_op" is appended to the
 **              name. 
 **
 **       Note2: These opcodes are sorted in numerical order.
 **
 **       Note3: Opcodes are broken up into groups of 16 (16#n0# to
 **              16#nF#).
 **
 **       Note4: When no opcode is defined, a variable of the name
 **              illegal_nn is declared in that space.  The value of
 **              "nn" is the number of the opcode in hexadecimal.
 **
 *****************************************************************************
 */
 
#ifndef	_OPCODES_H_INCLUDED_
#define	_OPCODES_H_INCLUDED_

#define   ILLEGAL_00   0x00
#define   CONSTN       0x01
#define   CONSTH       0x02
#define   CONST        0x03
#define   MTSRIM       0x04
#define   CONSTHZ      0x05
#define   LOADL0       0x06
#define   LOADL1       0x07
#define   CLZ0         0x08
#define   CLZ1         0x09
#define   EXBYTE0      0x0A
#define   EXBYTE1      0x0B
#define   INBYTE0      0x0C
#define   INBYTE1      0x0D
#define   STOREL0      0x0E
#define   STOREL1      0x0F

#define   ADDS0        0x10
#define   ADDS1        0x11
#define   ADDU0        0x12
#define   ADDU1        0x13
#define   ADD0         0x14
#define   ADD1         0x15
#define   LOAD0        0x16
#define   LOAD1        0x17
#define   ADDCS0       0x18
#define   ADDCS1       0x19
#define   ADDCU0       0x1A
#define   ADDCU1       0x1B
#define   ADDC0        0x1C
#define   ADDC1        0x1D
#define   STORE0       0x1E
#define   STORE1       0x1F

#define   SUBS0        0x20
#define   SUBS1        0x21
#define   SUBU0        0x22
#define   SUBU1        0x23
#define   SUB0         0x24
#define   SUB1         0x25
#define   LOADSET0     0x26
#define   LOADSET1     0x27
#define   SUBCS0       0x28
#define   SUBCS1       0x29
#define   SUBCU0       0x2A
#define   SUBCU1       0x2B
#define   SUBC0        0x2C
#define   SUBC1        0x2D
#define   CPBYTE0      0x2E
#define   CPBYTE1      0x2F

#define   SUBRS0       0x30
#define   SUBRS1       0x31
#define   SUBRU0       0x32
#define   SUBRU1       0x33
#define   SUBR0        0x34
#define   SUBR1        0x35
#define   LOADM0       0x36
#define   LOADM1       0x37
#define   SUBRCS0      0x38
#define   SUBRCS1      0x39
#define   SUBRCU0      0x3A
#define   SUBRCU1      0x3B
#define   SUBRC0       0x3C
#define   SUBRC1       0x3D
#define   STOREM0      0x3E
#define   STOREM1      0x3F

#define   CPLT0        0x40
#define   CPLT1        0x41
#define   CPLTU0       0x42
#define   CPLTU1       0x43
#define   CPLE0        0x44
#define   CPLE1        0x45
#define   CPLEU0       0x46
#define   CPLEU1       0x47
#define   CPGT0        0x48
#define   CPGT1        0x49
#define   CPGTU0       0x4A
#define   CPGTU1       0x4B
#define   CPGE0        0x4C
#define   CPGE1        0x4D
#define   CPGEU0       0x4E
#define   CPGEU1       0x4F

#define   ASLT0        0x50
#define   ASLT1        0x51
#define   ASLTU0       0x52
#define   ASLTU1       0x53
#define   ASLE0        0x54
#define   ASLE1        0x55
#define   ASLEU0       0x56
#define   ASLEU1       0x57
#define   ASGT0        0x58
#define   ASGT1        0x59
#define   ASGTU0       0x5A
#define   ASGTU1       0x5B
#define   ASGE0        0x5C
#define   ASGE1        0x5D
#define   ASGEU0       0x5E
#define   ASGEU1       0x5F

#define   CPEQ0        0x60
#define   CPEQ1        0x61
#define   CPNEQ0       0x62
#define   CPNEQ1       0x63
#define   MUL0         0x64
#define   MUL1         0x65
#define   MULL0        0x66
#define   MULL1        0x67
#define   DIV0_OP0     0x68
#define   DIV0_OP1     0x69
#define   DIV_OP0      0x6A
#define   DIV_OP1      0x6B
#define   DIVL0        0x6C
#define   DIVL1        0x6D
#define   DIVREM0      0x6E
#define   DIVREM1      0x6F

#define   ASEQ0        0x70
#define   ASEQ1        0x71
#define   ASNEQ0       0x72
#define   ASNEQ1       0x73
#define   MULU0        0x74
#define   MULU1        0x75
#define   ILLEGAL_76   0x76
#define   ILLEGAL_77   0x77
#define   INHW0        0x78
#define   INHW1        0x79
#define   EXTRACT0     0x7A
#define   EXTRACT1     0x7B
#define   EXHW0        0x7C
#define   EXHW1        0x7D
#define   EXHWS        0x7E
#define   ILLEGAL_7F   0x7F

#define   SLL0         0x80
#define   SLL1         0x81
#define   SRL0         0x82
#define   SRL1         0x83
#define   ILLEGAL_84   0x84
#define   ILLEGAL_85   0x85
#define   SRA0         0x86
#define   SRA1         0x87
#define   IRET         0x88
#define   HALT_OP      0x89
#define   ILLEGAL_8A   0x8A
#define   ILLEGAL_8B   0x8B
#define   IRETINV      0x8C
#define   ILLEGAL_8D   0x8D
#define   ILLEGAL_8E   0x8E
#define   ILLEGAL_8F   0x8F

#define   AND_OP0      0x90
#define   AND_OP1      0x91
#define   OR_OP0       0x92
#define   OR_OP1       0x93
#define   XOR_OP0      0x94
#define   XOR_OP1      0x95
#define   XNOR0        0x96
#define   XNOR1        0x97
#define   NOR0         0x98
#define   NOR1         0x99
#define   NAND0        0x9A
#define   NAND1        0x9B
#define   ANDN0        0x9C
#define   ANDN1        0x9D
#define   SETIP        0x9E
#define   INV          0x9F

#define   JMP0         0xA0
#define   JMP1         0xA1
#define   ILLEGAL_A2   0xA2
#define   ILLEGAL_A3   0xA3
#define   JMPF0        0xA4
#define   JMPF1        0xA5
#define   ILLEGAL_A6   0xA6
#define   ILLEGAL_A7   0xA7
#define   CALL0        0xA8
#define   CALL1        0xA9
#define   ORN_OP0   	0xAA
#define   ORN_OP1   	0xAB
#define   JMPT0        0xAC
#define   JMPT1        0xAD
#define   ILLEGAL_AE   0xAE
#define   ILLEGAL_AF   0xAF

#define   ILLEGAL_B0   0xB0
#define   ILLEGAL_B1   0xB1
#define   ILLEGAL_B2   0xB2
#define   ILLEGAL_B3   0xB3
#define   JMPFDEC0     0xB4
#define   JMPFDEC1     0xB5
#define   MFTLB        0xB6
#define   ILLEGAL_B7   0xB7
#define   ILLEGAL_B8   0xB8
#define   ILLEGAL_B9   0xB9
#define   ILLEGAL_BA   0xBA
#define   ILLEGAL_BB   0xBB
#define   ILLEGAL_BC   0xBC
#define   ILLEGAL_BD   0xBD
#define   MTTLB        0xBE
#define   ILLEGAL_BF   0xBF

#define   JMPI         0xC0
#define   ILLEGAL_C1   0xC1
#define   ILLEGAL_C2   0xC2
#define   ILLEGAL_C3   0xC3
#define   JMPFI        0xC4
#define   ILLEGAL_C5   0xC5
#define   MFSR         0xC6
#define   ILLEGAL_C7   0xC7
#define   CALLI        0xC8
#define   ILLEGAL_C9   0xC9
#define   ILLEGAL_CA   0xCA
#define   ILLEGAL_CB   0xCB
#define   JMPTI        0xCC
#define   ILLEGAL_CD   0xCD
#define   MTSR         0xCE
#define   ILLEGAL_CF   0xCF

#define   ILLEGAL_D0   0xD0
#define   ILLEGAL_D1   0xD1
#define   ILLEGAL_D2   0xD2
#define   ILLEGAL_D3   0xD3
#define   ILLEGAL_D4   0xD4
#define   ILLEGAL_D5   0xD5
#define   ILLEGAL_D6   0xD6
#define   EMULATE      0xD7
#define   ILLEGAL_D8   0xD8
#define   ILLEGAL_D9   0xD9
#define   ILLEGAL_DA   0xDA
#define   ILLEGAL_DB   0xDB
#define   ILLEGAL_DC   0xDC
#define   ILLEGAL_DD   0xDD
#define   MULTM        0xDE
#define   MULTMU       0xDF

#define   MULTIPLY     0xE0
#define   DIVIDE       0xE1
#define   MULTIPLU     0xE2
#define   DIVIDU       0xE3
#define   CONVERT      0xE4
#define   SQRT         0xE5
#define   CLASS        0xE6
#define   ILLEGAL_E7   0xE7
#define   ILLEGAL_E8   0xE8
#define   ILLEGAL_E9   0xE9
#define   FEQ          0xEA
#define   DEQ          0xEB
#define   FGT          0xEC
#define   DGT          0xED
#define   FGE          0xEE
#define   DGE          0xEF

#define   FADD         0xF0
#define   DADD         0xF1
#define   FSUB         0xF2
#define   DSUB         0xF3
#define   FMUL         0xF4
#define   DMUL         0xF5
#define   FDIV         0xF6
#define   DDIV         0xF7
#define   ILLEGAL_F8   0xF8
#define   FDMUL        0xF9
#define   ILLEGAL_FA   0xFA
#define   ILLEGAL_FB   0xFB
#define   ILLEGAL_FC   0xFC
#define   ILLEGAL_FD   0xFD
#define   ILLEGAL_FE   0xFE
#define   ILLEGAL_FF   0xFF

/* External declarations of variable defined in opcodes.c */

extern char 	*opcode_name[];
extern char 	*reg[];
extern char 	*spreg[];

#endif  /* _OPCODES_H_INCLUDED_ */
