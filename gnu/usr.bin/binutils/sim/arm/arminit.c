/*  arminit.c -- ARMulator initialization:  ARM6 Instruction Emulator.
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

#include "armdefs.h"
#include "armemu.h"

/***************************************************************************\
*                 Definitions for the emulator architecture                 *
\***************************************************************************/

void ARMul_EmulateInit(void) ;
ARMul_State *ARMul_NewState(void) ;
void ARMul_Reset(ARMul_State *state) ;
ARMword ARMul_DoCycle(ARMul_State *state) ;
unsigned ARMul_DoCoPro(ARMul_State *state) ;
ARMword ARMul_DoProg(ARMul_State *state) ;
ARMword ARMul_DoInstr(ARMul_State *state) ;
void ARMul_Abort(ARMul_State *state, ARMword address) ;

unsigned ARMul_MultTable[32] = {1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,
                                10,10,11,11,12,12,13,13,14,14,15,15,16,16,16} ;
ARMword ARMul_ImmedTable[4096] ; /* immediate DP LHS values */
char ARMul_BitList[256] ; /* number of bits in a byte table */

/***************************************************************************\
*         Call this routine once to set up the emulator's tables.           *
\***************************************************************************/

void ARMul_EmulateInit(void)
{unsigned long i, j ;

 for (i = 0 ; i < 4096 ; i++) { /* the values of 12 bit dp rhs's */
    ARMul_ImmedTable[i] = ROTATER(i & 0xffL,(i >> 7L) & 0x1eL) ;
    }

 for (i = 0 ; i < 256 ; ARMul_BitList[i++] = 0 ) ; /* how many bits in LSM */
 for (j = 1 ; j < 256 ; j <<= 1)
    for (i = 0 ; i < 256 ; i++)
       if ((i & j) > 0 )
          ARMul_BitList[i]++ ;

  for (i = 0 ; i < 256 ; i++)
    ARMul_BitList[i] *= 4 ; /* you always need 4 times these values */
    
}

/***************************************************************************\
*            Returns a new instantiation of the ARMulator's state           *
\***************************************************************************/

ARMul_State *ARMul_NewState(void)
{ARMul_State *state ;
 unsigned i, j ;

 state = (ARMul_State *)malloc(sizeof(ARMul_State)) ;

 state->Emulate = RUN ;
 for (i = 0 ; i < 16 ; i++) {
    state->Reg[i] = 0 ;
    for (j = 0 ; j < 7 ; j++)
       state->RegBank[j][i] = 0 ;
    }
 for (i = 0 ; i < 7 ; i++)
    state->Spsr[i] = 0 ;
 state->Mode = 0 ;

 state->CallDebug = FALSE ;
 state->Debug = FALSE ;
 state->VectorCatch = 0 ;
 state->Aborted = FALSE ;
 state->Reseted = FALSE ;
 state->Inted = 3 ;
 state->LastInted = 3 ;

 state->MemDataPtr = NULL ;
 state->MemInPtr = NULL ;
 state->MemOutPtr = NULL ;
 state->MemSparePtr = NULL ;
 state->MemSize = 0 ;

 state->OSptr = NULL ;
 state->CommandLine = NULL ;

 state->EventSet = 0 ;
 state->Now = 0 ;
 state->EventPtr = (struct EventNode **)malloc((unsigned)EVENTLISTSIZE *
                                               sizeof(struct EventNode *)) ;
 for (i = 0 ; i < EVENTLISTSIZE ; i++)
    *(state->EventPtr + i) = NULL ;

#ifdef ARM61
 state->prog32Sig = LOW ;
 state->data32Sig = LOW ;
#else
 state->prog32Sig = HIGH ;
 state->data32Sig = HIGH ;
#endif

 state->lateabtSig = LOW ;
 state->bigendSig = LOW ;

 ARMul_Reset(state) ;
 return(state) ;
 }

/***************************************************************************\
*       Call this routine to set ARMulator to model a certain processor     *
\***************************************************************************/
 
void ARMul_SelectProcessor(ARMul_State *state, unsigned processor) {
  if (processor & ARM_Fix26_Prop) {
    state->prog32Sig = LOW;
    state->data32Sig = LOW;
  }else{
    state->prog32Sig = HIGH;
    state->data32Sig = HIGH;
  }
 
  state->lateabtSig = LOW;
}

/***************************************************************************\
* Call this routine to set up the initial machine state (or perform a RESET *
\***************************************************************************/

void ARMul_Reset(ARMul_State *state)
{state->NextInstr = 0 ;
 if (state->prog32Sig) {
    state->Reg[15] = 0 ;
    state->Cpsr = INTBITS | SVC32MODE ;
    }
 else {
    state->Reg[15] = R15INTBITS | SVC26MODE ;
    state->Cpsr = INTBITS | SVC26MODE ;
    }
 ARMul_CPSRAltered(state) ;
 state->Bank = SVCBANK ;
 FLUSHPIPE ;

 state->EndCondition = 0 ;
 state->ErrorCode = 0 ;

 state->Exception = FALSE ;
 state->NresetSig = HIGH ;
 state->NfiqSig = HIGH ;
 state->NirqSig = HIGH ;
 state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
 state->abortSig = LOW ;
 state->AbortAddr = 1 ;

 state->NumInstrs = 0 ;
 state->NumNcycles = 0 ;
 state->NumScycles = 0 ;
 state->NumIcycles = 0 ;
 state->NumCcycles = 0 ;
 state->NumFcycles = 0 ;
#ifdef ASIM    
  (void)ARMul_MemoryInit() ;
  ARMul_OSInit(state) ;
#endif  
}


/***************************************************************************\
* Emulate the execution of an entire program.  Start the correct emulator   *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the last instruction that is executed.                         *
\***************************************************************************/

ARMword ARMul_DoProg(ARMul_State *state)
{ARMword pc = 0 ;

 state->Emulate = RUN ;
 while (state->Emulate != STOP) {
    state->Emulate = RUN ;
    if (state->prog32Sig && ARMul_MODE32BIT)
       pc = ARMul_Emulate32(state) ;
    else
       pc = ARMul_Emulate26(state) ;
    }
 return(pc) ;
 }

/***************************************************************************\
* Emulate the execution of one instruction.  Start the correct emulator     *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the instruction that is executed.                              *
\***************************************************************************/

ARMword ARMul_DoInstr(ARMul_State *state)
{ARMword pc = 0 ;

 state->Emulate = ONCE ;
 if (state->prog32Sig && ARMul_MODE32BIT)
    pc = ARMul_Emulate32(state) ;
 else
    pc = ARMul_Emulate26(state) ;

 return(pc) ;
 }

/***************************************************************************\
* This routine causes an Abort to occur, including selecting the correct    *
* mode, register bank, and the saving of registers.  Call with the          *
* appropriate vector's memory address (0,4,8 ....)                          *
\***************************************************************************/

void ARMul_Abort(ARMul_State *state, ARMword vector)
{ARMword temp ;

 state->Aborted = FALSE ;

 if (ARMul_OSException(state,vector,ARMul_GetPC(state)))
    return ;

 if (state->prog32Sig)
    if (ARMul_MODE26BIT)
       temp = R15PC ;
    else
       temp = state->Reg[15] ;
 else
    temp = R15PC | ECC | ER15INT | EMODE ;

 switch (vector) {
    case ARMul_ResetV : /* RESET */
       state->Spsr[SVCBANK] = CPSR ;
       SETABORT(INTBITS,state->prog32Sig?SVC32MODE:SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp ;
       break ;
    case ARMul_UndefinedInstrV : /* Undefined Instruction */
       state->Spsr[state->prog32Sig?UNDEFBANK:SVCBANK] = CPSR ;
       SETABORT(IBIT,state->prog32Sig?UNDEF32MODE:SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    case ARMul_SWIV : /* Software Interrupt */
       state->Spsr[SVCBANK] = CPSR ;
       SETABORT(IBIT,state->prog32Sig?SVC32MODE:SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    case ARMul_PrefetchAbortV : /* Prefetch Abort */
       state->AbortAddr = 1 ;
       state->Spsr[state->prog32Sig?ABORTBANK:SVCBANK] = CPSR ;
       SETABORT(IBIT,state->prog32Sig?ABORT32MODE:SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    case ARMul_DataAbortV : /* Data Abort */
       state->Spsr[state->prog32Sig?ABORTBANK:SVCBANK] = CPSR ;
       SETABORT(IBIT,state->prog32Sig?ABORT32MODE:SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ; /* the PC must have been incremented */
       break ;
    case ARMul_AddrExceptnV : /* Address Exception */
       state->Spsr[SVCBANK] = CPSR ;
       SETABORT(IBIT,SVC26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    case ARMul_IRQV : /* IRQ */
       state->Spsr[IRQBANK] = CPSR ;
       SETABORT(IBIT,state->prog32Sig?IRQ32MODE:IRQ26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    case ARMul_FIQV : /* FIQ */
       state->Spsr[FIQBANK] = CPSR ;
       SETABORT(INTBITS,state->prog32Sig?FIQ32MODE:FIQ26MODE) ;
       ARMul_CPSRAltered(state) ;
       state->Reg[14] = temp - 4 ;
       break ;
    }
 if (ARMul_MODE32BIT)
    ARMul_SetR15(state,vector) ;
 else
    ARMul_SetR15(state,R15CCINTMODE | vector) ;
}
