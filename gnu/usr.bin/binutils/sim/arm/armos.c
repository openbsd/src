/*  armos.c -- ARMulator OS interface:  ARM6 Instruction Emulator.
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

/* This file contains a model of Demon, ARM Ltd's Debug Monitor,
including all the SWI's required to support the C library. The code in
it is not really for the faint-hearted (especially the abort handling
code), but it is a complete example. Defining NOOS will disable all the
fun, and definign VAILDATE will define SWI 1 to enter SVC mode, and SWI
0x11 to halt the emulator. */

#include <time.h>
#include <errno.h>
#include <string.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

#ifdef __STDC__
#define unlink(s) remove(s)
#endif

#ifdef __sun__
#include <unistd.h>	/* For SEEK_SET etc */
#endif

#ifdef __riscos
extern int _fisatty(FILE *);
#define isatty_(f) _fisatty(f)
#else
#ifdef __ZTC__
#include <io.h>
#define isatty_(f) isatty((f)->_file)
#else
#ifdef macintosh
#include <ioctl.h>
#define isatty_(f) (~ioctl((f)->_file,FIOINTERACTIVE,NULL))
#else
#define isatty_(f) isatty(fileno(f))
#endif
#endif
#endif

#include "armdefs.h"
#include "armos.h"
#ifndef NOOS
#ifndef VALIDATE
/* #ifndef ASIM */
#include "armfpe.h"
/* #endif */
#endif
#endif

/* For RDIError_BreakpointReached.  */
#include "dbg_rdi.h"

extern unsigned ARMul_OSInit(ARMul_State *state) ;
extern void ARMul_OSExit(ARMul_State *state) ;
extern unsigned ARMul_OSHandleSWI(ARMul_State *state,ARMword number) ;
extern unsigned ARMul_OSException(ARMul_State *state, ARMword vector, ARMword pc) ;
extern ARMword ARMul_OSLastErrorP(ARMul_State *state) ;
extern ARMword ARMul_Debug(ARMul_State *state, ARMword pc, ARMword instr) ;

#define BUFFERSIZE 4096
#ifndef FOPEN_MAX
#define FOPEN_MAX 64
#endif
#define UNIQUETEMPS 256

#ifndef NOOS
static void UnwindDataAbort(ARMul_State *state, ARMword addr);
static void getstring(ARMul_State *state, ARMword from, char *to) ;
#endif

/***************************************************************************\
*                          OS private Information                           *
\***************************************************************************/

struct OSblock {
   ARMword Time0 ;
   ARMword ErrorP ;
   ARMword ErrorNo ;
   FILE *FileTable[FOPEN_MAX] ;
   char FileFlags[FOPEN_MAX] ;
   char *tempnames[UNIQUETEMPS] ;
   } ;

#define NOOP 0
#define BINARY 1
#define READOP 2
#define WRITEOP 4

#ifdef macintosh
#define FIXCRLF(t,c) ((t & BINARY)?c:((c=='\n'||c=='\r')?(c ^ 7):c))
#else                   
#define FIXCRLF(t,c) c 
#endif

static ARMword softvectorcode[] =
{   /* basic: swi tidyexception + event; mov pc, lr;
              ldmia r11,{r11,pc}; swi generateexception + event
     */
  0xef000090, 0xe1a0e00f, 0xe89b8800, 0xef000080, /*Reset*/
  0xef000091, 0xe1a0e00f, 0xe89b8800, 0xef000081, /*Undef*/
  0xef000092, 0xe1a0e00f, 0xe89b8800, 0xef000082, /*SWI  */
  0xef000093, 0xe1a0e00f, 0xe89b8800, 0xef000083, /*Prefetch abort*/
  0xef000094, 0xe1a0e00f, 0xe89b8800, 0xef000084, /*Data abort*/
  0xef000095, 0xe1a0e00f, 0xe89b8800, 0xef000085, /*Address exception*/
  0xef000096, 0xe1a0e00f, 0xe89b8800, 0xef000086, /*IRQ*/
  0xef000097, 0xe1a0e00f, 0xe89b8800, 0xef000087, /*FIQ*/
  0xef000098, 0xe1a0e00f, 0xe89b8800, 0xef000088, /*Error*/
  0xe1a0f00e /* default handler */
};

/***************************************************************************\
*            Time for the Operating System to initialise itself.            *
\***************************************************************************/

unsigned ARMul_OSInit(ARMul_State *state)
{
#ifndef NOOS
#ifndef VALIDATE
 ARMword instr, i , j ;
 struct OSblock* OSptr = (struct OSblock*)state->OSptr;

 if (state->OSptr == NULL) {
    state->OSptr = (unsigned char *)malloc(sizeof(struct OSblock));
    if (state->OSptr == NULL) {
       perror("OS Memory");
       exit(15);
       }
    }
 OSptr = (struct OSblock*)state->OSptr;
 OSptr->ErrorP = 0;
 state->Reg[13] = ADDRSUPERSTACK;  /* set up a stack for the current mode */
 ARMul_SetReg(state,SVC32MODE,13,ADDRSUPERSTACK); /* and for supervisor mode */
 ARMul_SetReg(state,ABORT32MODE,13,ADDRSUPERSTACK); /* and for abort 32 mode */
 ARMul_SetReg(state,UNDEF32MODE,13,ADDRSUPERSTACK); /* and for undef 32 mode */
 instr = 0xe59ff000 | (ADDRSOFTVECTORS - 8); /* load pc from soft vector */
 for (i = ARMul_ResetV ; i <= ARMFIQV ; i += 4)
    ARMul_WriteWord(state, i, instr);    /* write hardware vectors */
 for (i = ARMul_ResetV ; i <= ARMFIQV + 4 ; i += 4) {
    ARMul_WriteWord(state, ADDRSOFTVECTORS + i, SOFTVECTORCODE + i * 4);
    ARMul_WriteWord(state, ADDRSOFHANDLERS + 2*i + 4L, SOFTVECTORCODE + sizeof(softvectorcode) - 4L);
    }
 for (i = 0 ; i < sizeof(softvectorcode) ; i += 4)
    ARMul_WriteWord(state, SOFTVECTORCODE + i, softvectorcode[i/4]);
 for (i = 0 ; i < FOPEN_MAX ; i++)
    OSptr->FileTable[i] = NULL ;
 for (i = 0 ; i < UNIQUETEMPS ; i++)
    OSptr->tempnames[i] = NULL ;
 ARMul_ConsolePrint (state, ", Demon 1.01");

/* #ifndef ASIM */

 /* install fpe */
 for (i = 0 ; i < fpesize ; i+=4) /* copy the code */
    ARMul_WriteWord(state,FPESTART + i,fpecode[i >> 2]) ;
 for (i = FPESTART + fpesize ; ; i-=4) { /* reverse the error strings */
    if ((j = ARMul_ReadWord(state,i)) == 0xffffffff)
       break ;
    if (state->bigendSig && j < 0x80000000) { /* it's part of the string so swap it */
       j = ((j >> 0x18) & 0x000000ff) |
           ((j >> 0x08) & 0x0000ff00) |
           ((j << 0x08) & 0x00ff0000) |
           ((j << 0x18) & 0xff000000) ;
       ARMul_WriteWord(state,i,j) ;
       }
    }
 ARMul_WriteWord(state,FPEOLDVECT,ARMul_ReadWord(state,4)) ; /* copy old illegal instr vector */
 ARMul_WriteWord(state,4,FPENEWVECT(ARMul_ReadWord(state,i-4))) ; /* install new vector */
 ARMul_ConsolePrint (state, ", FPE") ;

/* #endif /* ASIM */
#endif /* VALIDATE */
#endif /* NOOS */

 return(TRUE) ;
}

void ARMul_OSExit(ARMul_State *state)
{
 free((char *)state->OSptr) ;
}


/***************************************************************************\
*                  Return the last Operating System Error.                  *
\***************************************************************************/

ARMword ARMul_OSLastErrorP(ARMul_State *state)
{
  return ((struct OSblock *)state->OSptr)->ErrorP;
}

/***************************************************************************\
* The emulator calls this routine when a SWI instruction is encuntered. The *
* parameter passed is the SWI number (lower 24 bits of the instruction).    *
\***************************************************************************/

#if 1
/* This is the cygnus way of doing it, which makes it simple
   to do our tests */

unsigned ARMul_OSHandleSWI(ARMul_State *state,ARMword number)
{
  ARMword addr, temp ;
  char buffer[BUFFERSIZE], *cptr ;
  FILE *fptr ;
  struct OSblock* OSptr = (struct OSblock*)state->OSptr ;

  switch (number)
    {
    case SWI_Read:
      {
	int f = state->Reg[0];
	int ptr = state->Reg[1];
	int len = state->Reg[2];
	int res;
	int i;
	char *local = malloc (len);
	res = read (f,local, len);
	if (res > 0)
	  for (i = 0; i < res; i++) 
	    ARMul_WriteByte(state, ptr + i, local[i]) ;
	free (local);
	state->Reg[0] = res == -1 ? -1 : len - res;
	OSptr->ErrorNo = errno;
	return TRUE;     
      }

    case SWI_Write:
      {
	int f = state->Reg[0];
	int ptr = state->Reg[1];
	int len = state->Reg[2];
	int res;
	int i;
	char *local = malloc (len);
	for (i = 0; i < len; i++) 
	  {
	    local[i] = ARMul_ReadByte(state, ptr + i);
	  }
	res = write (f, local, len);
	state->Reg[0] = res == -1 ? -1 : len - res;
	free (local);
	OSptr->ErrorNo = errno;
	return TRUE;     
      }

    case SWI_Open:
      {
	char dummy[2000];
	int i;
	for (i = 0; 
	     dummy[i] = ARMul_ReadByte(state, state->Reg[0] + i);
	     i++)
	  ;
	state->Reg[0] = open (dummy, state->Reg[1]);
	OSptr->ErrorNo = errno;
	return TRUE;
      }
   
    case SWI_Close:
      state->Reg[0] = close (state->Reg[0]);
      OSptr->ErrorNo = errno;
      return TRUE;

    case SWI_Exit:
      state->Emulate = FALSE ;
      return TRUE;

    case SWI_Seek:
      {
	state->Reg[0] = lseek (state->Reg[0],
			       state->Reg[1],
			       state->Reg[2]);
	OSptr->ErrorNo = errno;
	return TRUE;
      }

    case SWI_WriteC :
      (void)fputc((int)state->Reg[0],stderr) ;
      OSptr->ErrorNo = errno ;
      return(TRUE) ;

    case SWI_GetErrno :
      state->Reg[0] = OSptr->ErrorNo ;
      return(TRUE) ;

    case SWI_Breakpoint :
      state->EndCondition = RDIError_BreakpointReached ;
      state->Emulate = FALSE ;
      return(TRUE) ;

    default :
      state->Emulate = FALSE ;      
      return(FALSE) ;
    }
}
#else
unsigned ARMul_OSHandleSWI(ARMul_State *state,ARMword number)
{
#ifdef NOOS
 return(FALSE) ;
#else
#ifdef VALIDATE
 switch (number) {
    case 0x11 :
       state->Emulate = FALSE ;
       return(TRUE) ;
    case 0x01 :
       if (ARM32BITMODE)
          ARMul_SetCPSR(state, (ARMul_GetCPSR(state) & 0xffffffc0) | 0x13) ;
       else
          ARMul_SetCPSR(state, (ARMul_GetCPSR(state) & 0xffffffc0) | 0x3) ;
       return(TRUE) ;
    default :
       return(FALSE) ;
    }
#else
 ARMword addr, temp ;
 char buffer[BUFFERSIZE], *cptr ;
 FILE *fptr ;
 struct OSblock* OSptr = (struct OSblock*)state->OSptr ;

 switch (number) {
    case SWI_WriteC :
       (void)fputc((int)state->Reg[0],stderr) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Write0 :
       addr = state->Reg[0] ;
       while ((temp = ARMul_ReadByte(state,addr++)) != 0)
          fputc((char)temp,stderr) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_ReadC :
       state->Reg[0] = (ARMword)fgetc(stdin) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_CLI :
       addr = state->Reg[0] ;
       getstring(state,state->Reg[0],buffer) ;
       state->Reg[0] = (ARMword)system(buffer) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_GetEnv :
       state->Reg[0] = ADDRCMDLINE ;
       if (state->MemSize)
          state->Reg[1] = state->MemSize ;
       else
          state->Reg[1] = ADDRUSERSTACK ;

       addr = state->Reg[0] ;
       cptr = state->CommandLine ;
       if (cptr == NULL)
          cptr = "\0" ;
       do {
          temp = (ARMword)*cptr++ ;
          ARMul_WriteByte(state,addr++,temp) ;
          } while (temp != 0) ;
       return(TRUE) ;

    case SWI_Exit :
#ifdef ASIM
       simkernel1_abort_run() ;
#else
       state->Emulate = FALSE ;
#endif
       return(TRUE) ;

    case SWI_EnterOS :
       if (ARM32BITMODE)
          ARMul_SetCPSR(state, (ARMul_GetCPSR(state) & 0xffffffc0) | 0x13) ;
       else
          ARMul_SetCPSR(state, (ARMul_GetCPSR(state) & 0xffffffc0) | 0x3) ;
       return(TRUE) ;

    case SWI_GetErrno :
       state->Reg[0] = OSptr->ErrorNo ;
       return(TRUE) ;

    case SWI_Clock :
       /* return muber of centi-seconds... */
       state->Reg[0] =
#ifdef CLOCKS_PER_SEC
          (CLOCKS_PER_SEC >= 100)
             ? (ARMword) (clock() / (CLOCKS_PER_SEC / 100))
             : (ARMword) ((clock() * 100) / CLOCKS_PER_SEC) ;
#else
     /* presume unix... clock() returns microseconds */
          (ARMword) (clock() / 10000) ;
#endif
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Time :
       state->Reg[0] = (ARMword)time(NULL) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Remove :
       getstring(state,state->Reg[0],buffer) ;
       state->Reg[0] = unlink(buffer) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Rename : {
       char buffer2[BUFFERSIZE] ;

       getstring(state,state->Reg[0],buffer) ;
       getstring(state,state->Reg[1],buffer2) ;
       state->Reg[0] = rename(buffer,buffer2) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;
       }

    case SWI_Open : {
#if 0
      /* It seems to me that these are in the wrong order
	 sac@cygnus.com, so I've redone it to use the
	 flags instead, with the functionality which was already
	 there -- ahh, perhaps the TRUNC bit is in a different
	 place on the original host ?*/
       static char* fmode[] = {"r","rb","r+","r+b",
                               "w","wb","w+","w+b",
                               "a","ab","a+","a+b",
                               "r","r","r","r"} /* last 4 are illegal */ ;
#endif

       unsigned type ;

       type = (unsigned)(state->Reg[1] & 3L) ;
       getstring(state,state->Reg[0],buffer) ;
       if (strcmp(buffer,":tt")==0 && (type == O_RDONLY )) /* opening tty "r" */
          fptr = stdin ;
       else if (strcmp(buffer,":tt")==0 && (type == O_WRONLY)) /* opening tty "w" */
          fptr = stderr ;
       else
	 {
	   switch (type) 
	     {
	     case O_RDONLY:
	       fptr = fopen(buffer,"r") ;
	       break;
	     case O_WRONLY:
	       fptr = fopen(buffer,"w") ;
	       break;
	     case O_RDWR:
	       fptr = fopen(buffer,"rw") ;
	       break;
	     }
	 }

       state->Reg[0] = 0 ;
       if (fptr != NULL) {
          for (temp = 0 ; temp < FOPEN_MAX ; temp++)
             if (OSptr->FileTable[temp] == NULL) {
                OSptr->FileTable[temp] = fptr ;
                OSptr->FileFlags[temp] = type & 1 ; /* preserve the binary bit */
                state->Reg[0] = (ARMword)(temp + 1) ;
                break ;
                }
          if (state->Reg[0] == 0)
             OSptr->ErrorNo = EMFILE ; /* too many open files */
          else
             OSptr->ErrorNo = errno ;
          }
       else
         OSptr->ErrorNo = errno ;
       return(TRUE) ;
       }

    case SWI_Close :
       temp = state->Reg[0] ;
       if (temp == 0 || temp > FOPEN_MAX || OSptr->FileTable[temp - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       temp-- ;
       fptr = OSptr->FileTable[temp] ;
       if (fptr == stdin || fptr == stderr)
          state->Reg[0] = 0 ;
       else
          state->Reg[0] = fclose(fptr) ;
       OSptr->FileTable[temp] = NULL ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Write : {
       unsigned size, upto, type ;
       char ch ;

       temp = state->Reg[0] ;
       if (temp == 0 || temp > FOPEN_MAX || OSptr->FileTable[temp - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       temp-- ;
       fptr = OSptr->FileTable[temp] ;
       type = OSptr->FileFlags[temp] ;
       addr = state->Reg[1] ;
       size = (unsigned)state->Reg[2] ;

       if (type & READOP)
          fseek(fptr,0L,SEEK_CUR) ;
       OSptr->FileFlags[temp] = (type & BINARY) | WRITEOP ; ;
       while (size > 0) {
          if (size >= BUFFERSIZE)
             upto = BUFFERSIZE ;
          else
             upto = size ;
          for (cptr = buffer ; (cptr - buffer) < upto ; cptr++) {
             ch = (char)ARMul_ReadByte(state,(ARMword)addr++) ;
             *cptr = FIXCRLF(type,ch) ;
             }
          temp = fwrite(buffer,1,upto,fptr) ;
          if (temp < upto) {
             state->Reg[0] = (ARMword)(size - temp) ;
             OSptr->ErrorNo = errno ;
             return(TRUE) ;
             }
          size -= upto ;
          }
       state->Reg[0] = 0 ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;
       }

    case SWI_Read : {
       unsigned size, upto, type ;
       char ch ;

       temp = state->Reg[0] ;
       if (temp == 0 || temp > FOPEN_MAX || OSptr->FileTable[temp - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       temp-- ;
       fptr = OSptr->FileTable[temp] ;
       addr = state->Reg[1] ;
       size = (unsigned)state->Reg[2] ;
       type = OSptr->FileFlags[temp] ;

       if (type & WRITEOP)
          fseek(fptr,0L,SEEK_CUR) ;
       OSptr->FileFlags[temp] = (type & BINARY) | READOP ; ;
       while (size > 0) {
          if (isatty_(fptr)) {
             upto = (size >= BUFFERSIZE)?BUFFERSIZE:size + 1 ;
             if (fgets(buffer, upto, fptr) != 0)
               temp = strlen(buffer) ;
             else
               temp = 0 ;
             upto-- ; /* 1 char used for terminating null */
             }
          else {
             upto = (size>=BUFFERSIZE)?BUFFERSIZE:size ;
             temp = fread(buffer,1,upto,fptr) ;
             }
          for (cptr = buffer ; (cptr - buffer) < temp ; cptr++) {
             ch = *cptr ;
             ARMul_WriteByte(state,(ARMword)addr++,FIXCRLF(type,ch)) ;
             }
          if (temp < upto) {
             state->Reg[0] = (ARMword)(size - temp) ;
             OSptr->ErrorNo = errno ;
             return(TRUE) ;
             }
          size -= upto ;
          }
       state->Reg[0] = 0 ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;
       }

    case SWI_Seek :
       if (state->Reg[0] == 0 || state->Reg[0] > FOPEN_MAX
           || OSptr->FileTable[state->Reg[0] - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       fptr = OSptr->FileTable[state->Reg[0] - 1] ;
       state->Reg[0] = fseek(fptr,(long)state->Reg[1],SEEK_SET) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_Flen :
       if (state->Reg[0] == 0 || state->Reg[0] > FOPEN_MAX
           || OSptr->FileTable[state->Reg[0] - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       fptr = OSptr->FileTable[state->Reg[0] - 1] ;
       addr = (ARMword)ftell(fptr) ;
       if (fseek(fptr,0L,SEEK_END) < 0)
          state->Reg[0] = -1 ;
       else {
          state->Reg[0] = (ARMword)ftell(fptr) ;
          (void)fseek(fptr,addr,SEEK_SET) ;
          }
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_IsTTY :
       if (state->Reg[0] == 0 || state->Reg[0] > FOPEN_MAX
           || OSptr->FileTable[state->Reg[0] - 1] == 0) {
          OSptr->ErrorNo = EBADF ;
          state->Reg[0] = -1L ;
          return(TRUE) ;
          }
       fptr = OSptr->FileTable[state->Reg[0] - 1] ;
       state->Reg[0] = isatty_(fptr) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;

    case SWI_TmpNam :{
       ARMword size ;

       addr = state->Reg[0] ;
       temp = state->Reg[1] & 0xff ;
       size = state->Reg[2] ;
       if (OSptr->tempnames[temp] == NULL) {
          if ((OSptr->tempnames[temp] = malloc(L_tmpnam)) == NULL) {
             state->Reg[0] = 0 ;
             return(TRUE) ;
             }
          (void)tmpnam(OSptr->tempnames[temp]) ;
          }
       cptr = OSptr->tempnames[temp] ;
       if (strlen(cptr) > state->Reg[2])
          state->Reg[0] = 0 ;
       else
          do {
             ARMul_WriteByte(state,addr++,*cptr) ;
             } while (*cptr++ != 0) ;
       OSptr->ErrorNo = errno ;
       return(TRUE) ;
       }

    case SWI_InstallHandler:
       {  ARMword handlerp = ADDRSOFHANDLERS + state->Reg[0] * 8;
          ARMword oldr1 = ARMul_ReadWord(state, handlerp),
                  oldr2 = ARMul_ReadWord(state, handlerp + 4);
          ARMul_WriteWord(state, handlerp, state->Reg[1]);
          ARMul_WriteWord(state, handlerp + 4, state->Reg[2]);
          state->Reg[1] = oldr1;
          state->Reg[2] = oldr2;
          return(TRUE);
       }

    case SWI_GenerateError:
       ARMul_Abort(state, ARMSWIV) ;
       if (state->Emulate)
          ARMul_SetR15(state, ARMul_ReadWord(state, ADDRSOFTVECTORS + ARMErrorV));
       return(TRUE);

/* SWI's 0x9x unwind the state of the CPU after an abort of type x */

    case 0x90: /* Branch through zero */
       {  ARMword oldpsr = ARMul_GetCPSR(state) ;
          ARMul_SetCPSR(state, (oldpsr & 0xffffffc0) | 0x13) ;
          ARMul_SetSPSR(state, SVC32MODE, oldpsr) ;
          state->Reg[14] = 0;
          goto TidyCommon;
       }

    case 0x98: /* Error */
       {  ARMword errorp = state->Reg[0],
                  regp = state->Reg[1];
          unsigned i;
          ARMword errorpsr = ARMul_ReadWord(state, regp + 16*4);
          for (i = 0; i < 15; i++)
            ARMul_SetReg(state,errorpsr,i,ARMul_ReadWord(state, regp + i*4L)) ;
          state->Reg[14] = ARMul_ReadWord(state, regp + 15*4L);
          state->Reg[10] = errorp;
          ARMul_SetSPSR(state,state->Mode,errorpsr) ;
          OSptr->ErrorP = errorp;
          goto TidyCommon;
       }

    case 0x94: /* Data abort */
       {  ARMword addr = state->Reg[14] - 8;
          ARMword cpsr = ARMul_GetCPSR(state) ;
          if (ARM26BITMODE)
             addr = addr & 0x3fffffc ;
          ARMul_SetCPSR(state,ARMul_GetSPSR(state,cpsr)) ;
          UnwindDataAbort(state, addr);
          if (addr >= FPESTART && addr < FPEEND) { /* in the FPE */
             ARMword sp, spsr ;
             unsigned i ;

             sp = state->Reg[13] ;
             state->Reg[13] += 64 ; /* fix the aborting mode sp */
             state->Reg[14] = ARMul_ReadWord(state,sp + 60) ; /* and its lr */
             spsr = ARMul_GetSPSR(state,state->Mode) ;
             state->Mode = ARMul_SwitchMode(state, state->Mode, spsr);
             for (i = 0 ; i < 15 ; i++) {
                ARMul_SetReg(state,spsr,i,ARMul_ReadWord(state,sp)) ;
                sp += 4 ;
                }
             ARMul_SetCPSR(state,cpsr) ;
             state->Reg[14] = ARMul_ReadWord(state,sp) + 4 ; /* botch it */
             ARMul_SetSPSR(state,state->Mode,spsr) ;
             }
          else
             ARMul_SetCPSR(state,cpsr) ;

          /* and fall through to correct r14 */
       }
    case 0x95: /* Address Exception */
       state->Reg[14] -= 4;
    case 0x91: /* Undefined instruction */
    case 0x92: /* SWI */
    case 0x93: /* Prefetch abort */
    case 0x96: /* IRQ */
    case 0x97: /* FIQ */
       state->Reg[14] -= 4;
    TidyCommon:
       if (state->VectorCatch & (1 << (number - 0x90))) {
          ARMul_SetR15(state, state->Reg[14] + 8) ; /* the 8 is the pipelining the the RDI will undo */
          ARMul_SetCPSR(state,ARMul_GetSPSR(state,ARMul_GetCPSR(state))) ;
          if (number == 0x90)
             state->EndCondition = 10 ; /* Branch through Zero Error */
          else
             state->EndCondition = (unsigned)number - 0x8f;
          state->Emulate = FALSE ;
          }
       else {
          ARMword sp = state->Reg[13];
          ARMul_WriteWord(state, sp - 4, state->Reg[14]);
          ARMul_WriteWord(state, sp - 8, state->Reg[12]);
          ARMul_WriteWord(state, sp - 12, state->Reg[11]);
          ARMul_WriteWord(state, sp - 16, state->Reg[10]);
          state->Reg[13] = sp - 16;
          state->Reg[11] = ADDRSOFHANDLERS + 8 * (number - 0x90);
          }
       return(TRUE);

/* SWI's 0x8x pass an abort of type x to the debugger if a handler returns */

    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87: case 0x88:
       {  ARMword sp = state->Reg[13];
          state->Reg[10] = ARMul_ReadWord(state, sp);
          state->Reg[11] = ARMul_ReadWord(state, sp + 4);
          state->Reg[12] = ARMul_ReadWord(state, sp + 8);
          state->Reg[14] = ARMul_ReadWord(state, sp + 12);
          state->Reg[13] = sp + 16;
          ARMul_SetR15(state, state->Reg[14] + 8) ; /* the 8 is the pipelining the the RDI will undo */
          ARMul_SetCPSR(state,ARMul_GetSPSR(state,ARMul_GetCPSR(state))) ;
          if (number == 0x80)
             state->EndCondition = 10 ; /* Branch through Zero Error */
          else
             state->EndCondition = (unsigned)number - 0x7f;
          state->Emulate = FALSE ;
          return(TRUE);
       }

    default :
          state->Emulate = FALSE ;      
       return(FALSE) ;
    }
#endif
#endif
 }
#endif

#ifndef NOOS
#ifndef ASIM

/***************************************************************************\
* The emulator calls this routine when an Exception occurs.  The second     *
* parameter is the address of the relevant exception vector.  Returning     *
* FALSE from this routine causes the trap to be taken, TRUE causes it to    *
* be ignored (so set state->Emulate to FALSE!).                             *
\***************************************************************************/

unsigned ARMul_OSException(ARMul_State *state, ARMword vector, ARMword pc)
{ /* don't use this here */
 return(FALSE) ;
}

#endif

/***************************************************************************\
*                            Unwind a data abort                            *
\***************************************************************************/

static void UnwindDataAbort(ARMul_State *state, ARMword addr)
{
  ARMword instr = ARMul_ReadWord(state, addr);
  ARMword rn = BITS(16, 19);
  ARMword itype = BITS(24, 27);
  ARMword offset;
  if (rn == 15) return;
  if (itype == 8 || itype == 9) {
    /* LDM or STM */
    unsigned long regs = BITS(0, 15);
    offset = 0;
    if (!BIT(21)) return; /* no wb */
    for (; regs != 0; offset++)
      regs ^= (regs & -regs);
    if (offset == 0) offset = 16;
  } else if (itype == 12 ||              /* post-indexed CPDT */
             (itype == 13 && BIT(21))) { /* pre_indexed CPDT with WB */
    offset = BITS(0, 7);
  } else
    return;

  if (BIT(23))
    state->Reg[rn] -= offset * 4;
  else
    state->Reg[rn] += offset * 4;
}

/***************************************************************************\
*           Copy a string from the debuggee's memory to the host's          *
\***************************************************************************/

static void getstring(ARMul_State *state, ARMword from, char *to)
{do {
    *to = (char)ARMul_ReadByte(state,from++) ;
    } while (*to++ != '\0') ;
 }

#endif /* NOOS */
