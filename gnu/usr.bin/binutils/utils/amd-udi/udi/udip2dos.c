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
 *****************************************************************************
 *       $Id: udip2dos.c,v 1.2 1996/11/23 04:11:24 niklas Exp $
 *       $Id: @(#)udip2dos.c	2.22, AMD
 */

/* Modified M.Typaldos 11/92 - Added '386 specific code (mainly calls to
 *              functions in dos386c.c, UDIPxxx calls).
 */

#define _UDIP2DOS_C


#if 1
#define DFEIPCIdCompany 0x0001	/* Company ID AMD */
#define DFEIPCIdProduct 0x1	/* Product ID 0 */
#endif
#define DFEIPCIdVersion 0x125	/* 1.2.5 */

#include <process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <udiproc.h>
#include <udidos.h>

#ifdef DOS386
#include <pltypes.h>
#include <pharlap.h>
#include <dos386c.h>
#include <realcopy.h>

	/* 
	 * term_addr will be used to store the protected mode address
	 * of TermStruct.  This is a 48 bit pointer into the area
	 * allocated by the call to realcopy.  The address is stored
	 * in term_addr by UDIConnect.
	 *
	 */
	ULONG	*term_addr_ptr;	
	FARPTR	term_addr;

	/********************************************************
	 * In DOS386 mode, a standard C PTR is a protected mode ptr
	 * so CPTR_TO_REALPTR invokes PROT_TO_REAL and REALPTR_TO_CPTR
	 * just invokes REAL_TO_PROT
	 ********************************************************/
#define CPTR_TO_REALPTR(p) PROT_TO_REAL(p)
#define REALPTR_TO_CPTR(p) REAL_TO_PROT(p)


#else	/* not DOS386 */

	/********************************************************
	 * In non-DOS386 mode, a standard C PTR is just a far real ptr
	 * so CPTR_TO_REALPTR and REALPTR_TO_CPTR are noops
	 ********************************************************/
#define CPTR_TO_REALPTR(p) (p)
#define REALPTR_TO_CPTR(p) (p)
#define NEARPTR_TO_FARPTR(p) (p)

#define _fstrcmp(a,b) strcmp(a,b)

#define FP_OFF(p) (p)
#define REALPTR

	

#endif  /* DOS386 */


#ifdef __HIGHC__
/* hc386 doesn't have fstrcmp */
/* this specialized version only works for strings less than 256 */
/* which is good enough for comparing TIP names */
int _fstrcmp(FARPTR s1, char _far * s2)
{
#define MAXFARSTR 256
  char nears1[MAXFARSTR];
  char nears2[MAXFARSTR];
  extern USHORT	GetDS();

  _movedata(FP_SEL(s1), FP_OFF(s1), GetDS(), (unsigned int)nears1, MAXFARSTR);
  _movedata(FP_SEL(s2), FP_OFF(s2), GetDS(), (unsigned int)nears2, MAXFARSTR);
  return(strcmp(nears1,nears2));

}

#endif

#ifdef DOS386
#include "io.h"
	/* this routine takes a possibly incomplete executable name */
	/* and uses the PATH environment variable to get the full name */
void get_full_exe_name(char *TIPName, char *FullName)
{
char *path_env = getenv("PATH");
char drive[10];
char dir[128];
char fname[10];
char ext[10];
char pathprefix[128];
char pathdir[128];
char *tmp;

    _splitpath(TIPName, drive, dir, fname, ext);
    if (strlen(ext) == 0)
	strcpy(ext, ".exe");	/* if no ext, force .exe */
    if (dir[0] == '\\') {
        /* absolute pathname, just return it */
	_makepath(FullName, drive,dir,fname,ext);
	return;
    }
    else {
	/* not an absolute pathname, need to use PATH env var */
        tmp = path_env;
        while ( *tmp != '\0')
        {
            sscanf(tmp, "%[^;]", pathprefix);
            sprintf (pathdir, "%s\\%s", pathprefix, dir);
	    _makepath(FullName, drive,pathdir,fname,ext);
            if (access (FullName, 0) == 0 ) {  /* check for open with read access */
               return;				/* found one, return with FullName set */
            }
            else {
                tmp += strlen(pathprefix) + 1;
	    }
        }   /* while */
	/* if we got this far, we didn't find a match */
        *FullName = 0;
    } /* if */
    
}


int dpmi_exists(void)
{
    CONFIG_INF config;  /* for DPMI config info */
    UCHAR vmmname[256];

    _dx_config_inf(&config, vmmname);	/* arg2 not really used here */
    return(config.c_dpmif);
}


#endif

/*************************************************************
 * The following macros allow us to use a common macro to make a UDICall
 * for either real mode DFEs or DOS386 mode DFEs
 *************************************************************/
#if defined __STDC__ || defined _MSC_VER
#define XCAT(a,b) a##b
#else
#define XCAT(a,b) a/**/b
#endif
#define CAT(a,b) XCAT(a,b)

#ifndef DOS386

	/************************************************
	 * in real mode, just call directly thru the connection pointer to the TIP
	 * eg, UDICALL3(p,f,a,b,c) becomes p->UDIf(a,b,c)
	 ************************************************/
#define UDICALLPREFIX0(p,f) CAT(p->UDI,f)(
#define UDICALLPREFIX(p,f)  UDICALLPREFIX0(p,f) 	/* same for 0 or >0 args */

#else	/* not DOS386 */


	/************************************************
	 * in DOS386 mode, call an intermediate function UDIPxxx
	 * and pass it a pointer to the TIP function, (along with all parameters)
	 * eg, UDICALL3(p,f,a,b,c) becomes UDIPf(p->UDIf,a,b,c)
	 ************************************************/
#define UDICALLPREFIX0(p,f) CAT(UDIP,f)(CAT((REALPTR)p->UDI,f)
#define UDICALLPREFIX(p,f)  UDICALLPREFIX0(p,f) ,	/* need a comma here for >0 args */
#endif /* else on DOS386 */

#define UDICALL0(p,f) 				UDICALLPREFIX0(p,f) )
#define UDICALL1(p,f,a1) 			UDICALLPREFIX(p,f) a1)
#define UDICALL2(p,f,a1,a2) 			UDICALLPREFIX(p,f) a1,a2)
#define UDICALL3(p,f,a1,a2,a3) 			UDICALLPREFIX(p,f) a1,a2,a3)
#define UDICALL4(p,f,a1,a2,a3,a4) 		UDICALLPREFIX(p,f) a1,a2,a3,a4)
#define UDICALL5(p,f,a1,a2,a3,a4,a5) 		UDICALLPREFIX(p,f) a1,a2,a3,a4,a5)
#define UDICALL6(p,f,a1,a2,a3,a4,a5,a6) 	UDICALLPREFIX(p,f) a1,a2,a3,a4,a5,a6)
#define UDICALL7(p,f,a1,a2,a3,a4,a5,a6,a7) 	UDICALLPREFIX(p,f) a1,a2,a3,a4,a5,a6,a7)
#define UDICALL8(p,f,a1,a2,a3,a4,a5,a6,a7,a8) 	UDICALLPREFIX(p,f) a1,a2,a3,a4,a5,a6,a7,a8)




extern DOSTerm TermStruct;	/* located in dosdfe.asm */
extern void    UDITerminate();


#define FBUFSIZE 2048

struct UDIConnections {
    struct UDIVecRec _far * VecRecP;
    UDISessionId ConnId;
}; /* struct UDIConnections */

#define NumSessions 10

static struct UDIVecRec _far * CurrentConnection;

#ifdef DOS386

REALPTR _far * UDIVecP;
#define REALNULL (REALPTR) 0

#else

static struct UDIVecRec _FAR * _FAR * UDIVecP;
#define REALNULL NULL

#endif


static struct UDIConnections Connections[ NumSessions ];

static char *GetConfigFileName( void )
{
    char *cp;
    static char buf[ _MAX_PATH ];

    if ((cp = getenv( "UDICONF" )) != NULL)
	return cp;
    
    _searchenv( "udiconfs.txt", "PATH", buf );

    return buf;
    }

#ifdef DOS386

REALPTR _far * FindIntVect() 

#else

static struct UDIVecRec _FAR * _FAR * FindIntVect()

#endif
{
    union rec recognizer;
    int i;

    InitRecognizer( &recognizer );


    /* Try and find a vector that is currently in use for UDI */
    for (i = 0x60; i < 0x66; i++ ) {

#ifdef DOS386

	 UDIVecP = (REALPTR _far *) (REALPTR_TO_CPTR(i*4));

#else

	UDIVecP =  (static struct UDIVecRec _FAR * _FAR *)(i * 4);

#endif

	if ((*UDIVecP != REALNULL) &&
	    ((struct UDIVecRec _far *) REALPTR_TO_CPTR(*UDIVecP))->recognizer.l == recognizer.l)
	    	return UDIVecP;
	}

    return NULL;
}


 
UDIError CheckForConnect(char *TIPName, int SessionNum, UDISessionId *Session, char *TIPParms)
/* Check the interrupt table for a matching connection. 
 * Note that the call to the TIPs UDIConnect is via annother function UDIPConnect
 * for the protected mode version.
 *
 * 7/93 MDT -- Took out 'if' that only set up the connection id if there was
 *             no error message returned.  This caused the DFE to be unable
 *             to report TIP specific (negative) error messages from a 
 *             call to UDIConnect.  Placed a new if which checks if the error
 *             is positive and sets the CurrentConnection and VecRecP to null
 *             so that any subsequent call to UDIDisconnect will work properly.
 *
 */
{
	struct UDIVecRec _far * CurConn;
	UDIError	err;
	UDISessionId	tempId=0;


	if (UDIVecP)  {	/* TIPs running somewhere on machine */
		for (CurConn = (struct UDIVecRec _far *)REALPTR_TO_CPTR(*UDIVecP); 
			FP_OFF(CurConn); 
#ifdef DOS386
			CurConn = (struct UDIVecRec _far *) REALPTR_TO_CPTR((REALPTR)(CurConn->Next))) 
#else
			CurConn = (struct UDIVecRec _far *) REALPTR_TO_CPTR((CurConn->Next))) 
#endif
	
		{
#ifdef DOS386
			if (!_fstrcmp(NEARPTR_TO_FARPTR(TIPName),(char _far *)REALPTR_TO_CPTR((REALPTR)(CurConn->exeName)))) {
				err = UDICALL3(CurConn, Connect, TIPParms,
					&tempId,(DOSTerm _far *)term_addr);
#else
			if (!_fstrcmp(NEARPTR_TO_FARPTR(TIPName),(char _far *)REALPTR_TO_CPTR((CurConn->exeName)))) {
				err = UDICALL3(CurConn, Connect, TIPParms,
					&tempId, &TermStruct);
#endif
				Connections[SessionNum].ConnId = tempId;
				*Session = SessionNum;
				if (err > 0)
					CurrentConnection = Connections[SessionNum].VecRecP = 0;
				else
					CurrentConnection = Connections[SessionNum].VecRecP = CurConn;
	 			return err;
			} /* if */
		} /* for */
	} /* if */

	return UDIErrorExecutableNotTIP;


} /* CheckForConnect */



UDIError UDIConnect UDIParams((
  char		*Configuration,		/* In */
  UDISessionId	*Session		/* Out */
  ))
{
	int	i, n;
	char	buf[ FBUFSIZE ];
	char	TIPName[ FILENAME_MAX ];
	char	*TIPParms;


    FILE *fp;

#ifdef DOS386
    static int		DOS386_Initialized;
    extern USHORT	GetCS();
    extern USHORT	GetDS();


    int	start_real(), end_real();	/* prototypes to cast locations */
    						/*   in real-mode code and data */
    REALPTR	term_func_addr;
    ULONG	*term_func_addr_ptr;

   
    if (!DOS386_Initialized) {		/* Do this init logic only once */


    	/* Need to copy the real mode code into conventional memory. */
	if (realcopy((ULONG)start_real,(ULONG) end_real,(REALPTR *) &real_base,(FARPTR *) &prot_base,(USHORT *) &rmem_adr)) {
		printf("\nUDIConnect: realcopy call failed;\n");
		printf(  "   Probable cause: insufficient free conventional memory.\n");
		exit(0);
	}

	/* 
	 * The following code sets up the pointer in TermStruct to 
	 * contain the correct real mode address of the UDITerminate
	 * function.  This process can best be illustrated with the
	 * following diagram.
	 *
	 *                      |    ____________________
	 *			|   |                    |____
	 *			|   | term_func_addr_ptr |    |
	 *                      |   |____________________|    |
	 *			|			      |D
	 *			|    ____________________     |
	 *			|   |		         |/___|
	 *			|   | term_func_addr     |\
	 *                      |   |                    |_________
	 *			|   |____________________|         |
	 *                      |			           |
	 *                      |    ____________________          |
	 *  Protected Memory   	|   |		         |____     |
	 *                      |   | term_addr	         |    |    |
	 *                      |   |____________________|    |    |
	 *                      |	                      |    |
	 *                      |		              |B   |
	 *                      |---------------------------- |    |C
	 *                      |___ ____________________     |    |
	 *                      |r  |		         |/___|    |
	 *                      |e m|		         |\        |
	 *                      |a e| TermStruct         |____     |
	 *  Real Mode Memory    |l m|____________________|    |    |
	 *                      |  o|		         |    |A   |
	 *                      |c r|		         |/___|	   |
        *                      |o y| UDITerminate()     |\	   |
        *                      |p  |		         |/________|
        *                      |y  |____________________|\
	 *                      |---
	 *
	 * Note that the point of this is to get TermStruct to contain
	 * a real mode pointer to the UDITerminate() function.  Therefor,
	 * pointer A is a real mode far pointer.  To write the function
	 * address into TermStruct, we are going to need a protected
	 * mode far pointer, pointer B.  We also need the real mode
	 * function address to copy into TermStruct, this is pointer C.
	 * Since we'll need to use _fmemmove, which requires two far
	 * pointers (remember that since B is a protected mode far pointer,
	 * we must use _fmemmove), we need another protected mode far
	 * pointer to point at the address to be copied, this is pointer
	 * D.
	 *
	 * Calls to NEARPTR_TO_FARPTR cannot be used to create pointers
	 * into the real-copy allocated area.  These need to be constructed
	 * using real_base and prot_base, as explained in the "Mixing
	 * Real and Protected Mode Code" section of the chapter on 
	 * "Programming for DPMI Compatablity" in the Phar Lap 386|DOS-
	 * Extender Programmer's Guide to DPMI and Windows.
	 *
	 */

	term_func_addr_ptr = &term_func_addr;
	term_func_addr = (REALPTR)(real_base + (ULONG)(&UDITerminate));
	term_addr = (FARPTR)(prot_base + (ULONG)(&TermStruct));
	
	
	/*
	 * Used to create pointers into conventional memory other than the
	 * the area created by the call to real_copy (for example to access
	 * the interrupt table.
	 */
	code_selector = GetCS();
	data_selector = GetDS();

	_fmemmove(term_addr,NEARPTR_TO_FARPTR(term_func_addr_ptr),sizeof(*term_func_addr_ptr));

	DOS386_Initialized = TRUE;
    }

#endif /* DOS386 */

    n = strlen( Configuration );
    TIPParms = NULL;

    /* Get configuration line that corresponds to requested configuration */
    if ((fp = fopen( GetConfigFileName(), "r" )) == NULL)
	return UDIErrorCantOpenConfigFile;

	while (fgets( buf, FBUFSIZE -1, fp ) != NULL) {
		if (isspace( buf[n] ) && (strncmp( buf, Configuration, n ) == 0)) {
			sscanf( buf, "%*s %s%n", TIPName, &n );
	    		TIPParms = &buf[n];
	    		while (isspace( *TIPParms ))
				TIPParms++;
	    		if (*TIPParms)
				TIPParms[ strlen( TIPParms ) -1 ] = '\0';
	    		break;
	    	} /* if */
	} /* while */

	fclose( fp );

	if (!TIPParms)
		return UDIErrorNoSuchConfiguration;

	for (i = 0; i < NumSessions; i++)
		if (!Connections[i].VecRecP)
			break;
    
    if (i >= NumSessions)
	return UDIErrorCantConnect;

    /* Determine if any TIPs are currently running */
    if (!UDIVecP)	/* None for this DFE */
	UDIVecP = FindIntVect();	/* Try other DFEs */


    n = CheckForConnect(TIPName, i, Session, TIPParms);

    if (n != UDIErrorExecutableNotTIP)
    	return n;

#ifdef DOS386
  {
    char FullTIPName[256]; /* general name storage */
    UINT errinfo, doserr;
    UINT vmmh;
    LDEXP_BLK parmp;
    typedef UDIStruct {	/* a shortened version of the pharlap.h struct */
				/* to make sure this ons is packed (in this modeule, */
				/* all we really need from this structure is ss:esp */
	  ULONG	 eip;		/* initial EIP */
	  USHORT cs;		/* initial CS */
	  ULONG	 esp;		/* initial ESP */
	  USHORT ss;		/* initial SS */
    } PACKED_LDEXP_BLK;
    PACKED_LDEXP_BLK *packed_parmp = (PACKED_LDEXP_BLK *) &parmp;
    int err;
    FARPTR TIPStackPtr;
    extern int _exp_call_to(LDEXP_BLK *p);
    
      /* Under Windows (DPMI 0.9) support we can't load a second DOS extender
	application and switch back and forth to it.  So we get around this
	by loading the .exp file instead (using the current Dos extender)
	if DPMI is present, check if the tip program name also exists with a .exp extension 
	if it does, do a dx_ld_flat of that and run it under our DFE dos extender
     */

    if (dpmi_exists()) {
	get_full_exe_name(TIPName, FullTIPName);	/* ld_flat needs full pathname */

	/* now try to load the .exe file as if it were an exp file */
	/* (a nice feature of pharlap is that it dx_ld_flat can load */
	/* an .exe file that has an .exp file bound into it */
	err = _dx_ld_flat((UCHAR *) FullTIPName,
	      &parmp,
	      FALSE,		/* read into mem immediately */
	      &vmmh,		/* vmm handle pointer */
	      &errinfo,		/* error info */
	      &doserr);		/* dos error */

	if (err)		/* if any kind of error at all, give up on exp file */
	    goto SpawnTipExe;	/* and go back to exe file */
	else {
		/* successful load, now jump to .exp file code, which will
		   return back to here. First we will put the original TIPName
		   into the top of stack so the TIP can find it (like an argument).
		*/
	    FP_SET(TIPStackPtr, packed_parmp->esp-256, packed_parmp->ss);
	    _fmemmove(TIPStackPtr, NEARPTR_TO_FARPTR(TIPName), strlen(TIPName)+1);

	    /* now call into loaded exp code (and hopefully return) */
	    if (_exp_call_to(&parmp) != 0)
	        return UDIErrorCantStartTIP;
	    else
	        goto TIPLoaded;
        }/*else*/
    } /* if DPMI present */
  } /* block for ifdef DOS386 */
#endif


SpawnTipExe:
    /* To get around DOS 2.x problems as well as problems with filename
       expansion/searching, we pass the executable name as an addtional
       parameter. Also, we expect the TIP to TSR, so the high byte of
       the return value is probably 0x03. But some TIPs might be more
       clever and really return (activating a device driver or other
       TSR first), so we ignore the high byte of the return from spawnlp.
    */

    if ((n = spawnlp( P_WAIT, TIPName, TIPName, TIPName, NULL ) & 0xff) != 0)
	return UDIErrorCantStartTIP;

TIPLoaded:
    /* Determine if any TIPs are currently running */
	if (!UDIVecP)	/* None for this DFE */
		UDIVecP = FindIntVect();	/* Try other DFEs (or new TIPs) */


    return CheckForConnect(TIPName, i, Session, TIPParms);
    
} /* UDIConnect() */



UDIError UDIDisconnect UDIParams((
	UDISessionId	Session,		/* In */
	UDIBool		Terminate		/* In */
	))
{
	UDIError err;

	if (Session >= NumSessions || !Connections[Session].VecRecP)
		return UDIErrorNoSuchConnection;

#ifdef DOS386
	if (((err = UDICALL3(CurrentConnection,Disconnect,
		Connections[ Session ].ConnId, Terminate, (DOSTerm _far *)term_addr ))
		== UDINoError) &&
		(CurrentConnection == Connections[ Session ].VecRecP))
#else
	if (((err = UDICALL3(CurrentConnection,Disconnect,
		Connections[ Session ].ConnId, Terminate, &TermStruct))
		== UDINoError) &&
		(CurrentConnection == Connections[ Session ].VecRecP))
#endif

	CurrentConnection = NULL;

    	return err;

} /* UDIDisconnect() */



UDIError UDISetCurrentConnection UDIParams((
	UDISessionId	Session			/* In */
	))
{
	UDIError n;
	struct UDIConnections *ConnP;

	if (Session >= NumSessions || !Connections[Session].VecRecP)
		return UDIErrorNoSuchConnection;

	ConnP = &Connections[Session];

	n = UDICALL1(ConnP->VecRecP,SetCurrentConnection,
			   ConnP->ConnId);
	if (n == UDINoError)
	    CurrentConnection = ConnP->VecRecP;

	return n;

}  /* UDIDisconnect () */




UDIError UDICapabilities UDIParams((
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

	if (!CurrentConnection)
		return UDIErrorNoConnection;

	err = UDICALL8(CurrentConnection,Capabilities,
		TIPId, TargetId, DFEId, DFE, TIP, DFEIPCId,
		TIPIPCId, TIPString);

	*DFEIPCId = (((UDIUInt32)DFEIPCIdCompany) << 16) |
		(DFEIPCIdProduct << 12) | DFEIPCIdVersion;

	return err;

}  /* UDICapabilities() */




UDIError UDIEnumerateTIPs UDIParams((
	UDIInt	(*UDIETCallback)	/* In */
	UDIParams(( char *Configuration ))	/* In to callback() */
	))
{
	FILE *fp;
	char buf[ FBUFSIZE ];

	if ((fp = fopen( GetConfigFileName(), "r" )) == NULL)
		return UDIErrorCantOpenConfigFile;

	while (fgets( buf, FBUFSIZE, fp ))
		if (UDIETCallback( buf ) == UDITerminateEnumeration)
			break;

	fclose( fp );

	return UDINoError;

} /* UDIEnumerateTIPs() */



UDIError UDIGetErrorMsg UDIParams((
	UDIError	ErrorCode,		/* In */
	UDISizeT	MsgSize,		/* In */
	char		*Msg,			/* Out */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL4(CurrentConnection,GetErrorMsg,
		ErrorCode, MsgSize, Msg, CountDone);

} /* UDIGetErrorMsg() */



UDIError UDIGetTargetConfig UDIParams((
	UDIMemoryRange KnownMemory[],		/* Out */
	UDIInt	*NumberOfRanges,	/* In/Out */
	UDIUInt32	ChipVersions[],		/* Out */
	UDIInt	*NumberOfChips		/* In/Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL4(CurrentConnection,GetTargetConfig,
		KnownMemory, NumberOfRanges, ChipVersions, NumberOfChips);

} /* UDIGetTargetConfig() */



UDIError UDICreateProcess UDIParams((
	UDIPId	*PId			/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL1(CurrentConnection,CreateProcess,
			PId);

} /* UDICreateProcess() */



UDIError UDISetCurrentProcess UDIParams((
	UDIPId	PId			/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return	UDICALL1(CurrentConnection,SetCurrentProcess,
			 PId);

} /* UDISetCurrentProcess() */



UDIError UDIDestroyProcess UDIParams((
	UDIPId	PId			/* In */
	))
{
	
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL1(CurrentConnection,DestroyProcess,
			PId);
	
} /* UDIDestroyProcess() */



UDIError UDIInitializeProcess UDIParams((
	UDIMemoryRange	ProcessMemory[],	/* In */
	UDIInt		NumberOfRanges,		/* In */
	UDIResource	EntryPoint,		/* In */
	CPUSizeT	StackSizes[],		/* In */
	UDIInt		NumberOfStacks,		/* In */
	char		*ArgString		/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL6(CurrentConnection,InitializeProcess,
		ProcessMemory, NumberOfRanges, EntryPoint, StackSizes,
		NumberOfStacks, ArgString );

} /* UDIInitializeProcess() */



UDIError UDIRead UDIParams((
	UDIResource	From,			/* In */
	UDIHostMemPtr	To,			/* Out */
	UDICount	Count,			/* In */
	UDISizeT	Size,			/* In */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL6(CurrentConnection,Read, 
		From, To, Count, Size, CountDone, HostEndian);

} /* UDIRead() */



UDIError UDIWrite UDIParams((
	UDIHostMemPtr	From,			/* In */
	UDIResource	To,			/* In */
	UDICount	Count,			/* In */
	UDISizeT	Size,			/* In */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In */
  ))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL6(CurrentConnection,Write,
		From, To, Count, Size, CountDone, HostEndian);

} /* UDIWrite() */



UDIError UDICopy UDIParams((
	UDIResource	From,			/* In */
	UDIResource	To,			/* In */
	UDICount	Count,			/* In */
	UDISizeT	Size,			/* In */
	UDICount	*CountDone,		/* Out */
	UDIBool		Direction		/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL6(CurrentConnection,Copy,
		From, To, Count, Size, CountDone, Direction);

} /* UDICopy() */



UDIError UDIExecute UDIParams((
	void
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL0(CurrentConnection,Execute);

} /* UDIExecute() */



UDIError UDIStep UDIParams((
	UDIUInt32	Steps,			/* In */
	UDIStepType   StepType,		/* In */
	UDIRange      Range			/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL3(CurrentConnection,Step,
			Steps,StepType,Range);

} /* UDIStep() */




UDIVoid UDIStop UDIParams((
	void
	))
{
	if (!CurrentConnection)
		return;

	UDICALL0(CurrentConnection,Stop);

} /* UDIStop() */




UDIError UDIWait UDIParams((
	UDIInt32	MaxTime,		/* In */
	UDIPId		*PId,			/* Out */
	UDIUInt32	*StopReason		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL3(CurrentConnection,Wait,
		MaxTime, PId, StopReason);

} /* UDIWait() */



UDIError UDISetBreakpoint UDIParams((
	UDIResource	Addr,			/* In */
	UDIInt32	PassCount,		/* In */
	UDIBreakType	Type,			/* In */
	UDIBreakId	*BreakId		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL4(CurrentConnection,SetBreakpoint,
		Addr, PassCount, Type, BreakId);

} /* UDISetBreakpoint() */



UDIError UDIQueryBreakpoint UDIParams((
	UDIBreakId	BreakId,		/* In */
	UDIResource	*Addr,			/* Out */
	UDIInt32	*PassCount,		/* Out */
	UDIBreakType	*Type,			/* Out */
	UDIInt32	*CurrentCount		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL5(CurrentConnection,QueryBreakpoint,
		BreakId, Addr, PassCount, Type, CurrentCount);

} /* UDIQueryBreakpoint() */




UDIError UDIClearBreakpoint UDIParams((
	UDIBreakId	BreakId			/* In */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL1(CurrentConnection,ClearBreakpoint,
		BreakId);

} /* UDIClearBreakpoint() */



UDIError UDIGetStdout UDIParams((
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL3(CurrentConnection,GetStdout,
		Buf, BufSize, CountDone);

} /* UDIGetStout() */



UDIError UDIGetStderr UDIParams((
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL3(CurrentConnection,GetStderr,
		Buf, BufSize, CountDone);

} /* UDIGetStderr() */




UDIError UDIPutStdin UDIParams((
	UDIHostMemPtr	Buf,			/* In */
	UDISizeT	Count,			/* In */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;

	return UDICALL3(CurrentConnection,PutStdin,
		Buf, Count, CountDone);

} /* UDIPutStdin() */



UDIError UDIStdinMode UDIParams((
	UDIMode		*Mode			/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL1(CurrentConnection,StdinMode,
		Mode);

} /* UDIStdinMode() */




UDIError UDIPutTrans UDIParams((
	UDIHostMemPtr	Buf,			/* In */
	UDISizeT	Count,			/* In */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL3(CurrentConnection,PutTrans,
		Buf, Count, CountDone);


} /* UDIPutTrans() */




UDIError UDIGetTrans UDIParams((
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In */
	UDISizeT	*CountDone		/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL3(CurrentConnection,GetTrans,
		Buf, BufSize, CountDone);

} /* UDIGetTrans() */




UDIError UDITransMode UDIParams((
	UDIMode		*Mode			/* Out */
	))
{
	if (!CurrentConnection)
		return UDIErrorNoConnection;


	return UDICALL1(CurrentConnection,TransMode,
			Mode);


} /* UDITransMode() */



UDIUInt32 UDIGetDFEIPCId()
{

    return((((UDIUInt32)DFEIPCIdCompany) << 16) |(DFEIPCIdProduct << 12) | DFEIPCIdVersion);

} /* UDIGetDFEIPCId() */
