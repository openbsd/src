/*  armemu.h -- ARMulator emulation macros:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/***************************************************************************\
*                           Condition code values                           *
\***************************************************************************/

#define EQ 0
#define NE 1
#define CS 2
#define CC 3
#define MI 4
#define PL 5
#define VS 6
#define VC 7
#define HI 8
#define LS 9
#define GE 10
#define LT 11
#define GT 12
#define LE 13
#define AL 14
#define NV 15

/***************************************************************************\
*                               Shift Opcodes                               *
\***************************************************************************/

#define LSL 0
#define LSR 1
#define ASR 2
#define ROR 3

/***************************************************************************\
*               Macros to twiddle the status flags and mode                 *
\***************************************************************************/

#define NBIT ((unsigned)1L << 31)
#define ZBIT (1L << 30)
#define CBIT (1L << 29)
#define VBIT (1L << 28)
#define IBIT (1L << 7)
#define FBIT (1L << 6)
#define IFBITS (3L << 6)
#define R15IBIT (1L << 27)
#define R15FBIT (1L << 26)
#define R15IFBITS (3L << 26)

#define POS(i) ( (~(i)) >> 31 )
#define NEG(i) ( (i) >> 31 )

#define NFLAG state->NFlag
#define SETN state->NFlag = 1
#define CLEARN state->NFlag = 0
#define ASSIGNN(res) state->NFlag = res

#define ZFLAG state->ZFlag
#define SETZ state->ZFlag = 1
#define CLEARZ state->ZFlag = 0
#define ASSIGNZ(res) state->ZFlag = res

#define CFLAG state->CFlag
#define SETC state->CFlag = 1
#define CLEARC state->CFlag = 0
#define ASSIGNC(res) state->CFlag = res

#define VFLAG state->VFlag
#define SETV state->VFlag = 1
#define CLEARV state->VFlag = 0
#define ASSIGNV(res) state->VFlag = res

#define IFLAG (state->IFFlags >> 1)
#define FFLAG (state->IFFlags & 1)
#define IFFLAGS state->IFFlags
#define ASSIGNINT(res) state->IFFlags = (((res) >> 6) & 3)
#define ASSIGNR15INT(res) state->IFFlags = (((res) >> 26) & 3) ;

#define CCBITS (0xf0000000L)
#define INTBITS (0xc0L)
#define PCBITS (0xfffffffcL)
#define MODEBITS (0x1fL)
#define R15INTBITS (3L << 26)
#define R15PCBITS (0x03fffffcL)
#define R15PCMODEBITS (0x03ffffffL)
#define R15MODEBITS (0x3L)

#ifdef MODE32
#define PCMASK PCBITS
#define PCWRAP(pc) (pc)
#else
#define PCMASK R15PCBITS
#define PCWRAP(pc) ((pc) & R15PCBITS)
#endif
#define PC (state->Reg[15] & PCMASK)
#define R15CCINTMODE (state->Reg[15] & (CCBITS | R15INTBITS | R15MODEBITS))
#define R15INT (state->Reg[15] & R15INTBITS)
#define R15INTPC (state->Reg[15] & (R15INTBITS | R15PCBITS))
#define R15INTPCMODE (state->Reg[15] & (R15INTBITS | R15PCBITS | R15MODEBITS))
#define R15INTMODE (state->Reg[15] & (R15INTBITS | R15MODEBITS))
#define R15PC (state->Reg[15] & R15PCBITS)
#define R15PCMODE (state->Reg[15] & (R15PCBITS | R15MODEBITS))
#define R15MODE (state->Reg[15] & R15MODEBITS)

#define ECC ((NFLAG << 31) | (ZFLAG << 30) | (CFLAG << 29) | (VFLAG << 28))
#define EINT (IFFLAGS << 6)
#define ER15INT (IFFLAGS << 26)
#define EMODE (state->Mode)

#define CPSR (ECC | EINT | EMODE)
#ifdef MODE32
#define PATCHR15
#else
#define PATCHR15 state->Reg[15] = ECC | ER15INT | EMODE | R15PC
#endif

#define GETSPSR(bank) bank>0?state->Spsr[bank]:ECC | EINT | EMODE ;
#define SETPSR(d,s) d = (s) & (ARMword)(CCBITS | INTBITS | MODEBITS)
#define SETINTMODE(d,s) d = ((d) & CCBITS) | ((s) & (INTBITS | MODEBITS))
#define SETCC(d,s) d = ((d) & (INTBITS | MODEBITS)) | ((s) & CCBITS)
#define SETR15PSR(s) if (state->Mode == USER26MODE) { \
                        state->Reg[15] = ((s) & CCBITS) | R15PC | ER15INT | EMODE ; \
                        ASSIGNN((state->Reg[15] & NBIT) != 0) ; \
                        ASSIGNZ((state->Reg[15] & ZBIT) != 0) ; \
                        ASSIGNC((state->Reg[15] & CBIT) != 0) ; \
                        ASSIGNV((state->Reg[15] & VBIT) != 0) ; \
                        } \
                     else { \
                        state->Reg[15] = R15PC | (s) & (CCBITS | R15INTBITS | R15MODEBITS) ; \
                        ARMul_R15Altered(state) ; \
                        }
#define SETABORT(i,m) state->Cpsr = ECC | EINT | (i) | (m)

#ifndef MODE32
#define VECTORS 0x20
#define LEGALADDR 0x03ffffff
#define VECTORACCESS(address) (address < VECTORS && ARMul_MODE26BIT && state->prog32Sig)
#define ADDREXCEPT(address) (address > LEGALADDR && !state->data32Sig)
#endif

#define INTERNALABORT(address) if (address < VECTORS) \
                                  state->Aborted = ARMul_DataAbortV ; \
                               else \
                                  state->Aborted = ARMul_AddrExceptnV ;

#ifdef MODE32
#define TAKEABORT ARMul_Abort(state,ARMul_DataAbortV)
#else
#define TAKEABORT if (state->Aborted == ARMul_AddrExceptnV) \
                     ARMul_Abort(state,ARMul_AddrExceptnV) ; \
                  else \
                     ARMul_Abort(state,ARMul_DataAbortV)
#endif
#define CPTAKEABORT if (!state->Aborted) \
                       ARMul_Abort(state,ARMul_UndefinedInstrV) ; \
                    else if (state->Aborted == ARMul_AddrExceptnV) \
                       ARMul_Abort(state,ARMul_AddrExceptnV) ; \
                    else \
                       ARMul_Abort(state,ARMul_DataAbortV)


/***************************************************************************\
*               Different ways to start the next instruction                *
\***************************************************************************/

#define SEQ 0
#define NONSEQ 1
#define PCINCEDSEQ 2
#define PCINCEDNONSEQ 3
#define PRIMEPIPE 4
#define RESUME 8

#define NORMALCYCLE state->NextInstr = 0
#define BUSUSEDN state->NextInstr |= 1 /* the next fetch will be an N cycle */
#define BUSUSEDINCPCS state->Reg[15] += 4 ; /* a standard PC inc and an S cycle */ \
                      state->NextInstr = (state->NextInstr & 0xff) | 2
#define BUSUSEDINCPCN state->Reg[15] += 4 ; /* a standard PC inc and an N cycle */ \
                      state->NextInstr |= 3
#define INCPC state->Reg[15] += 4 ; /* a standard PC inc */ \
              state->NextInstr |= 2
#define FLUSHPIPE state->NextInstr |= PRIMEPIPE

/***************************************************************************\
*                          Cycle based emulation                            *
\***************************************************************************/

#define OUTPUTCP(i,a,b)
#define NCYCLE
#define SCYCLE
#define ICYCLE
#define CCYCLE
#define NEXTCYCLE(c)

/***************************************************************************\
*                 States of the cycle based state machine                   *
\***************************************************************************/


/***************************************************************************\
*                 Macros to extract parts of instructions                   *
\***************************************************************************/

#define DESTReg (BITS(12,15))
#define LHSReg (BITS(16,19))
#define RHSReg (BITS(0,3))

#define DEST (state->Reg[DESTReg])

#ifdef MODE32
#define LHS (state->Reg[LHSReg])
#else
#define LHS ((LHSReg == 15) ? R15PC : (state->Reg[LHSReg]) )
#endif

#define MULDESTReg (BITS(16,19))
#define MULLHSReg (BITS(0,3))
#define MULRHSReg (BITS(8,11))
#define MULACCReg (BITS(12,15))

#define DPImmRHS (ARMul_ImmedTable[BITS(0,11)])
#define DPSImmRHS temp = BITS(0,11) ; \
                  rhs = ARMul_ImmedTable[temp] ; \
                  if (temp > 255) /* there was a shift */ \
                     ASSIGNC(rhs >> 31) ;

#ifdef MODE32
#define DPRegRHS ((BITS(4,11)==0) ? state->Reg[RHSReg] \
                                  : GetDPRegRHS(state, instr))
#define DPSRegRHS ((BITS(4,11)==0) ? state->Reg[RHSReg] \
                                   : GetDPSRegRHS(state, instr))
#else
#define DPRegRHS ((BITS(0,11)<15) ? state->Reg[RHSReg] \
                                  : GetDPRegRHS(state, instr))
#define DPSRegRHS ((BITS(0,11)<15) ? state->Reg[RHSReg] \
                                   : GetDPSRegRHS(state, instr))
#endif

#define LSBase state->Reg[LHSReg]
#define LSImmRHS (BITS(0,11))

#ifdef MODE32
#define LSRegRHS ((BITS(4,11)==0) ? state->Reg[RHSReg] \
                                  : GetLSRegRHS(state, instr))
#else
#define LSRegRHS ((BITS(0,11)<15) ? state->Reg[RHSReg] \
                                  : GetLSRegRHS(state, instr))
#endif

#define LSMNumRegs ((ARMword)ARMul_BitList[BITS(0,7)] + \
                    (ARMword)ARMul_BitList[BITS(8,15)] )
#define LSMBaseFirst ((LHSReg == 0 && BIT(0)) || \
                      (BIT(LHSReg) && BITS(0,LHSReg-1) == 0))

#define SWAPSRC (state->Reg[RHSReg])

#define LSCOff (BITS(0,7) << 2)
#define CPNum BITS(8,11)

/***************************************************************************\
*                    Macro to rotate n right by b bits                      *
\***************************************************************************/

#define ROTATER(n,b) (((n)>>(b))|((n)<<(32-(b))))

/***************************************************************************\
*                 Macros to store results of instructions                   *
\***************************************************************************/

#define WRITEDEST(d) if (DESTReg==15) \
                        WriteR15(state, d) ; \
                     else \
                          DEST = d

#define WRITESDEST(d) if (DESTReg == 15) \
                         WriteSR15(state, d) ; \
                      else { \
                         DEST = d ; \
                         ARMul_NegZero(state, d) ; \
                         }

#define BYTETOBUS(data) ((data & 0xff) | \
                        ((data & 0xff) << 8) | \
                        ((data & 0xff) << 16) | \
                        ((data & 0xff) << 24))
#define BUSTOBYTE(address,data) \
           if (state->bigendSig) \
              temp = (data >> (((address ^ 3) & 3) << 3)) & 0xff ; \
           else \
              temp = (data >> ((address & 3) << 3)) & 0xff

#define LOADMULT(instr,address,wb) LoadMult(state,instr,address,wb)
#define LOADSMULT(instr,address,wb) LoadSMult(state,instr,address,wb)
#define STOREMULT(instr,address,wb) StoreMult(state,instr,address,wb)
#define STORESMULT(instr,address,wb) StoreSMult(state,instr,address,wb)

#define POSBRANCH ((instr & 0x7fffff) << 2)
#define NEGBRANCH (0xff000000 | ((instr & 0xffffff) << 2))

/***************************************************************************\
*                          Values for Emulate                               *
\***************************************************************************/

#define STOP            0 /* stop */
#define CHANGEMODE      1 /* change mode */
#define ONCE            2 /* execute just one interation */
#define RUN             3 /* continuous execution */

/***************************************************************************\
*                      Stuff that is shared across modes                    *
\***************************************************************************/

extern ARMword ARMul_Emulate26(ARMul_State *state) ;
extern ARMword ARMul_Emulate32(ARMul_State *state) ;
extern unsigned ARMul_MultTable[] ; /* Number of I cycles for a mult */
extern ARMword ARMul_ImmedTable[] ; /* immediate DP LHS values */
extern char ARMul_BitList[] ; /* number of bits in a byte table */
extern void ARMul_Abort26(ARMul_State *state, ARMword) ;
extern void ARMul_Abort32(ARMul_State *state, ARMword) ;
extern unsigned ARMul_NthReg(ARMword instr,unsigned number) ;
extern void ARMul_MSRCpsr(ARMul_State *state, ARMword instr, ARMword rhs) ;
extern void ARMul_NegZero(ARMul_State *state, ARMword result) ;
extern void ARMul_AddCarry(ARMul_State *state, ARMword a, ARMword b, ARMword result) ;
extern void ARMul_AddOverflow(ARMul_State *state, ARMword a, ARMword b, ARMword result) ;
extern void ARMul_SubCarry(ARMul_State *state, ARMword a, ARMword b, ARMword result) ;
extern void ARMul_SubOverflow(ARMul_State *state, ARMword a, ARMword b, ARMword result) ;
extern void ARMul_CPSRAltered(ARMul_State *state) ;
extern void ARMul_R15Altered(ARMul_State *state) ;
extern ARMword ARMul_SwitchMode(ARMul_State *state,ARMword oldmode, ARMword newmode) ;
extern unsigned ARMul_NthReg(ARMword instr, unsigned number) ;
extern void ARMul_LDC(ARMul_State *state,ARMword instr,ARMword address) ;
extern void ARMul_STC(ARMul_State *state,ARMword instr,ARMword address) ;
extern void ARMul_MCR(ARMul_State *state,ARMword instr, ARMword source) ;
extern ARMword ARMul_MRC(ARMul_State *state,ARMword instr) ;
extern void ARMul_CDP(ARMul_State *state,ARMword instr) ;
extern unsigned IntPending(ARMul_State *state) ;
extern ARMword ARMul_Align(ARMul_State *state, ARMword address, ARMword data) ;
#define EVENTLISTSIZE 1024L

/***************************************************************************\
*                      Macros to scrutinize instructions                    *
\***************************************************************************/


#define UNDEF_Test
#define UNDEF_Shift
#define UNDEF_MSRPC
#define UNDEF_MRSPC
#define UNDEF_MULPCDest
#define UNDEF_MULDestEQOp1
#define UNDEF_LSRBPC
#define UNDEF_LSRBaseEQOffWb
#define UNDEF_LSRBaseEQDestWb
#define UNDEF_LSRPCBaseWb
#define UNDEF_LSRPCOffWb
#define UNDEF_LSMNoRegs
#define UNDEF_LSMPCBase
#define UNDEF_LSMUserBankWb
#define UNDEF_LSMBaseInListWb
#define UNDEF_SWPPC
#define UNDEF_CoProHS
#define UNDEF_MCRPC
#define UNDEF_LSCPCBaseWb
#define UNDEF_UndefNotBounced
#define UNDEF_ShortInt
#define UNDEF_IllegalMode
#define UNDEF_Prog32SigChange
#define UNDEF_Data32SigChange

