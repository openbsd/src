/*---------------------------------------------------------------------------

  VMSmunch.c                    version 1.3                     28 Apr 1992

  This routine is a blatant and unrepentent appropriation of all the nasty
  and difficult-to-do and complicated VMS shenanigans which Joe Meadows has
  so magnificently captured in his FILE utility.  Not only that, it's even
  allowed! (see below).  But let it be clear at the outset that Joe did all
  the work; yea, verily, he is truly a godlike unit.

  The appropriations and modifications herein were performed primarily by
  him known as "Cave Newt," although the Info-ZIP working group probably had
  their fingers in it somewhere along the line.  The idea is to put the raw
  power of Joe's original routine at the disposal of various routines used
  by UnZip (and Zip, possibly), not least among them the utime() function.
  Read on for details...

	18-JUL-1994	Hunter Goatley <goathunter@WKU.EDU>
			Fixed IO$_ACCESS call.

	18-Jul-1994	Richard Levitte	levitte@e.kth.se
			Changed VMSmunch() to deassign the channel before
			returning when an error has occured.

	02-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved definition of VMStimbuf struct from here
			to vmsmunch.h
  ---------------------------------------------------------------------------

  Usage (i.e., "interface," in geek-speak):

     int VMSmunch( char *filename, int action, char *ptr );

     filename   the name of the file on which to be operated, obviously
     action     an integer which specifies what action to take
     ptr        pointer to any extra item which may be needed (else NULL)

  The possible values for the action argument are as follows:

     GET_TIMES      get the creation and revision dates of filename; ptr
                    must point to an empty VMStimbuf struct, as defined 
		    in vmsmunch.h
                    (with room for at least 24 characters, including term.)
     SET_TIMES      set the creation and revision dates of filename (utime
                    option); ptr must point to a valid VMStimbuf struct,
                    as defined in vmsmunch.h
     GET_RTYPE      get the record type of filename; ptr must point to an
                    integer which, on return, is set to the type (as defined
                    in VMSmunch.h:  FAT$C_* defines)
     CHANGE_RTYPE   change the record type to that specified by the integer
                    to which ptr points; save the old record type (later
                    saves overwrite earlier ones)
     RESTORE_RTYPE  restore the record type to the previously saved value;
                    or, if none, set it to "fixed-length, 512-byte" record
                    format (ptr not used)

  ---------------------------------------------------------------------------

  Comments from FILE.C, a utility to modify file characteristics:

     Written by Joe Meadows Jr, at the Fred Hutchinson Cancer Research Center
     BITNET: JOE@FHCRCVAX
     PHONE: (206) 467-4970

     There are no restrictions on this code, you may sell it, include it 
     with any commercial package, or feed it to a whale.. However, I would 
     appreciate it if you kept this comment in the source code so that anyone
     receiving this code knows who to contact in case of problems. Note that 
     I do not demand this condition..

  ---------------------------------------------------------------------------*/




/*****************************/
/*  Includes, Defines, etc.  */
/*****************************/

#include <descrip.h>
#include <rms.h>
#include <stdio.h>
#include <iodef.h>
#include <string.h>
#include <starlet.h>
#include <atrdef.h>   /* this gets created with the c3.0 compiler */
#include <fibdef.h>   /* this gets created with the c3.0 compiler */

#include "VMSmunch.h"  /* GET/SET_TIMES, RTYPE, etc. */
#include "VMSmunch_private.h"	/* fatdef.h, etc. */

#define RTYPE     fat$r_rtype_overlay.fat$r_rtype_bits
#define RATTRIB   fat$r_rattrib_overlay.fat$r_rattrib_bits

static void asctim();
static void bintim();

/* from <ssdef.h> */
#ifndef SS$_NORMAL
#  define SS$_NORMAL    1
#  define SS$_BADPARAM  20
#endif





/*************************/
/*  Function VMSmunch()  */
/*************************/

int VMSmunch( filename, action, ptr )
    char  *filename, *ptr;
    int   action;
{

    /* original file.c variables */

    static struct FAB Fab;
    static struct NAM Nam;
    static struct fibdef Fib; /* short fib */

    static struct dsc$descriptor FibDesc =
      {sizeof(Fib),DSC$K_DTYPE_Z,DSC$K_CLASS_S,(char *)&Fib};
    static struct dsc$descriptor_s DevDesc =
      {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,&Nam.nam$t_dvi[1]};
    static struct fatdef Fat;
    static union {
      struct fchdef fch;
      long int dummy;
    } uchar;
    static struct fjndef jnl;
    static long int Cdate[2],Rdate[2],Edate[2],Bdate[2];
    static short int revisions;
    static unsigned long uic;
    static union {
      unsigned short int value;
      struct {
        unsigned system : 4;
        unsigned owner : 4;
        unsigned group : 4;
        unsigned world : 4;
      } bits;
    } prot;

    static struct atrdef Atr[] = {
      {ATR$S_RECATTR,ATR$C_RECATTR,&Fat},        /* record attributes */
      {ATR$S_UCHAR,ATR$C_UCHAR,&uchar},      /* File characteristics */
      {ATR$S_CREDATE,ATR$C_CREDATE,&Cdate[0]}, /* Creation date */
      {ATR$S_REVDATE,ATR$C_REVDATE,&Rdate[0]}, /* Revision date */
      {ATR$S_EXPDATE,ATR$C_EXPDATE,&Edate[0]}, /* Expiration date */
      {ATR$S_BAKDATE,ATR$C_BAKDATE,&Bdate[0]}, /* Backup date */
      {ATR$S_ASCDATES,ATR$C_ASCDATES,&revisions}, /* number of revisions */
      {ATR$S_FPRO,ATR$C_FPRO,&prot},         /* file protection  */
      {ATR$S_UIC,ATR$C_UIC,&uic},            /* file owner */
      {ATR$S_JOURNAL,ATR$C_JOURNAL,&jnl},        /* journal flags */
      {0,0,0}
    } ;

    static char EName[NAM$C_MAXRSS];
    static char RName[NAM$C_MAXRSS];
    static struct dsc$descriptor_s FileName =
      {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    static struct dsc$descriptor_s string = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    static short int DevChan;
    static short int iosb[4];

    static long int i,status;
/*  static char *retval;  */


    /* new VMSmunch variables */

    static int  old_rtype=FAT$C_FIXED;   /* storage for record type */



/*---------------------------------------------------------------------------
    Initialize attribute blocks, parse filename, resolve any wildcards, and
    get the file info.
  ---------------------------------------------------------------------------*/

    /* initialize RMS structures, we need a NAM to retrieve the FID */
    Fab = cc$rms_fab;
    Fab.fab$l_fna = filename;
    Fab.fab$b_fns = strlen(filename);
    Fab.fab$l_nam = &Nam; /* FAB has an associated NAM */
    Nam = cc$rms_nam;
    Nam.nam$l_esa = EName; /* expanded filename */
    Nam.nam$b_ess = sizeof(EName);
    Nam.nam$l_rsa = RName; /* resultant filename */
    Nam.nam$b_rss = sizeof(RName);

    /* do $PARSE and $SEARCH here */
    status = sys$parse(&Fab);
    if (!(status & 1)) return(status);

    /* search for the first file.. If none signal error */
    status = sys$search(&Fab);
    if (!(status & 1)) return(status);

    while (status & 1) {
        /* initialize Device name length, note that this points into the NAM
           to get the device name filled in by the $PARSE, $SEARCH services */
        DevDesc.dsc$w_length = Nam.nam$t_dvi[0];

        status = sys$assign(&DevDesc,&DevChan,0,0);
        if (!(status & 1)) return(status);

        FileName.dsc$a_pointer = Nam.nam$l_name;
        FileName.dsc$w_length = Nam.nam$b_name+Nam.nam$b_type+Nam.nam$b_ver;

        /* Initialize the FIB */
        for (i=0;i<3;i++)
#ifdef VAXC
            Fib.fib$r_fid_overlay.fib$w_fid[i]=Nam.nam$w_fid[i];
#else
	    Fib.fib$w_fid[i]=Nam.nam$w_fid[i];
#endif
        for (i=0;i<3;i++)
#ifdef VAXC
            Fib.fib$r_did_overlay.fib$w_did[i]=Nam.nam$w_did[i];
#else
            Fib.fib$w_did[i]=Nam.nam$w_did[i];
#endif

        /* Use the IO$_ACCESS function to return info about the file */
        /* Note, used this way, the file is not opened, and the expiration */
        /* and revision dates are not modified */
        status = sys$qiow(0,DevChan,IO$_ACCESS,&iosb,0,0,
                          &FibDesc,&FileName,0,0,&Atr,0);
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
	    return(status);
	  }
        status = iosb[0];
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
	    return(status);
	  }

    /*-----------------------------------------------------------------------
        We have the current information from the file:  now see what user
        wants done with it.
      -----------------------------------------------------------------------*/

        switch (action) {

          case GET_TIMES:
              asctim(((struct VMStimbuf *)ptr)->modtime, Cdate);
              asctim(((struct VMStimbuf *)ptr)->actime, Rdate);
              break;

          case SET_TIMES:
              bintim(((struct VMStimbuf *)ptr)->modtime, Cdate);
              bintim(((struct VMStimbuf *)ptr)->actime, Rdate);
              break;

          case GET_RTYPE:   /* non-modifying */
              *(int *)ptr = Fat.RTYPE.fat$v_rtype;
              return RMS$_NORMAL;     /* return to user */
              break;

          case CHANGE_RTYPE:
              old_rtype = Fat.RTYPE.fat$v_rtype;         /* save current one */
              if ((*(int *)ptr < FAT$C_UNDEFINED) || 
                  (*(int *)ptr > FAT$C_STREAMCR))
                  Fat.RTYPE.fat$v_rtype = FAT$C_STREAMLF;  /* Unix I/O happy */
              else
                  Fat.RTYPE.fat$v_rtype = *(int *)ptr;
              break;

          case RESTORE_RTYPE:
              Fat.RTYPE.fat$v_rtype = old_rtype;
              break;

          default:
              return SS$_BADPARAM;   /* anything better? */
        }

    /*-----------------------------------------------------------------------
        Go back and write modified data to the file header.
      -----------------------------------------------------------------------*/

        /* note, part of the FIB was cleared by earlier QIOW, so reset it */
#ifdef VAXC
        Fib.fib$r_acctl_overlay.fib$l_acctl = FIB$M_NORECORD;
#else
        Fib.fib$l_acctl = FIB$M_NORECORD;
#endif
        for (i=0;i<3;i++)
#ifdef VAXC
            Fib.fib$r_fid_overlay.fib$w_fid[i]=Nam.nam$w_fid[i];
#else
            Fib.fib$w_fid[i]=Nam.nam$w_fid[i];
#endif
        for (i=0;i<3;i++)
#ifdef VAXC
            Fib.fib$r_did_overlay.fib$w_did[i]=Nam.nam$w_did[i];
#else
            Fib.fib$w_did[i]=Nam.nam$w_did[i];
#endif

        /* Use the IO$_MODIFY function to change info about the file */
        /* Note, used this way, the file is not opened, however this would */
        /* normally cause the expiration and revision dates to be modified. */
        /* Using FIB$M_NORECORD prohibits this from happening. */
        status = sys$qiow(0,DevChan,IO$_MODIFY,&iosb,0,0,
                          &FibDesc,&FileName,0,0,&Atr,0);
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
	    return(status);
	  }

        status = iosb[0];
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
	    return(status);
	  }

        status = sys$dassgn(DevChan);
        if (!(status & 1)) return(status);

        /* look for next file, if none, no big deal.. */
        status = sys$search(&Fab);
    }
} /* end function VMSmunch() */





/***********************/
/*  Function bintim()  */
/***********************/

void asctim(time,binval)   /* convert 64-bit binval to string, put in time */
    char *time;
    long int binval[2];
{
    static struct dsc$descriptor date_str={23,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
      /* dsc$w_length, dsc$b_dtype, dsc$b_class, dsc$a_pointer */
 
    date_str.dsc$a_pointer = time;
    sys$asctim(0, &date_str, binval, 0);
    time[23] = '\0';
}





/***********************/
/*  Function bintim()  */
/***********************/

void bintim(time,binval)   /* convert time string to 64 bits, put in binval */
    char *time;
    long int binval[2];
{
    static struct dsc$descriptor date_str={0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};

    date_str.dsc$w_length = strlen(time);
    date_str.dsc$a_pointer = time;
    sys$bintim(&date_str, binval);
}
