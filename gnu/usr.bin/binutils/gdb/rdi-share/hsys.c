/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * Host C Library support functions.
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 */

#ifdef DEBUG
#  include <ctype.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include "adp.h"
#include "host.h"
#include "ardi.h"
#include "buffers.h"
#include "channels.h"        /* Channel interface. */
#include "angel_endian.h"
#include "logging.h"         /* Angel support functions. */
#include "msgbuild.h"
#include "sys.h"    
#include "hsys.h"      /* Function and structure declarations. */
#include "hostchan.h"

#define FILEHANDLE int

/* Note: no statics allowed.  All globals must be malloc()ed on the heap.
   The state struct is used for this purpose.  See 'hsys.h'.                */
/* This is the message handler function passed to the channel manager in
   HostSysInit.  It is called whenever a message is received. 'buffptr'
   points to the message body.  Functionality is provided by the debugger
   toolkit.  The routine is very loosely based on the HandleSWI routine from
   armos.c in the armulator source.                                         */
/* These routines could be tested by providing a simple interface to armsd, 
   and running them in that.   */


/* taken staight from armulator source */
#ifdef __riscos
  extern int _fisatty(FILE *);
# define isatty_(f) _fisatty(f)
# define EMFILE -1
# define EBADF -1
  int _kernel_escape_seen(void) { return 0 ;}
#else
# if defined(_WINDOWS) || defined(_CONSOLE)
#   define isatty_(f) (f == stdin || f == stdout)
# else
#   ifdef __ZTC__
#     include <io.h>
#     define isatty_(f) isatty((f)->_file)
#   else
#     ifdef macintosh
#       include <ioctl.h>
#       define isatty_(f) (~ioctl((f)->_file,FIOINTERACTIVE,NULL))
#     else
#       define isatty_(f) isatty(fileno(f))
#     endif
#   endif
# endif
#endif

/* Set up the state block, filetable and register the C lib callback fn */
int HostSysInit(const struct Dbg_HostosInterface *hostif, char **cmdline,
                hsys_state **stateptr)
{
  ChannelCallback HandleMessageFPtr = (ChannelCallback) HandleSysMessage;
  int i;
  *stateptr = (hsys_state *)malloc(sizeof(hsys_state));

  if (*stateptr == NULL) return RDIError_OutOfStore;

  (*stateptr)->hostif=hostif;
  (*stateptr)->last_errno=0;
  (*stateptr)->OSptr=(OSblock *)malloc(sizeof(OSblock));
  if ((*stateptr)->OSptr == NULL) return RDIError_OutOfStore;
  for (i=0; i<UNIQUETEMPS; i++) (*stateptr)->OSptr->TempNames[i]=NULL;
  for (i=0; i<HSYS_FOPEN_MAX; i++) {
       (*stateptr)->OSptr->FileTable[i]=NULL;
       (*stateptr)->OSptr->FileFlags[i]=0;
  }
  (*stateptr)->CommandLine=cmdline;

  return Adp_ChannelRegisterRead(CI_CLIB, (ChannelCallback)HandleMessageFPtr,
                                 *stateptr);
}

/* Shut down the Clib support, this will probably never get called though */
int HostSysExit(hsys_state *stateptr)
{
  free(stateptr->OSptr);
  free(stateptr);
  return RDIError_NoError;
}

#ifdef DEBUG
static void DebugCheckNullTermString(char *prefix, bool nl,
                                     unsigned int len, unsigned char *strp)
{
    printf("%s: %d: ", prefix, len);
    if (strp[len]=='\0')
       printf("\"%s\"", strp);
    else
       printf("NOT NULL TERMINATED");
    if (nl)
       printf("\n");
    else
    {
        printf(" ");
        fflush(stdout);
    }
}

#ifdef NEED_SYSERRLIST
extern int sys_nerr;
extern char *sys_errlist[];
#endif

static char *DebugStrError(int last_errno)
{
    if (last_errno < sys_nerr)
       return sys_errlist[last_errno];
    else
       return "NO MSG (errno>sys_nerr)";
}

static void DebugCheckErr(char *prefix, bool nl, int err, int last_errno)
{
    printf("\t%s: returned ", prefix);
    if (err == 0)
       printf("okay");
    else
       printf("%d, errno = %d \"%s\"", err, last_errno,
              DebugStrError(last_errno));
    if (nl)
       printf("\n");
    else
    {
        printf(" ");
        fflush(stdout);
    }
}

static void DebugCheckNonNull(char *prefix, bool nl,
                              void *handle, int last_errno)
{
    printf("\t%s: returned ", prefix);
    if (handle != NULL)
       printf("okay [%08x]", (unsigned int)handle);
    else
       printf("NULL, errno = %d \"%s\"", last_errno,
              DebugStrError(last_errno));
    if (nl)
       printf("\n");
    else
    {
        printf(" ");
        fflush(stdout);
    }
}

#define DebugPrintF(c) printf c;

#else

#define DebugCheckNullTermString(p, n, l, s)    ((void)(0))
#define DebugCheckErr(p, n, e, l)               ((void)(0))
#define DebugCheckNonNull(p, n, h, l)           ((void)(0))
#define DebugPrintF(c)                          ((void)(0))

#endif /* ifdef DEBUG ... else */

static FILE *hsysGetRealFileHandle(hsys_state *stateptr, int fh, char *flags)
{
    FILE *file_p = NULL;

    if (fh < 0 || fh >= HSYS_FOPEN_MAX)
    {
        stateptr->last_errno = EBADF;
        DebugPrintF(("\tfh %d out-of-bounds!\n", fh));
        return NULL;
    }
    else
    {
        file_p = stateptr->OSptr->FileTable[fh];
        if (file_p != NULL) {
            if (flags != NULL)
               *flags = stateptr->OSptr->FileFlags[fh];
        }
        else {
          stateptr->last_errno = EBADF;
          DebugPrintF(("\tFileTable[%d] is NULL\n", fh));
        }

        return file_p;
    }
}

int HandleSysMessage(Packet *packet, hsys_state *stateptr)
{
  unsigned int reason_code, mode, len, c, nbytes, nbtotal, nbtogo = 0;
  long posn, fl;
  char character;
  int err;

  /* Note: We must not free the buffer passed in as the callback handler */
  /* expects to do this.  Freeing any other buffers we have malloced */
  /* ourselves is acceptable */

  unsigned char *buffp = ((unsigned char *)BUFFERDATA(packet->pk_buffer))+16;
                                          /* buffp points to the parameters*/
                                          /* the invidual messages, excluding*/
                                          /* standard SYS fields (debugID, */
                                          /* osinfo and reasoncode) */
  unsigned char *buffhead = (unsigned char *)(packet->pk_buffer);

  int DebugID, OSInfo1, OSInfo2, count;

  const char* fmode[] = {"r","rb","r+","r+b",
                               "w","wb","w+","w+b",
                               "a","ab","a+","a+b",
                               "r","r","r","r"} /* last 4 are illegal */ ;

  FILEHANDLE fh;  /* fh is used as an index to the real file handle
                         * in OSptr */  
  FILE *fhreal;
  unpack_message(BUFFERDATA(buffhead), "%w%w%w%w", &reason_code,
                 &DebugID, &OSInfo1, &OSInfo2);
                                        /* Extract reason code from buffer. */
  reason_code &= 0xFFFF;        /* Strip away direction bit, OSInfo and     */
                                /* DebugInfo fields.  Will want to do some  */
                                /* sort of validation on this later.        */
  
  switch(reason_code)
  {

  case CL_WriteC:   /* Write a character to the terminal. */
                    /* byte data -> word status           */
    {
#ifdef DEBUG
      int c = (int)(*buffp);
      printf("CL_WriteC: [%02x]>%c<", c, isprint(c) ? c : '.');
#endif
      stateptr->hostif->writec(stateptr->hostif->hostosarg, (int)(*buffp));
      DevSW_FreePacket(packet);
      return msgsend(CI_CLIB,"%w%w%w%w%w", CL_WriteC|HtoT,
                    DebugID, OSInfo1, OSInfo2, NoError);
    }

  case CL_Write0:  /* Write a null terminated string to the terminal. */
    {
      unpack_message(buffp, "%w", &len);
      DebugCheckNullTermString("CL_Write0", TRUE, len, buffp+4);
      stateptr->hostif->write(stateptr->hostif->hostosarg,
                              (char *) buffp+4, len);
      DevSW_FreePacket(packet);
      return msgsend(CI_CLIB, "%w%w%w%w%w", CL_Write0|HtoT, DebugID, 
                    OSInfo1, OSInfo2, NoError);
    }

  case CL_ReadC:   /* Read a byte from the terminal */
    {
      DebugPrintF(("CL_ReadC: "));
      DevSW_FreePacket(packet);

      character = stateptr->hostif->readc(stateptr->hostif->hostosarg);
      DebugPrintF(("\nCL_ReadC returning [%02x]>%c<\n", character,
                   isprint(character) ? character : '.'));

      return msgsend(CI_CLIB, "%w%w%w%w%w%b", CL_ReadC|HtoT,
                    DebugID, OSInfo1, OSInfo2, NoError, character);
    }

  case CL_System:  /* Pass NULL terminated string to the hosts command 
                    * interpreter. As it is nULL terminated we dont need
                    * the length
                    */
    {
      unpack_message(buffp, "%w", &len);
      DebugCheckNullTermString("CL_System", TRUE, len, buffp+4);

      err = system((char *)buffp+4); /* Use the string in the buffer */
      stateptr->last_errno = errno;
      DebugCheckErr("system", TRUE, err, stateptr->last_errno);

      err = msgsend(CI_CLIB, "%w%w%w%w%w%w", CL_System|HtoT,
                    DebugID, OSInfo1, OSInfo2, NoError, err);
      DevSW_FreePacket(packet);
      return err;
    }

  case CL_GetCmdLine:  /* Returns the command line used to call the program */
    {
      /* Note: we reuse the packet here, this may not always be desirable */
      /* /* TODO: Use long buffers if possible */
      DebugPrintF(("CL_GetCmdLine: \"%s\"\n", *(stateptr->CommandLine)));

      if (buffhead!=NULL) {
        len = strlen(*(stateptr->CommandLine));
        if (len > Armsd_BufferSize-24) len = Armsd_BufferSize-24; 
        packet->pk_length = len + msgbuild(BUFFERDATA(buffhead),
                                           "%w%w%w%w%w%w", CL_GetCmdLine|HtoT,
                                           DebugID, OSInfo1, OSInfo2,
                                           NoError, len);
        strncpy((char *) BUFFERDATA(buffhead)+24,*(stateptr->CommandLine),
                len);
        
        Adp_ChannelWrite(CI_CLIB, packet);/* Send message. */
        return 0;
      }
      else return -1;
    }

  case CL_Clock:   /* Return the number of centiseconds since the support */
                   /* code started executing */
    {
      time_t retTime = time(NULL);
      if (retTime == (time_t)-1)
             stateptr->last_errno = errno;
      else
             retTime *=100;

      DebugPrintF(("CL_Clock: %lu\n", retTime));
      DebugCheckErr("time", TRUE, (retTime == (time_t)-1),
                    stateptr->last_errno);

      DevSW_FreePacket(packet);
      return msgsend(CI_CLIB, "%w%w%w%w%w%w",CL_Clock|HtoT,
                         DebugID, OSInfo1, OSInfo2, NoError, retTime);
    }

  case CL_Time:    /* return time, in seconds since the start of 1970 */
    {
      time_t retTime = time(NULL);
      if (retTime == (time_t)-1)
              stateptr->last_errno = errno;

      DebugPrintF(("CL_Time: %lu\n", retTime));
      DebugCheckErr("time", TRUE, (retTime == (time_t)-1),
                    stateptr->last_errno);

      DevSW_FreePacket(packet);
      return msgsend(CI_CLIB,"%w%w%w%w%w%w",CL_Time|HtoT,
                         DebugID, OSInfo1, OSInfo2, NoError, retTime);
    }

  case CL_Remove:  /* delete named in the null terminated string */
    {
      /* Removing an open file will cause problems but once again
       * its not our problem, likely result is a tangled FileTable */
      /* As the filename is passed with a null terminator we can use it
       * straight out of the buffer without copying it.*/

      unpack_message(buffp, "%w", &len);
      DebugCheckNullTermString("CL_Remove", TRUE, len, buffp+4);

      err=remove((char *)buffp+4);
      stateptr->last_errno = errno;
      DevSW_FreePacket(packet);
      DebugCheckErr("remove", TRUE, err, stateptr->last_errno);

      return msgsend(CI_CLIB, "%w%w%w%w%w", CL_Remove|HtoT,
                     DebugID, OSInfo1, OSInfo2, err?-1:NoError);
    }

  case CL_Rename:  /* rename file */
    {
      /* Rename(word nbytes, bytes oname, word nbytes, bytes nname)
      * return(byte status)
      */
      unsigned int len2;

      unpack_message(buffp, "%w", &len);
      DebugCheckNullTermString("CL_Rename", FALSE, len, buffp+4);
      unpack_message(buffp+5+len, "%w", &len2);
      DebugCheckNullTermString("to", TRUE, len2, buffp+9+len);

      /* Both names are passed with null terminators so we can use them
       * directly from the buffer. */
      err = rename((char *)buffp+4, (char *)buffp+9+len);
      stateptr->last_errno = errno;
      DebugCheckErr("rename", TRUE, err, stateptr->last_errno);
      DevSW_FreePacket(packet);

      return msgsend(CI_CLIB, "%w%w%w%w%w",  CL_Rename|HtoT,
                     DebugID, OSInfo1, OSInfo2, (err==0)? NoError : -1);
    }
  
  case CL_Open:    /* open the file */
    {
      /* Open(word nbytes, bytes name, byte mode)
      * return(word handle)
      */
      unpack_message(buffp, "%w", &len);
      /* get the open mode */
      unpack_message((buffp)+4+len+1, "%w", &mode);
      DebugCheckNullTermString("CL_Open", FALSE, len, buffp+4);
      DebugPrintF(("mode: %d\n", mode));

      /* do some checking on the file first? */
      /* check if its a tty */
      if (strcmp((char *)buffp+4, ":tt")==0 && (mode==0||mode==1)) {
        /* opening tty "r" */
        fhreal = stdin;
        stateptr->last_errno = errno;
        DebugPrintF(("\tstdin "));
      }
      else if (strcmp((char *)buffp+4, ":tt")== 0 && (mode==4||mode==5)) {
        /* opening tty "w" */
        fhreal = stdout;
        stateptr->last_errno = errno;
        DebugPrintF(("\tstdout "));
      }
      else
      {
        fhreal = fopen((char *)buffp+4, fmode[mode&0xFF]);
        stateptr->last_errno = errno;
        DebugCheckNonNull("fopen", FALSE, fhreal, stateptr->last_errno);
      }
      DevSW_FreePacket(packet);

      c = NONHANDLE;
      if (fhreal != NULL) {
        /* update filetable */
        for (c=3; c < HSYS_FOPEN_MAX; c++) {
          /* allow for stdin, stdout, stderr (!!! WHY? MJG) */
          if (stateptr->OSptr->FileTable[c] == NULL) {
            stateptr->OSptr->FileTable[c]= fhreal;
            stateptr->OSptr->FileFlags[c]= mode & 1;
            DebugPrintF(("fh: %d\n", c));
            break;
          }
          else if (c == HSYS_FOPEN_MAX) {
          /* no filehandles free */
          DebugPrintF(("no free fh: %d\n", c));
          stateptr->last_errno = EMFILE;
          }
        }
      }
      else {
        /*        c = NULL;*/
        DebugPrintF(("error fh: %d\n", c));
      }
      (void) msgsend(CI_CLIB, "%w%w%w%w%w",  CL_Open|HtoT,
                     DebugID, OSInfo1, OSInfo2, c);
      return 0;
    }

  case CL_Close:   /* close the file pointed to by the filehandle */
    {
      unpack_message(buffp, "%w", &fh);
      DebugPrintF(("CL_Close: fh %d\n", fh));
      DevSW_FreePacket(packet);

      fhreal = hsysGetRealFileHandle(stateptr, fh, NULL);
      if (fhreal == NULL)
         err = -1;
      else {
          if (fhreal == stdin || fhreal == stdout || fhreal == stderr) {
              stateptr->last_errno = errno;
              DebugPrintF(("\tskipping close of std*\n"));
              err = 0;
          }
          else {
              err = fclose(fhreal);
              if (err == 0)
                 stateptr->OSptr->FileTable[fh]=NULL;
              stateptr->last_errno = errno;
              DebugCheckErr("fclose", TRUE, err, stateptr->last_errno);
          }
      }
      return msgsend(CI_CLIB,"%w%w%w%w%w",  CL_Close|HtoT, DebugID,
                     OSInfo1, OSInfo2, err);
    }

  case CL_Write:
    {
        /* Write(word handle, word nbtotal, word nbytes, bytes data)
         * return(word nbytes)
         * WriteX(word nbytes, bytes data)
         * return(word nbytes)
         */
      unsigned char *rwdata = NULL, *rwhead = NULL;
      unsigned char *write_source = NULL;
      char flags;
      FILE *fhreal;
      unsigned int ack_reason = CL_Write; /* first ack is for CL_Write */

      err = -1;                 /* err == 0 is fwrite() error indication */
      unpack_message(buffp, "%w%w%w", &fh, &nbtotal, &nbytes); 
      DebugPrintF(("CL_Write: fh %d nbtotal %u nbytes %u\n",
                   fh, nbtotal, nbytes));

      fhreal = hsysGetRealFileHandle(stateptr, fh, &flags);
      nbtogo = nbtotal;

      /* deal with the file handle */
      if (fhreal == NULL)
         err = 0;
      else {
        if (flags & READOP)
           fseek(fhreal,0,SEEK_CUR);
        stateptr->OSptr->FileFlags[fh] = (flags & BINARY) | WRITEOP;

        nbtogo -= nbytes;

        if (nbtogo > 0) {
          write_source = rwdata = rwhead = (unsigned char *)malloc(nbtotal);
          if (rwhead == NULL) {
            fprintf(stderr, "OUT OF MEMORY at line %d in %s\n",
                    __LINE__, __FILE__);
            return -1;
          }
          memcpy(rwdata, buffp+12, nbytes);
          rwdata += nbytes;
        }
        else
           write_source = buffp+12;
      }

      do {
        /* at least once!! */

        if (nbtogo == 0 && err != 0) {
          /* Do the actual write! */
          if (fhreal == stdout || fhreal == stderr) {
            stateptr->hostif->write(stateptr->hostif->hostosarg,
                                    (char *)write_source, nbtotal);
          }
          else 
             err = fwrite(write_source, 1, nbtotal, fhreal);
          stateptr->last_errno = errno;
          DebugCheckErr("fwrite", TRUE, (err == 0), stateptr->last_errno);
        }

        DevSW_FreePacket(packet);
        if (msgsend(CI_CLIB,"%w%w%w%w%w%w", ack_reason|HtoT,
                    DebugID, OSInfo1, OSInfo2, (err == 0), nbtogo))
        {
            fprintf(stderr, "COULD NOT REPLY at line %d in %s\n",
                    __LINE__, __FILE__);
            if (rwhead != NULL)
               free(rwhead);
            return -1;
        }

        if (nbtogo == 0 || err == 0) {
          DebugPrintF(("\twrite complete - returning\n"));
          if (rwhead != NULL)
             free(rwhead);
          return 0;
        }
        else {
          /* await extension */
          ack_reason = CL_WriteX;

          packet = DevSW_AllocatePacket(Armsd_BufferSize);
          if (packet == NULL)
          {
            fprintf(stderr, "COULD NOT ALLOC PACKET at line %d in %s\n",
                    __LINE__, __FILE__);
            if (rwhead != NULL)
               free(rwhead);
            return -1;
          }
          Adp_ChannelRegisterRead(CI_CLIB, NULL, NULL);
          Adp_ChannelRead(CI_CLIB, &packet);
          Adp_ChannelRegisterRead(CI_CLIB,
                                  (ChannelCallback)HandleSysMessage,
                                  stateptr);

          buffhead = packet->pk_buffer;
          unpack_message(BUFFERDATA(buffhead), "%w%w%w%w%w", &reason_code,
                         &DebugID, &OSInfo1, &OSInfo2, &nbytes); 
          if (reason_code != (CL_WriteX|TtoH)) {
            DevSW_FreePacket(packet);
            free(rwhead);
            fprintf(stderr, "EXPECTING CL_WriteX GOT %u at line %d in %s\n",
                    reason_code, __LINE__, __FILE__);
            return -1;
          }

          DebugPrintF(("CL_WriteX: nbytes %u\n", nbytes));
          memcpy(rwdata, BUFFERDATA(buffhead)+20, nbytes);
          rwdata += nbytes;
          nbtogo -= nbytes;
        }

      } while (TRUE);           /* will return when done */
    }

  case CL_WriteX:     /*
                       * NOTE: if we've got here something has gone wrong
                       * CL_WriteX's should all be picked up within the
                       * CL_Write loop, probably best to return an error here
                       * do this for the moment just so we do actually return
                       */
    fprintf(stderr, "ERROR: unexpected CL_WriteX message received\n");
    return -1; 

  case CL_Read:
    {
                   /* Read(word handle, word nbtotal)
                    * return(word nbytes, word nbmore, bytes data)
                    */
                   /* ReadX()
                    * return(word nbytes, word nbmore, bytes data) */
      unsigned char *rwdata, *rwhead;
      int gotlen;
      unsigned int max_data_in_buffer=Armsd_BufferSize-28;
      char flags;
      FILE *fhreal;
      unsigned int nbleft = 0, reason = CL_Read;

      err = NoError;

      unpack_message(buffp, "%w%w", &fh, &nbtotal);
      DebugPrintF(("CL_Read: fh %d, nbtotal %d: ", fh, nbtotal));

      rwdata = rwhead = (unsigned char *)malloc(nbtotal);
      if (rwdata == NULL) {
        fprintf(stderr, "OUT OF MEMORY at line %d in %s\n",
                __LINE__, __FILE__);
        DevSW_FreePacket(packet);
        return -1;
      }

      /* perform the actual read */
      fhreal = hsysGetRealFileHandle(stateptr, fh, &flags);
      if (fhreal == NULL)
      {
        /* bad file handle */
        err = -1;
        nbytes = 0;
        gotlen = 0;
      }
      else
      {
        if (flags & WRITEOP)
          fseek(fhreal,0,SEEK_CUR);
        stateptr->OSptr->FileFlags[fh] = (flags & BINARY) | WRITEOP;
        if (isatty_(fhreal)) {
          /* reading from a tty, so do some nasty stuff, reading into rwdata */
          if (angel_hostif->gets(stateptr->hostif->hostosarg, (char *)rwdata,
                                 nbtotal) != 0)
             gotlen = strlen((char *)rwdata);
          else
             gotlen = 0;
          stateptr->last_errno = errno;
          DebugPrintF(("ttyread %d\n", gotlen));
        }
        else {
          /* not a tty, reading from a real file */
          gotlen = fread(rwdata, 1, nbtotal, fhreal);
          stateptr->last_errno = errno;
          DebugCheckErr("fread", FALSE, (gotlen == 0), stateptr->last_errno);
          DebugPrintF(("(%d)\n", gotlen));
        }
      }

      nbtogo = gotlen;

      do {
        /* at least once */

        if ((unsigned int) nbtogo <= max_data_in_buffer)
           nbytes = nbtogo;
        else
           nbytes = max_data_in_buffer;
        nbtogo -= nbytes;

        /* last ReadX needs subtle adjustment to returned nbtogo */
        if (nbtogo == 0 && err == NoError && reason == CL_ReadX)
           nbleft = nbtotal - gotlen;
        else
           nbleft = nbtogo;

        count = msgbuild(BUFFERDATA(buffhead), "%w%w%w%w%w%w%w",
                         reason|HtoT, 0, ADP_HandleUnknown,
                         ADP_HandleUnknown, err, nbytes, nbleft);

        if (err == NoError) {
          /* copy data into buffptr */   
          memcpy(BUFFERDATA(buffhead)+28, rwdata, nbytes);
          rwdata += nbytes;
          count += nbytes;
        }

        DebugPrintF(("\treplying err %d, nbytes %d, nbtogo %d\n",
                     err, nbytes, nbtogo));

        packet->pk_length = count;
        Adp_ChannelWrite(CI_CLIB, packet);

        if (nbtogo == 0 || err != NoError) {
          /* done */
          free(rwhead);
          return 0;
        }
        else {
          /* await extension */
          reason = CL_ReadX;

          packet = DevSW_AllocatePacket(Armsd_BufferSize);
          if (packet == NULL) {
            fprintf(stderr, "COULD NOT ALLOC PACKET at line %d in %s\n",
                    __LINE__, __FILE__);
            free(rwhead);
            return -1;
          }
          Adp_ChannelRegisterRead(CI_CLIB, NULL, NULL);
          Adp_ChannelRead(CI_CLIB, &packet);
          Adp_ChannelRegisterRead(CI_CLIB,
                                  (ChannelCallback)HandleSysMessage,
                                  stateptr);
          buffhead = packet->pk_buffer;
          unpack_message(BUFFERDATA(buffhead),"%w", &reason_code);
          if (reason_code != (CL_ReadX|TtoH)) {
            fprintf(stderr, "EXPECTING CL_ReadX GOT %u at line %d in %s\n",
                    reason_code, __LINE__, __FILE__);
            DevSW_FreePacket(packet);
            free(rwdata);
            return -1;
          }
        }

      } while (TRUE);           /* will return above on error or when done */
    }

  case CL_ReadX:      /* If we're here something has probably gone wrong */
    fprintf(stderr, "ERROR: Got unexpected CL_ReadX message\n");
    return -1;

  case CL_Seek:
    {
      unpack_message(buffp, "%w%w", &fh, &posn);
      DebugPrintF(("CL_Seek: fh %d, posn %ld\n", fh, posn));
      DevSW_FreePacket(packet);

      fhreal = hsysGetRealFileHandle(stateptr, fh, NULL);
      if (fhreal == NULL)
         err = -1;
      else {
        err = fseek(fhreal, posn, SEEK_SET); 
        stateptr->last_errno = errno;
        DebugCheckErr("fseek", TRUE, err, stateptr->last_errno);
      }

      return msgsend(CI_CLIB, "%w%w%w%w%w", CL_Seek|HtoT, 
                         DebugID, OSInfo1, OSInfo2, err);
    }

  case CL_Flen:
    {
      unpack_message(buffp, "%w", &fh);
      DebugPrintF(("CL_Flen: fh %d ", fh));
      DevSW_FreePacket(packet);

      fhreal = hsysGetRealFileHandle(stateptr, fh, NULL);
      if (fhreal == NULL)
        fl = -1;
      else {
        posn = ftell(fhreal);
        if (fseek(fhreal, 0L, SEEK_END) < 0) {
          fl=-1;
        }
        else {
          fl = ftell(fhreal);
          fseek(fhreal, posn, SEEK_SET);
        }
        stateptr->last_errno = errno;
      }
      DebugPrintF(("returning len %ld\n", fl));
      return msgsend(CI_CLIB, "%w%w%w%w%w", CL_Flen|HtoT, DebugID, OSInfo1,
                     OSInfo2, fl);
    } 

  case CL_IsTTY:
    {
      int  ttyOrNot;
      unpack_message(buffp, "%w", &fh);
      DebugPrintF(("CL_IsTTY: fh %d ", fh));
      DevSW_FreePacket(packet);

      fhreal = hsysGetRealFileHandle(stateptr, fh, NULL);
      if (fhreal == NULL)
         ttyOrNot = FALSE;
      else {
        ttyOrNot = isatty_(fhreal);
        stateptr->last_errno = errno;
      }
      DebugPrintF(("returning %s\n", ttyOrNot ? "tty (1)" : "not (0)"));

      return msgsend(CI_CLIB, "%w%w%w%w%w",CL_IsTTY|HtoT, 
                         DebugID, OSInfo1, OSInfo2, ttyOrNot);
    }

  case CL_TmpNam:
    {
      char *name;
      unsigned int tnamelen, TargetID;
      unpack_message(buffp, "%w%w", &tnamelen, &TargetID); 
      DebugPrintF(("CL_TmpNam: tnamelen %d TargetID %d: ",
                   tnamelen, TargetID));
      DevSW_FreePacket(packet);

      TargetID = TargetID & 0xFF;
      if (stateptr->OSptr->TempNames[TargetID] == NULL) {
        if ((stateptr->OSptr->TempNames[TargetID] =
             (char *)malloc(L_tmpnam)) == NULL)
        {
          fprintf(stderr, "OUT OF MEMORY at line %d in %s\n",
                  __LINE__, __FILE__);
          return -1;
        }
        tmpnam(stateptr->OSptr->TempNames[TargetID]);
      }
      name = stateptr->OSptr->TempNames[TargetID];
      len = strlen(name) + 1;
      packet = DevSW_AllocatePacket(Armsd_BufferSize);
      if (packet == NULL)
      {
          fprintf(stderr, "COULD NOT ALLOC PACKET at line %d in %s\n",
                  __LINE__, __FILE__);
          return -1;
      }
      buffhead = packet->pk_buffer;
      if (len > tnamelen) {
        DebugPrintF(("TMPNAME TOO LONG!\n"));
        count = msgbuild(BUFFERDATA(buffhead), "%w%w%w%w%w",
                           CL_TmpNam|HtoT, DebugID, OSInfo1, OSInfo2, -1);
      }
      else {
        DebugPrintF(("returning \"%s\"\n", name));
        count = msgbuild(BUFFERDATA(buffhead), "%w%w%w%w%w%w", CL_TmpNam|HtoT,
                         DebugID, OSInfo1, OSInfo2, 0, len);
        strcpy((char *)BUFFERDATA(buffhead)+count, name);
        count +=len+1;
      }
      packet->pk_length = count;
      Adp_ChannelWrite(CI_CLIB, packet);/* Send message. */
      return 0;
    }

  case CL_Unrecognised:
    DebugPrintF(("CL_Unrecognised!!\n"));
    return 0;

  default:
    fprintf(stderr, "UNRECOGNISED CL code %08x\n", reason_code);
    break;
/* Need some sort of error handling here. */
/* A call to CL_Unrecognised should suffice */
  }
  return -1;  /* Stop a potential compiler warning */
}

#ifdef COMPILING_ON_WINDOWS

#include <windows.h>

extern HWND hwndParent;

void panic(const char *format, ...)
{
    char buf[2048];
    va_list args;

    Adp_CloseDevice();

    va_start(args, format);
    vsprintf(buf, format, args);

    MessageBox(hwndParent, (LPCTSTR)buf, (LPCTSTR)"Fatal Error:", MB_OK);

    /* SJ - Not the proper way to shutdown the app */
    exit(EXIT_FAILURE);

/*
    if (hwndParent != NULL)
        SendMessage(hwndParent, WM_QUIT, 0, 0);
*/

    va_end(args);
}

#else

void panic(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    fprintf(stderr, "Fatal error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr,"\n");

    exit(EXIT_FAILURE);
}

#endif

/* EOF hsys.c */
