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
 * Comments about this software should be directed to udi@amd.com. If access
 * to electronic mail isn't available, send mail to:
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 * DOS386 changes were merged into:
 *       $Id: dos2udip.c,v 1.2 1996/11/23 04:11:12 niklas Exp $
 *       $Id: @(#)dos2udip.c	2.11, AMD
 */

	/* TIPIPCId components */
#define TIPIPCIdCompany	0x0001	/* Company ID from AMD, others should change this */
#define TIPIPCIdVersion	0x124	/* Version */
#ifdef DOS386
#define TIPIPCIdProduct	0x3	/* Product ID for DOS386 IPC */
#else
#define TIPIPCIdProduct	0x2	/* Product ID for non-DOS386 IPC */
#endif

#include <stdio.h>
#include <dos.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include <udiproc.h>
#include <udidos.h>
static FILE *      fpstdout = stdout;	/* where we write the information */

#ifndef DOS386
#pragma check_stack( off )
#pragma check_pointer( off )

	/********************************************************
	 * In non-DOS386 mode, a standard C PTR is just a far real ptr
	 * so FARCPTR_TO_REALPTR and REALPTR_TO_FARCPTR are pass-thru
	 ********************************************************/
#define FARCPTR_TO_REALPTR(p) p
#define REALPTR_TO_FARCPTR(p) p

#define IPCFar far	/* qualifier for pointer that can reach the real data used by IPC */
#define REALNULL NULL
typedef void far * FARCPTR;

#else 
#include "malloc.h"
#include "alloca.h"
#include "pharlap.h"
#include "realcopy.h"

#define IPCFar _far	/* qualifier for pointer that can reach the real data used by IPC */
#define REALNULL (REALPTR) 0
typedef void _far * FARCPTR;


	/********************************************************
	 * In DOS386 protected mode, we have two types of pointers, near and far
	 * near is a 32-bit pointer, ie a 32-bit offset from DS.
	 * far is a 48-bit pointer, with an explicit segment register.
	 * We want to be able to convert real mode pointers (16-bit seg, 16-bit ofst)
	 * into these near and far protected mode pointers and vice versa.
	 *
	 * It is always possible to convert a real mode pointer to a far prot ptr.
	 * (Pharlap provides an explicit segment that maps to the low 1 meg of memory).
	 ********************************************************/
FARCPTR  REALPTR_TO_FARCPTR(REALPTR p);

	/********************************************************
	 * The ability to convert from a real mode pointer to a near protected
	 * pointer depends on being able to map converntional memory onto the
	 * end of our data segment.  This is NOT possible under DPMI 0.90
	 * If we're not under DPMI 0.90,
	 * REALPTR_TO_NEARCPTR takes a real ptr, and returns its offset
	 * in the SS_DATA segment (using the fact that the 1 meg of real
	 * memory was mapped to the SS_DATA by dx_map_physical).
	 *
	 ********************************************************/
#define REALPTR_TO_NEARCPTR(rp) ((void *)(&conventional_memory[LINEARIZE(rp)]))


	/**********************************************************
	 *  LINEARIZE converts a segment:ofst pointer into a linear
	 *  addr between 0 and 1meg
	 *********************************************************/
#define LINEARIZE(rp) ((RP_SEG(rp)<<4) + RP_OFF(rp))

	/********************************************************
	 * FARCPTR_TO_REALPTR converts a far protected ptr to a real ptr.
	 * Naturally, only certain protected pointers can be converted
	 * into real pointers (they must map to something in the
	 * first 1 meg of memory).  If it can't be converted, it's
	 * a fatal error.  This is a routine rather than a macro.
	 * If we need to convert a near prot ptr to a real ptr,
	 * this can be done by simply casting it to a far
	 * 
	 ********************************************************/
REALPTR FARCPTR_TO_REALPTR(FARPTR p);

extern USHORT GetCS();
extern USHORT GetDS();


#endif   /* DOS386 */

/****************** External Prototypes *****************************/

extern void TIPPrintUsage(char *arg);

#ifndef DOS386
extern UDIError UDIASMDisconnect UDIParams((
  UDISessionId		Session,		/* In */
  UDIBool		Terminate,		/* In */
  DOSTerm          	far *TermStruct 	/* In - not seen in UDIP */
  ));
extern UDIError UDIASMConnect UDIParams((
  char			*Configuration,		/* In */
  UDISessionId		*Session,		/* Out */
  DOSTerm          	far *TermStruct 	/* In - not seen in UDIP */
  ));

#endif

/****************** Internal Prototypes *****************************/

UDIError UDICCapabilities UDIParams((
  UDIUInt32	*TIPId,			/* Out */
  UDIUInt32	*TargetId,		/* Out */
  UDIUInt32	DFEId,			/* In */
  UDIUInt32	DFE,			/* In */
  UDIUInt32	*TIP,			/* Out */
  UDIUInt32	*DFEIPCId,		/* Out */
  UDIUInt32	*TIPIPCId,		/* Out */
  char		*TIPString		/* Out */
  ));

static unsigned short GetPSP( void );
static void SetPSP( unsigned short PSPSegment );
static unsigned int ComputeTSRSize(void *topofstack);
static void SetupEnvironment(void);
static void TerminateTIP UDIParams((DOSTerm           IPCFar *TermStruct));


/****************** External and Static Data *****************************/
static int ConnectCount;

#ifdef DOS386

char    *conventional_memory;	/* pointer to first byte of conventinal memory */
                                /* if 0, then conventional mem not mapped */
USHORT  our_tsr_psp;		/* TIP's original PSP */
USHORT  dos_ext_psp;		/* Dos extender PSP (TIP's parent) */
extern  REALPTR  call_prot;	/* These are in the module dostip.asm */
extern  USHORT   code_selector;
extern  USHORT   data_selector;
extern  USHORT	segregblock[4];
extern  int end_real;		/* marks end of stuff that must be placed in low mem */
int * stack_table[3];		/* used when we need to get a new stack frame
				 * to establish C context on each UDI call 
				 * but only if conventional memory didn't map */
REALPTR	 real_basep;		/* returned by realcopy */
FARPTR	 prot_basep;		/* returned by realcopy */
USHORT	 rmem_adrp;		/* returned by realcopy */

extern char TIPName[];			/* in DOS386, defined in rmdata in dosdfe.asm */
extern struct UDIVecRec TIPVecRec;	/* in DOS386, defined in rmdata in dosdfe.asm */



#else	/* non-DOS386 static and external data */

char TIPName[ FILENAME_MAX ];		/* in non-386 version, TIPName defined right here */
struct UDIVecRec TIPVecRec = {		/* in non-386 version, TIPVecRec defined right here */
    UDIDOSTIPRecognizer,	/* Initialized in main */
    NULL,			/* Pointer to next TIP */
    NULL,			/* Pointer to previous TIP */
    TIPName,			/* Name of the executable we were loaded as */
    UDIASMConnect,
    UDIASMDisconnect,
    UDISetCurrentConnection,
    UDICCapabilities,
    UDIGetErrorMsg,
    UDIGetTargetConfig,
    UDICreateProcess,
    UDISetCurrentProcess,
    UDIDestroyProcess,
    UDIInitializeProcess,
    UDIRead,
    UDIWrite,
    UDICopy,
    UDIExecute,
    UDIStep,
    UDIStop,
    UDIWait,
    UDISetBreakpoint,
    UDIQueryBreakpoint,
    UDIClearBreakpoint,
    UDIGetStdout,
    UDIGetStderr,
    UDIPutStdin,
    UDIStdinMode,
    UDIPutTrans,
    UDIGetTrans,
    UDITransMode
   };
#endif

struct UDIVecRec IPCFar * pTIPVecRec;	/* pointer to TIPVecRec */
					/* in DOS386, this points to real memory */
static RealUDIVecRecPtr IPCFar * UDIVecP;

static int loaded_from_exp_file = 0;


void do_exit(int errcode)
{
  /* this routine normally just calls exit but in the special case
   * of DOS386 mode AND we were loaded from a .exp file, then we want
   * to exit in a different way by calling exp_return
   */
#ifdef DOS386
extern void _exp_return(int err);
    if (loaded_from_exp_file)
	_exp_return(errcode);
    else
#endif
        /* normal non-DOS386 and non-exp_file exit */
	exit(errcode);
}


void do_dos_keep(int errcode, int tsrsize)
{
  /* similar logic to do_exit above, but this time for dos_keep
   */
#ifdef DOS386
extern void _exp_return(int err);
    if (loaded_from_exp_file)
	_exp_return(errcode);
    else
#endif
        /* normal non-DOS386 and non-exp_file dos_keep */
        _dos_keep( 0, tsrsize );
}

void get_tip_name(int argc, char *argv[])
{
  /* This routine normally gets the Tipname from argv[1], but
   * in the special case of DOS386 and loaded as an exp file,
   * it gets the name from the stack
   */

#ifdef DOS386
    extern  char * _top;

    if ((GetCS() & 0xfffc) != SS_CODE) {
        /* a CS that is not SS_CODE indicates that we were
	   loaded as a .exp file.  In that case, we don't
	   want to exit or do a TSR, instead we want to return
	   back to the DFE using _exp_return.
        */
      	loaded_from_exp_file = TRUE;
	strcpy(TIPName, _top+16);
	return;
    }
#endif
    
    if ((argc!= 2)  || (argv[1][0] == '-')) {
	TIPPrintUsage(argv[1]);
	do_exit( 1 );
    }

    strcpy( TIPName, argv[1] );
}


#ifdef DOS386
REALPTR FARCPTR_TO_REALPTR(FARCPTR p)	/* converts a FAR PROT ptr to a REALPTR */
{
REALPTR  dummyrp;
int   err;

		/* returns a real mode pointer given a prot mode pointer p */
	err = _dx_toreal(p, 0, &dummyrp);
	if (err) {
	   printf("Fatal Error _dx_toreal(%04x:%08x)\n", FP_SEG(p), FP_OFF(p));
	   do_exit(err);
	}
	else
	   return(dummyrp);
	
}


FARCPTR  REALPTR_TO_FARCPTR(REALPTR rp)
{
FARCPTR   dummyfp;
	FP_SET(dummyfp, LINEARIZE(rp), SS_DOSMEM);
	return(dummyfp);
}

/*****************
 * Routine used to create and initialize a stack for C context stack switching
 * (used only when conventional memory can't be mapped
 ****************/
static void create_stack(int stack_index, int size_in_bytes)
{
int *p;
int  index_to_last_int;

	/* malloc appropriate size and point stack_table entry to second last word */
    p = (int *)malloc(size_in_bytes);
    if (p == 0) {
        printf("\nTIP: unable to malloc stacks\n");
        do_exit(1);
    }
    index_to_last_int =  (size_in_bytes/sizeof(int)) - 1;
    stack_table[stack_index] = &p[index_to_last_int-1];

	/* and set last word to 0 (marked as free) */
	/* set second last word to stack size (used for alloca checking) */
    p[index_to_last_int-1]   = size_in_bytes-8;
    p[index_to_last_int] = 0;
}
#endif



static void TerminateTIP UDIParams((
  DOSTerm          	IPCFar *TermStruct 	/* In - not seen in UDIP */
  ))
{
    /* Delink ourselves from the linked list of TIPs */
    if (pTIPVecRec->Next != REALNULL)
	((struct UDIVecRec IPCFar *)REALPTR_TO_FARCPTR(pTIPVecRec->Next))->Prev = pTIPVecRec->Prev;
    if (pTIPVecRec->Prev != REALNULL)
	((struct UDIVecRec IPCFar *)REALPTR_TO_FARCPTR(pTIPVecRec->Prev))->Next = pTIPVecRec->Next;
    else
	*UDIVecP = pTIPVecRec->Next;	/* no previous TIP, set the interrupt vector
					   to point to our Next TIP */

#ifdef DOS386
{
    if (loaded_from_exp_file) 	/* if we were loaded from an exp file, skip all this PSP stuff */
	return;

    /* Under DOSEXT, our PSP is parented by the DOSEXTENDER's PSP */
    /* We want to modify the DOSEXT's PSP to point to the DFE info */
REALPTR ptr_dos_ext_psp_parent;
REALPTR ptr_dos_ext_psp_termaddr;

    /* Set the dos_ext_psp's Parent PSP to the current PSP (ie, the DFE PSP)*/
    RP_SET(ptr_dos_ext_psp_parent,0x16, dos_ext_psp);
    *((USHORT _far *)(REALPTR_TO_FARCPTR(ptr_dos_ext_psp_parent))) = GetPSP();

    /* Set the dos_ext_psp's Terminate address to reasonable address in
       current PSP (DFE)'s program space */
    RP_SET(ptr_dos_ext_psp_termaddr,0xa, dos_ext_psp);
    *((ULONG _far *)(REALPTR_TO_FARCPTR(ptr_dos_ext_psp_termaddr))) = (ULONG) TermStruct->TermFunc;
}
#else
    /* Set our TSR's PSP's Parent PSP to the current PSP */
    fflush(fpstdout);

    *(unsigned _far *)(((long)_psp << 16) + 0x16) = GetPSP();

    /* Set our TSR's PSP's Terminate address to reasonable address in
       current PSP's program space */
    /*(void _far (_far *) (void))(((long)_psp << 16) + 0xa) = ExitAddr;*/
    *(void (_far *(_far *))(void))(((long)_psp << 16) + 0xa) =
	TermStruct->TermFunc;
#endif

    /* Change DOS's notion of what the current PSP is to be our TSR's PSP */
#ifdef DOS386
    SetPSP(our_tsr_psp); 
	/* Under Dosext, termination will chain back from our_psp to DOSEXT PSP */
	/* and then back to the DFE (since we modified the DOSEXT PSP above)    */
#else
    SetPSP(_psp );
#endif

    /* Terminate the program by using DOSTerminate 0x21/0x4c. Execution
       will resume at the Terminate address set above with ALL REGISTERS
       UNKNOWN especially SS:SP, DS, ES, etc */
    bdos( 0x4c, 0, 0 );
    }

UDIError UDICConnect UDIParams((
  char			*Configuration,		/* In */
  UDISessionId		*Session,		/* Out */
  DOSTerm          	IPCFar *TermStruct 	/* In - not seen in UDIP */
  ))
{
    UDIError err;
 
    if ((err = UDIConnect( Configuration, Session )) <= UDINoError)
	ConnectCount++;

    if (ConnectCount == 0) {	/* Terminate the unused TIP */
	/* Save the error status in the TermStruct */
	TermStruct->retval = err;

	TerminateTIP( TermStruct );	/* Never returns */
	}

    return err;
    }

UDIError UDICDisconnect UDIParams((
  UDISessionId		Session,		/* In */
  UDIBool		Terminate,		/* In */
  DOSTerm          	IPCFar *TermStruct 	/* In - not seen in UDIP */
  ))
{
    UDIError err;

    /* Disconnect via the real TIP */
    if ((err = UDIDisconnect( Session, Terminate )) == UDINoError)
	ConnectCount--;

    if (Terminate != UDIContinueSession && ConnectCount == 0) {
	/* Terminate the unused TIP */
	/* Save the error status in the TermStruct */
	TermStruct->retval = err;

	TerminateTIP( TermStruct );	/* Never returns */
	}

    return err;
    }

UDIError UDICCapabilities UDIParams((
  UDIUInt32	*TIPId,			/* Out */
  UDIUInt32	*TargetId,		/* Out */
  UDIUInt32	DFEId,			/* In */
  UDIUInt32	DFE,			/* In */
  UDIUInt32	*TIP,			/* Out */
  UDIUInt32	*DFEIPCId,		/* Out */
  UDIUInt32	*TIPIPCId,		/* Out */
  char		*TIPString		/* Out */
  ))
{
    UDIError err;

    err = UDICapabilities( TIPId, TargetId, DFEId, DFE, TIP,
				DFEIPCId, TIPIPCId, TIPString );

    *TIPIPCId = (((UDIUInt32)TIPIPCIdCompany) << 16) |
			(TIPIPCIdProduct << 12) | TIPIPCIdVersion;

    return err;
    }


static RealUDIVecRecPtr IPCFar * AllocateIntVect()
{
    RealUDIVecRecPtr IPCFar * VecP;

    /* Try and find a vector that is unused */
    for (VecP =  (RealUDIVecRecPtr IPCFar *)(REALPTR_TO_FARCPTR(0x60*4));
	 VecP <= (RealUDIVecRecPtr IPCFar *)(REALPTR_TO_FARCPTR(0x66*4));
	 VecP++) {
	if (*VecP == REALNULL)
	    return VecP;
	}

    return NULL;
    }

static RealUDIVecRecPtr IPCFar * FindIntVect()
{
    RealUDIVecRecPtr IPCFar * VecP;
    union rec recognizer;

    InitRecognizer( &recognizer );

    /* Try and find a vector that matches the passed in recognizer */
    for (VecP =  (RealUDIVecRecPtr IPCFar *)(REALPTR_TO_FARCPTR(0x60*4));
	 VecP <= (RealUDIVecRecPtr IPCFar *)(REALPTR_TO_FARCPTR(0x66*4));
	 VecP++) {
	if ((*VecP != REALNULL) && ((struct UDIVecRec IPCFar *) REALPTR_TO_FARCPTR(*VecP))->recognizer.l == recognizer.l)
	    	return VecP;
    }
     
    return NULL;
}

static void SetupEnvironment(void)
{
#ifndef DOS386
		/* if not DOS386, nothing to do except set up the
		   pointer to TIPVecRec
		   */
        pTIPVecRec = &TIPVecRec;

#else		/* setup code for DOS386 */
FARPTR  dummyfp;
REALPTR dummyrp;
ULONG   dummyint;
REALPTR IPCFar *p;
REALPTR ptr_dos_ext_psp;
int     err;
int	i;

	/**************************************************************
	 * There are some initialization things that we might as well do before
	 * we do the realcopy down below.  Here we do some initialization
	 * of TIPVecRec and the code_selector and data_selector and call_prot
	 * routine address that are then used by the real mode code.
	 ****************************************************************/

	_dx_rmlink_get(&call_prot, &dummyrp, &dummyint, &dummyfp);
	code_selector = GetCS();
	data_selector = GetDS();
	for (i=0; i<4; i++)
	    segregblock[i] = data_selector;
	
	/******************************************************
	 * Map first 1 meg of physical memory into our ds: address space
	 * This is 256 4K pages starting at physical address 0.
	 * The pointer conventional_memory is its mapped offset in our data space
	 * If this mapping cannot be done (because we are running under DPMI 0.90)
	 * then we will have to access dos memory using far pointers and do some
	 * copies of data down in the d386 routines.
	 ********************************************************/ 
	err = _dx_map_phys(data_selector, (ULONG) 0, (ULONG) 256, (ULONG *)&conventional_memory);
	if (err) 
	    conventional_memory = NULL;

#ifdef DEBUG
        if (err) 
	    printf("TIP: Unable to map conventional memory %d\n", err);
	else 
	    printf("TIP: Successfully mapped conventional memory\n");
#endif

        if (!conventional_memory) {
	   /* mapping conventional memory did not work */
	   /* need to set up stacks to switch to at UDI call time */
	    create_stack(0, 64000);
	    create_stack(1, 10000);
	    stack_table[2] =  0;	/* end of list */
	  }

        /* do a realcopy to copy all the things that must be reachable
	   * from real mode into a real mode segment.  For simplicity,
	   * we just always assume that REALBREAK might not work.
	   * This is only used at TIP INIT time and the performance impact is negligent.
	   */
         err = realcopy(0,    		  /* real mode stuff was linked first */
                        end_real,         /* where the real mode stuff ends */
			&real_basep,
			&prot_basep,
			&rmem_adrp);

        if (err) {
	        printf("\nTIP: realcopy call failed;\n");
                printf(  "     Probable cause: insufficient free conventional memory.\n");
                do_exit(1);
        }


        /* The prot_basep that was returned above must now be used
	   to access from protected mode the data elements that were
	   copied above.  In particular, we create a pointer to the
	   copied TIPVecRec and use that.
	 */
        pTIPVecRec = (struct UDIVecRec IPCFar *) (prot_basep + (ULONG) &TIPVecRec);

         

	/**************************************************************
	 * The real_basep that was returned from realcopy must be used as
	 * the code segment pointer of all the real mode routines.
	 * and so must be patched into TIPVecRec.
	 * real_basep is returned by realcopy such that the offset parts
	 * (as assembled in in the module dostip.asm) can remain unchanged.
	 * So we just need to patch the real_basep seg into each of those pointers
	 ***************************************************************/
	for (p = (REALPTR IPCFar *)&pTIPVecRec->exeName;
	     p<= (REALPTR IPCFar *)&pTIPVecRec->UDITransMode; p++) {
		RP_SET(*p, RP_OFF(*p), RP_SEG(real_basep)); 
	}

	/*****************************************************
	   Store our PSP (real segment) away for later termination
	   and also the dos extender's PSP (our parent).  We get this by 
	   building a real pointer with seg = our_tsr_psp, ofst = 0x16,
	   and then derefencing that
	*****************************************************/
	our_tsr_psp = GetPSP();
	RP_SET(ptr_dos_ext_psp, 0x16, our_tsr_psp);
	dos_ext_psp = *((USHORT _far *)(REALPTR_TO_FARCPTR(ptr_dos_ext_psp)));

#endif	/* end of DOS386 setup code */
}


static unsigned int ComputeTSRSize(void *topofstack)
{
#ifndef DOS386
	/* Real mode program, compute program size */
	/* Huge pointers force compiler to do segment arithmetic for us. */
static char _huge *tsrstack;
static char _huge *tsrbottom;
    /* Initialize stack and bottom of program. */
    tsrstack = (char huge *)topofstack;
    FP_SEG( tsrbottom ) = _psp;
    FP_OFF( tsrbottom ) = 0;

    /* Program size is:
     *	   top of stack
     *	 - bottom of program (converted to paragraphs) (using huge math)
     *	 + one extra paragraph
     */
    return((unsigned int) (((tsrstack - tsrbottom) >> 4) + 1));
#else
	/*********************
	 In DOS386 mode, the TSR size consists of the real memory that
	 is used by the Pharlap DOS extender and the small amount of real memory
	 used by UDI.  The number 6400 seems to be a good guess for now.
	 This might have to be adjusted with newer versions of Dos extender, etc.
	 I wonder if there is some way to compute this number accurately.
         **********************/
	return(6400);	/* our best guess for now */
#endif
}

main(int argc, char *argv[])
{
    unsigned tsrsize;

    get_tip_name(argc, argv);	/* get name from argv or whereever */

#ifdef TSRDEBUG
    {
    int i;
    printf( "Invoked with %d arguments\n", argc );
    for (i = 0; i < argc; i++)
	printf( "%s ", argv[i] );
    printf( "\n" );
    }
#endif

    InitRecognizer(&TIPVecRec.recognizer );

    SetupEnvironment();		/* do some setup specific to DOS or DOS386 */


    /* See if the interrupt vector has already been selected for us */
    if ((UDIVecP = FindIntVect()) == NULL) {
	if ((UDIVecP = AllocateIntVect()) == NULL)
	    return -1;	/* No interrupt vectors available */
	}
    else {	/* Interrupt vector already allocated */
	pTIPVecRec->Next = *UDIVecP;		/* always store a real ptr there */
	((struct UDIVecRec IPCFar *) REALPTR_TO_FARCPTR(pTIPVecRec->Next))->Prev = FARCPTR_TO_REALPTR(pTIPVecRec);
	}

    *UDIVecP = FARCPTR_TO_REALPTR(pTIPVecRec);

    tsrsize = ComputeTSRSize(&argv);	/* pass it pointer to argv (top of stack) */

    /* We are now ready to support DFEs. If we wish to debug back-ends,
       though, we are probably running CodeView with the TIP right now
       and don't want to really TSR because CV will shut down at that
       point. Instead, let's spawn a new DOS shell from which we can
       start a DFE (after setting a breakpoint in the TIP somewhere).
    */
#ifdef TSRDEBUG
    system( getenv( "COMSPEC" ) );
#else
    do_dos_keep(0, tsrsize);
#endif

    return 0;
}


#define GET_PSP_DOS2	0x51
#define GET_PSP_DOS3	0x62
#define SET_PSP 	0x50

static unsigned short GetPSP( void )
{
    union REGS regs;

    if (_osmajor == 2)
	return 0;
#ifdef DOS386
    regs.h.ah = GET_PSP_DOS2;	/* Phar Lap requires we use this to get real segment */
#else
    regs.h.ah = GET_PSP_DOS3;
#endif
    intdos( &regs, &regs );
    return regs.x.bx;
}

static void SetPSP( unsigned short PSPSegment )
{
    union REGS regs;

    regs.h.ah = SET_PSP;
    regs.x.bx = PSPSegment;
    intdos( &regs, &regs );
}


#ifdef DOS386
/*============================ DOS386 glue routines ====================================*/

/****************************************************************
 * In DPMI Compatibility mode, when we get to this point, the only
 * thing that is on the stack is the saved far stack pointer (which actually
 * points back to the real mode stack).   Remember in pmstub in dostip.asm,
 * we switched stack pointers so that SS = DS for C level requirements.
 *
 * The INCOMING_PARAMS macro defines a packed structure which expresses what the
 * real mode stack really looks like when we get to each dos386 glue routine. 
 * The STACK_PAD is all the extra stuff that was on the stack because of the switching
 * from real to protected mode, etc.
 * The packed structure can be used to express where things really are on the stack
 * because the DFE's MSC compiler will push things differently from the hc386 compiler.
 ********************************************************************/
typedef _packed struct {
	FARPTR  ret_to_dosext;
	USHORT	zero_word;
	USHORT  saved_di;
	USHORT  saved_si;
	USHORT  saved_bp;
	USHORT  saved_ds;
	ULONG	ret_to_dfe;
} STACK_PAD;

/* The following macro defines the packed structure for the incoming parameters
 * including the STACK_PAD stuff noted above.  It is used by those few d386_
 * routines that do not need converted pointers to avoid non-use warnings
 */
#define INCOMING_PARAMS_NO_PTR(params)  \
	_packed struct {		\
	    STACK_PAD padding;        	\
	    params			\
	} _far *in  = rm_stk_ptr; 	\


/* The following macro defines the packed structure for the incoming parameters
 * (see above) and also defines a local structure for storing the converted local
 * pointers.  Most d386_ routines use this macro.
 */
#define INCOMING_PARAMS(params)   	\
	INCOMING_PARAMS_NO_PTR(params) 	\
	struct {			\
	    params			\
	    int dummy;     /* to avoid warnings and for local count */          \
	} local ;    /* local structure for holding converted pointers */  \
	int  stackspace = stacksize;    \



/**************************************************************
 * The following macros handle the creation of near C pointers from real pointers
 * so that the real UDI routines can be called with near C pointers.
 * Different macros are called for IN pointers vs. OUT pointers and
 * for PREPROCESSING (before the real UDI call) and POSTPROCESSING (cleanup
 * after returning from the real UDI call).
 *
 * If conventional_memory has been mapped, the following happens
 *	PREPROCESS (IN or OUT ptr) sets local.var pointer to the mapped pointer
 *				   nothing to copy so count is ignored
 *	POSTPROCESS		   nothing to do 
 *
 * If conventional_memory has not been mapped, then
 *	PREPROCESS (IN ptr) 	   does alloca of count to get local pointer
 *				   copies data into local allocated area
 *	PREPROCESS (OUT ptr) 	   does alloca of count to get local pointer
 *				   no copy of data yet.
 *	POSTPROCESS (OUT ptr)	   copies data from local allocated area back to real mem
 * 
 * Note that a few UDI routines have pointers that are both IN and OUT
 */

	/* the following is used in a couple of places in the macros */
#define ALLOC_LOCAL(var, count) \
	if ((stackspace -= count) <= 500) return(UDIErrorIPCLimitation); \
	local.var = alloca(count); 

#define INPTR_PREPROCESS_COUNT(var,count)  \
    if (conventional_memory) { \
	local.var = REALPTR_TO_NEARCPTR(in->var); \
    } \
    else { \
	local.dummy = count;	/* avoid double evaluation if count is expression */   \
	ALLOC_LOCAL(var, local.dummy); \
	movedata(SS_DOSMEM, LINEARIZE(in->var), data_selector, (unsigned int) local.var, local.dummy); \
    }
 

#define INPTR_PREPROCESS(var) INPTR_PREPROCESS_COUNT(var, sizeof(*(in->var)))

#define OUTPTR_PREPROCESS_COUNT(var,count)  \
    if (conventional_memory)  \
	local.var = REALPTR_TO_NEARCPTR(in->var); \
    else { \
	ALLOC_LOCAL(var,count); \
    }

#define OUTPTR_PREPROCESS(var) OUTPTR_PREPROCESS_COUNT(var, sizeof(*(in->var)))

#define OUTPTR_POSTPROCESS_COUNT(var,count)  \
    if (!conventional_memory) {\
	movedata(data_selector, (unsigned int)local.var, SS_DOSMEM, LINEARIZE(in->var), count); \
    }

#define OUTPTR_POSTPROCESS(var) OUTPTR_POSTPROCESS_COUNT(var, sizeof(*(in->var)))



/* The following routine computes the length of a string that
 * is pointed to by a real pointer.  This is only needed when
 * we cannot map real mode memory at the end of the DS.
 */
int realptr_strlen(REALPTR rp) 
{
char _far *farp;
char _far *start;

    farp = (char _far *) REALPTR_TO_FARCPTR(rp);   /* need to use a far c ptr */
    start = farp;
    while (*farp++);       /* advance until a 0 located */
    return(FP_OFF(farp) - FP_OFF(start));
}

/*========================  Glue Routines ============================================*/


UDIError d386_UDIConnect (void _far * rm_stk_ptr, int stacksize)
{ 
INCOMING_PARAMS(
  char		*Configuration;		/* In  */
  UDISessionId	*Session;		/* Out */
  DOSTerm           *TermStruct; 	/* In - not seen in UDIP */
)
UDIError err;

    INPTR_PREPROCESS_COUNT(Configuration, realptr_strlen((REALPTR)(in->Configuration))+1);
    OUTPTR_PREPROCESS(Session);
 
    err = UDICConnect(	   /* for UDIConnect, special case, call UDICConnect in dos2udip.c */
	local.Configuration,
	local.Session,
	REALPTR_TO_FARCPTR((REALPTR)in->TermStruct)
    );

    OUTPTR_POSTPROCESS(Session);
    return(err);
}

UDIError d386_UDIDisconnect (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDISessionId	Session;		/* In */
  UDIBool	Terminate;
  DOSTerm           *TermStruct; 		/* In - not seen in UDIP */
)
UDIError err;

    local.dummy = 0;                    /* avoids warning */
    err = UDICDisconnect(		/* need to call UDICDisconnect */
	in->Session,
	in->Terminate,
	REALPTR_TO_FARCPTR((REALPTR)in->TermStruct)
    );
    return(err);
		
}


UDIError d386_UDISetCurrentConnection  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS_NO_PTR(
  UDISessionId	Session;		/* In */
)
	return(UDISetCurrentConnection(in->Session));
}


UDIError d386_UDICapabilities  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIUInt32	*TIPId;			/* Out */
  UDIUInt32	*TargetId;		/* Out */
  UDIUInt32	DFEId;			/* In */
  UDIUInt32	DFE;			/* In */
  UDIUInt32	*TIP;			/* Out */
  UDIUInt32	*DFEIPCId;		/* Out */
  UDIUInt32	*TIPIPCId;		/* Out */
  char		*TIPString;		/* Out */
)
UDIError err;

    OUTPTR_PREPROCESS(TIPId);
    OUTPTR_PREPROCESS(TargetId);
    OUTPTR_PREPROCESS(TIP);
    OUTPTR_PREPROCESS(DFEIPCId);
    OUTPTR_PREPROCESS(TIPIPCId);
    OUTPTR_PREPROCESS_COUNT(TIPString, 100);   /* max TIP string? */

    err = UDICCapabilities(		/* another special case call UDICapabilities */
	local.TIPId,
	local.TargetId,
	in->DFEId,
	in->DFE,
	local.TIP,
	local.DFEIPCId,
	local.TIPIPCId,
	local.TIPString
    );

    OUTPTR_POSTPROCESS(TIPId);
    OUTPTR_POSTPROCESS(TargetId);
    OUTPTR_POSTPROCESS(TIP);
    OUTPTR_POSTPROCESS(DFEIPCId);
    OUTPTR_POSTPROCESS(TIPIPCId);
    OUTPTR_POSTPROCESS_COUNT(TIPString, strlen(local.TIPString)+1);

    return(err);
}


UDIError d386_UDIGetErrorMsg  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIError	ErrorCode;		/* In */
  UDISizeT	MsgSize;		/* In */
  char		*Msg;			/* Out */
  UDISizeT	*CountDone;		/* Out */
)
UDIError err;

    OUTPTR_PREPROCESS_COUNT(Msg, in->MsgSize);
    OUTPTR_PREPROCESS(CountDone);
    
    err = UDIGetErrorMsg(
	in->ErrorCode,
	in->MsgSize,
	local.Msg,		/* pointers made local */
	local.CountDone
    );

    OUTPTR_POSTPROCESS_COUNT(Msg, *(local.CountDone)+1);
    OUTPTR_POSTPROCESS(CountDone);
    return(err);
}



UDIError d386_UDIGetTargetConfig (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIMemoryRange *KnownMemory;		/* Out */
  UDIInt	*NumberOfRanges;	/* In/Out */
  UDIUInt32	*ChipVersions;		/* Out */
  UDIInt	*NumberOfChips;		/* In/Out */
)
UDIError err;

    INPTR_PREPROCESS(NumberOfRanges);
    INPTR_PREPROCESS(NumberOfChips);
    OUTPTR_PREPROCESS_COUNT(KnownMemory, *(local.NumberOfRanges) * sizeof(UDIMemoryRange));
    OUTPTR_PREPROCESS_COUNT(ChipVersions, *(local.NumberOfChips) * sizeof(UDIUInt32));

    err = UDIGetTargetConfig(
  	local.KnownMemory,
  	local.NumberOfRanges,
  	local.ChipVersions,
  	local.NumberOfChips
    );

    OUTPTR_POSTPROCESS(NumberOfRanges);
    OUTPTR_POSTPROCESS(NumberOfChips);
    OUTPTR_POSTPROCESS_COUNT(KnownMemory, *(local.NumberOfRanges) * sizeof(UDIMemoryRange));
    OUTPTR_POSTPROCESS_COUNT(ChipVersions, *(local.NumberOfChips) * sizeof(UDIUInt32));

    return(err);
}

UDIError d386_UDICreateProcess (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIPId	*PId;			/* Out */
)
UDIError err;

    OUTPTR_PREPROCESS(PId);

    err = UDICreateProcess(
	local.PId
    );

    OUTPTR_POSTPROCESS(PId);
    return(err);
}

UDIError d386_UDISetCurrentProcess (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS_NO_PTR(
  UDIPId	PId;			/* In */
  )

    return(UDISetCurrentProcess(
	in->PId
    ));
}

UDIError d386_UDIDestroyProcess (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS_NO_PTR(
  UDIPId	PId;			/* In */
  )

    return(UDIDestroyProcess(
	in->PId
    ));
}



UDIError d386_UDIInitializeProcess (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIMemoryRange *ProcessMemory;	/* In */
  UDIInt	NumberOfRanges;		/* In */
  UDIResource	EntryPoint;		/* In */
  CPUSizeT	*StackSizes;		/* In */
  UDIInt	NumberOfStacks;		/* In */
  char		*ArgString;		/* In */
  )
UDIError err;

    INPTR_PREPROCESS_COUNT(ProcessMemory, in->NumberOfRanges * sizeof(UDIMemoryRange));
    INPTR_PREPROCESS_COUNT(StackSizes, in->NumberOfStacks * sizeof(CPUSizeT));
    INPTR_PREPROCESS_COUNT(ArgString, realptr_strlen((REALPTR)(in->ArgString))+1);

    err = UDIInitializeProcess(
	local.ProcessMemory,
	in->NumberOfRanges,
	in->EntryPoint,
	local.StackSizes,
	in->NumberOfStacks,
	local.ArgString
    );

    return(err);
}



UDIError d386_UDIRead (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIResource	From;			/* In */
  UDIHostMemPtr	To;			/* Out */
  UDICount	Count;			/* In */
  UDISizeT	Size;			/* In */
  UDICount	*CountDone;		/* Out */
  UDIBool	HostEndian;		/* In */
  )
UDIError err;

    OUTPTR_PREPROCESS_COUNT(To, in->Count * in->Size);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIRead(
	in->From,
	local.To,
	in->Count,
	in->Size,
	local.CountDone,
	in->HostEndian
    );

    OUTPTR_POSTPROCESS_COUNT(To, *(local.CountDone) * in->Size);
    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}



UDIError d386_UDIWrite  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	From;			/* In */
  UDIResource	To;			/* In */
  UDICount	Count;			/* In */
  UDISizeT	Size;			/* In */
  UDICount	*CountDone;		/* Out */
  UDIBool	HostEndian;		/* In */
  )
UDIError err;

    INPTR_PREPROCESS_COUNT (From, in->Count * in->Size);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIWrite(
	local.From,
	in->To,
	in->Count,
	in->Size,
	local.CountDone,
	in->HostEndian
    );

    OUTPTR_POSTPROCESS(CountDone);

    return(err);

}


UDIError d386_UDICopy (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIResource	From;			/* In */
  UDIResource	To;			/* In */
  UDICount	Count;			/* In */
  UDISizeT	Size;			/* In */
  UDICount	*CountDone;		/* Out */
  UDIBool	Direction;		/* In */
 )
UDIError err;

    OUTPTR_PREPROCESS(CountDone);

    err = UDICopy(
	in->From,
	in->To,
	in->Count,
	in->Size,
	local.CountDone,
	in->Direction
    );

    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}


UDIError d386_UDIExecute (void _far * rm_stk_ptr, int stacksize)
{
/* no incoming parameters */

    return(UDIExecute());
}


UDIError d386_UDIStep  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS_NO_PTR(
  UDIUInt32	Steps;			/* In */
  UDIStepType   StepType;		/* In */
  UDIRange      Range;			/* In */
  )
UDIError err;

    err = UDIStep(
	in->Steps,
	in->StepType,
	in->Range
    );

    return(err);
}



UDIVoid d386_UDIStop   (void _far * rm_stk_ptr, int stacksize)
{
/* no incoming parameters, no return value */
    UDIStop();
}




UDIError d386_UDIWait  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIInt32	MaxTime;		/* In */
  UDIPId	*PId;			/* Out */
  UDIUInt32	*StopReason;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS(PId);
    OUTPTR_PREPROCESS(StopReason);

    err = UDIWait(
	in->MaxTime,
	local.PId,
	local.StopReason
    );

    OUTPTR_POSTPROCESS(PId);
    OUTPTR_POSTPROCESS(StopReason);

    return(err);
}



UDIError d386_UDISetBreakpoint  (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIResource	Addr;			/* In */
  UDIInt32	PassCount;		/* In */
  UDIBreakType	Type;			/* In */
  UDIBreakId    *BreakId;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS(BreakId);
 
    err = UDISetBreakpoint(
	in->Addr,
	in->PassCount,
	in->Type,
	local.BreakId
    );

    OUTPTR_POSTPROCESS(BreakId);

    return(err);
}


UDIError d386_UDIQueryBreakpoint   (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIBreakId	BreakId;		/* In */
  UDIResource	*Addr;			/* Out */
  UDIInt32	*PassCount;		/* Out */
  UDIBreakType	*Type;		/* Out */
  UDIInt32	*CurrentCount;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS(Addr);
    OUTPTR_PREPROCESS(PassCount);
    OUTPTR_PREPROCESS(Type);
    OUTPTR_PREPROCESS(CurrentCount);

    err = UDIQueryBreakpoint(
	in->BreakId,
	local.Addr,
	local.PassCount,
	local.Type,
	local.CurrentCount
    );

    OUTPTR_POSTPROCESS(Addr);
    OUTPTR_POSTPROCESS(PassCount);
    OUTPTR_POSTPROCESS(Type);
    OUTPTR_POSTPROCESS(CurrentCount);

    return(err);
}



UDIError d386_UDIClearBreakpoint (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS_NO_PTR(
  UDIBreakId	BreakId;		/* In */
  )
    return(UDIClearBreakpoint(
	in->BreakId
    ));

}

UDIError d386_UDIGetStdout (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	Buf;			/* Out */
  UDISizeT	BufSize;		/* In */
  UDISizeT	*CountDone;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS_COUNT(Buf, in->BufSize);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIGetStdout(
	local.Buf,
	in->BufSize,
	local.CountDone
    );

    OUTPTR_POSTPROCESS_COUNT(Buf, *(local.CountDone));
    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}


UDIError d386_UDIGetStderr (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	Buf;			/* Out */
  UDISizeT	BufSize;		/* In */
  UDISizeT	*CountDone;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS_COUNT(Buf, in->BufSize);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIGetStderr(
	local.Buf,
	in->BufSize,
	local.CountDone
    );

    OUTPTR_POSTPROCESS_COUNT(Buf, *(local.CountDone));
    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}



UDIError d386_UDIPutStdin (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	Buf;			/* In */
  UDISizeT	Count;			/* In */
  UDISizeT	*CountDone;		/* Out */
  )
UDIError err;

    INPTR_PREPROCESS_COUNT(Buf, in->Count);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIPutStdin(
	local.Buf,
	in->Count,
	local.CountDone
    );

    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}


UDIError d386_UDIStdinMode (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIMode	*Mode;			/* Out */
 )
UDIError err;

    OUTPTR_PREPROCESS(Mode);

    err = UDIStdinMode(
	local.Mode
    );

    OUTPTR_POSTPROCESS(Mode);

    return(err);
}

UDIError d386_UDIPutTrans (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	Buf;			/* In */
  UDISizeT	Count;			/* In */
  UDISizeT	*CountDone;		/* Out */
  )
UDIError err;

    INPTR_PREPROCESS_COUNT(Buf, in->Count);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIPutTrans(
	local.Buf,
	in->Count,
	local.CountDone
    );

    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}


UDIError d386_UDIGetTrans (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIHostMemPtr	Buf;			/* Out */
  UDISizeT	BufSize;		/* In */
  UDISizeT	*CountDone;		/* Out */
  )
UDIError err;

    OUTPTR_PREPROCESS_COUNT(Buf, in->BufSize);
    OUTPTR_PREPROCESS(CountDone);

    err = UDIGetTrans(
	local.Buf,
	in->BufSize,
	local.CountDone
    );

    OUTPTR_POSTPROCESS_COUNT(Buf, *(local.CountDone));
    OUTPTR_POSTPROCESS(CountDone);

    return(err);
}


UDIError d386_UDITransMode (void _far * rm_stk_ptr, int stacksize)
{
INCOMING_PARAMS(
  UDIMode	*Mode;			/* Out */
 )
UDIError err;

    OUTPTR_PREPROCESS(Mode);

    err = UDITransMode(
	local.Mode
    );

    OUTPTR_POSTPROCESS(Mode);

    return(err);
}

#endif
/*==================== End of DOS386 glue routines ====================================*/


