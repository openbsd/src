
/* MODULE							HTVMSUtil.c
**		VMS Utility Routines
**
** AUTHORS:
**	MD	Mark Donszelmann    duns@vxdeop.cern.ch
**
** HISTORY:
**	14 Nov 93  MD	Written
**
** BUGS:
**
**
*/

#include <HTUtils.h>
#include <HTFormat.h>
#include <HTStream.h>
#include <UCDefs.h>
#include <UCMap.h>
#include <UCAux.h>
#include <HTVMSUtils.h>
#include <ssdef.h>
#include <jpidef.h>
#include <prvdef.h>
#include <acldef.h>
#include <chpdef.h>
#include <descrip.h>
#include <lib$routines.h>
#include <starlet.h>
#include <rmsdef.h>

#include <LYUtils.h>
#include <LYLeaks.h>

#define INFINITY 512            	/* File name length @@ FIXME */

PUBLIC BOOL HTVMSFileVersions=FALSE; /* Include version numbers in listing? */

typedef struct {
   unsigned long BufferLength : 16;
   unsigned long ItemCode : 16;
   unsigned long BufferAddress : 32;
   unsigned long ReturnLengthAddress : 32;
} ItemStruct;

extern CONST char * HTHostName NOPARAMS;

/* PUBLIC							HTVMS_authSysPrv()
**		CHECKS IF THIS PROCESS IS AUTHORIZED TO ENABLE SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	YES if SYSPRV is authorized
*/
PUBLIC BOOL HTVMS_authSysPrv NOARGS
{
unsigned long Result;
ItemStruct ItemList[2];
unsigned long Length;
unsigned long Buffer[2];

  /* fill Item */
  ItemList[0].BufferLength = sizeof(Buffer);
  ItemList[0].BufferAddress = (unsigned long)Buffer;
  ItemList[0].ReturnLengthAddress = (unsigned long)&Length;
  ItemList[0].ItemCode = JPI$_AUTHPRIV;

  /* terminate list */
  ItemList[1].ItemCode = 0;
  ItemList[1].BufferLength = 0;

  /* call system */
  Result = sys$getjpiw(0, 0, 0, ItemList, 0, 0, 0);

  if (Result != SS$_NORMAL)
     return(NO);

  if (Buffer[0] & PRV$M_SYSPRV)
     return(YES);

  return(NO);
}



/* PUBLIC							HTVMS_enableSysPrv()
**		ENABLES SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**
*/
PUBLIC void HTVMS_enableSysPrv NOARGS
{
unsigned long Result;
unsigned long Prv[2], PreviousPrv[2];

   Prv[0] = PRV$M_SYSPRV;
   Prv[1] = 0;
   Result = sys$setprv(1,&Prv,0,&PreviousPrv);

   if (Result == SS$_NORMAL) {
       if (!(PreviousPrv[0] & PRV$M_SYSPRV)) {
           CTRACE(tfp, "HTVMS_enableSysPrv: Enabled SYSPRV\n");
       }
   }
}



/* PUBLIC							HTVMS_disableSysPrv()
**		DISABLES SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**
*/
PUBLIC void HTVMS_disableSysPrv NOARGS
{
unsigned long Result;
unsigned long Prv[2], PreviousPrv[2];

   Prv[0] = PRV$M_SYSPRV;
   Prv[1] = 0;
   Result = sys$setprv(0,&Prv,0,&PreviousPrv);

   if (Result == SS$_NORMAL) {
       if (PreviousPrv[0] & PRV$M_SYSPRV) {
           CTRACE(tfp, "HTVMS_disableSysPrv: Disabled SYSPRV\n");
       }
   }
}



/* PUBLIC							HTVMS_checkAccess()
**		CHECKS ACCESS TO FILE FOR CERTAIN USER
** ON ENTRY:
**	FileName	The file to be accessed
**	UserName	Name of the user to check access for.
**			User nobody, represented by "" is given NO for an answer
**	Method		Name of the method to be chceked
**
** ON EXIT:
**	returns YES if access is allowed
**
*/
PUBLIC BOOL HTVMS_checkAccess ARGS3(
	CONST char *, FileName,
	CONST char *, UserName,
	CONST char *, Method)
{
unsigned long Result;
ItemStruct ItemList[2];
unsigned long Length;
unsigned long Buffer;
unsigned long ObjType;

char *VmsName;

struct dsc$descriptor_s FileNameDesc;
struct dsc$descriptor_s UserNameDesc;

char *colon;

   /* user nobody should access as from account under which server is running */
   if (0 == strcmp(UserName,""))
      return(NO);

   /* check Filename and convert */
   colon = strchr(FileName,':');
   if (colon)
      VmsName = HTVMS_name("",colon+1);
   else
      VmsName = HTVMS_name("",FileName);

   /* check for GET */
   if (0 == strcmp(Method,"GET"))
   {
     /* fill Item */
     ItemList[0].BufferLength = sizeof(Buffer);
     ItemList[0].BufferAddress = (unsigned long)&Buffer;
     ItemList[0].ReturnLengthAddress = (unsigned long)&Length;
     ItemList[0].ItemCode = CHP$_FLAGS;

     /* terminate list */
     ItemList[1].ItemCode = 0;
     ItemList[1].BufferLength = 0;

     /* fill input */
     ObjType = ACL$C_FILE;
     Buffer = CHP$M_READ;
     UserNameDesc.dsc$w_length = strlen(UserName);
     UserNameDesc.dsc$b_dtype = DSC$K_DTYPE_T;
     UserNameDesc.dsc$b_class = DSC$K_CLASS_S;
     UserNameDesc.dsc$a_pointer = (char *)UserName;
     FileNameDesc.dsc$w_length = strlen(VmsName);
     FileNameDesc.dsc$b_dtype = DSC$K_DTYPE_T;
     FileNameDesc.dsc$b_class = DSC$K_CLASS_S;
     FileNameDesc.dsc$a_pointer = VmsName;

     /* call system */
     Result = sys$check_access(&ObjType,&FileNameDesc,&UserNameDesc,ItemList);

     if (Result == SS$_NORMAL)
        return(YES);
     else
        return(NO);
   }

   return(NO);
}



/* PUBLIC							HTVMS_wwwName()
**		CONVERTS VMS Name into WWW Name
** ON ENTRY:
**	vmsname		VMS file specification (NO NODE)
**
** ON EXIT:
**	returns 	www file specification
**
** EXAMPLES:
**	vmsname				wwwname
**	DISK$USER 			disk$user
**	DISK$USER: 			/disk$user/
**	DISK$USER:[DUNS] 		/disk$user/duns
**	DISK$USER:[DUNS.ECHO] 		/disk$user/duns/echo
**	[DUNS] 				duns
**	[DUNS.ECHO] 			duns/echo
**	[DUNS.ECHO.-.TRANS] 		duns/echo/../trans
**	[DUNS.ECHO.--.TRANS] 		duns/echo/../../trans
**	[.DUNS] 			duns
**	[.DUNS.ECHO] 			duns/echo
**	[.DUNS.ECHO]TEST.COM 		duns/echo/test.com
**	TEST.COM 			test.com
**
**
*/
PUBLIC char * HTVMS_wwwName ARGS1(
	char *, vmsname)
{
static char wwwname[256];
char *src, *dst;
int dir;
   dst = wwwname;
   src = vmsname;
   dir = 0;
   if (strchr(src,':')) *(dst++) = '/';
   for ( ; *src != '\0' ; src++)
   {
      switch(*src)
      {
         case ':':  *(dst++) = '/'; break;
         case '-': if (dir)
	 	   {
	 	      if ((*(src-1)=='[' || *(src-1)=='.' || *(src-1)=='-') &&
		          (*(src+1)=='.' || *(src+1)=='-'))
		      {
		          *(dst++) = '/';
                          *(dst++) = '.';
                          *(dst++) = '.';
		      }
		      else
		          *(dst++) = '-';
		   }
		   else
		   {
		      if (*(src-1) == ']') *(dst++) = '/';
		      *(dst++) = '-';
		   }
                   break;
         case '.': if (dir)
                   {
                      if (*(src-1) != '[') *(dst++) = '/';
                   }
                   else
		   {
		      if (*(src-1) == ']') *(dst++) = '/';
                      *(dst++) = '.';
		   }
                   break;
         case '[': dir = 1; break;
         case ']': dir = 0; break;
         default:  if (*(src-1) == ']') *(dst++) = '/';
                   *(dst++) = *src;
                   break;
      }
   }
   *(dst++) = '\0';
   return(wwwname);
}


/* PUBLIC							HTVMS_name()
**		CONVERTS WWW name into a VMS name
** ON ENTRY:
**	nn		Node Name (optional)
**	fn		WWW file name
**
** ON EXIT:
**	returns 	vms file specification
**
** Bug:	Returns pointer to static -- non-reentrant
*/
PUBLIC char * HTVMS_name ARGS2(
	CONST char *, nn,
	CONST char *, fn)
{

/*	We try converting the filename into Files-11 syntax.  That is, we assume
**	first that the file is, like us, on a VMS node.  We try remote
**	(or local) DECnet access.  Files-11, VMS, VAX and DECnet
**	are trademarks of Digital Equipment Corporation.
**	The node is assumed to be local if the hostname WITHOUT DOMAIN
**	matches the local one. @@@
*/
    static char vmsname[INFINITY];	/* returned */
    char * filename = (char*)malloc(strlen(fn)+1);
    char * nodename = (char*)malloc(strlen(nn)+2+1);	/* Copies to hack */
    char *second;		/* 2nd slash */
    char *last;			/* last slash */

    char * hostname = (char *)HTHostName();

    if (!filename || !nodename) outofmem(__FILE__, "HTVMSname");
    strcpy(filename, fn);
    strcpy(nodename, "");	/* On same node?  Yes if node names match */
    if (strncmp(nn,"localhost",9)) {
        char *p, *q;
        for (p=hostname, q=(char *)nn;
	     *p && *p!='.' && *q && *q!='.'; p++, q++){
	    if (TOUPPER(*p)!=TOUPPER(*q)) {
	        strcpy(nodename, nn);
		q = strchr(nodename, '.');	/* Mismatch */
		if (q) *q=0;			/* Chop domain */
		strcat(nodename, "::");		/* Try decnet anyway */
		break;
	    }
	}
    }

    second = strchr(filename+1, '/');		/* 2nd slash */
    last = strrchr(filename, '/');	/* last slash */

    if (!second) {				/* Only one slash */
	sprintf(vmsname, "%s%s", nodename, filename + 1);
    } else if(second==last) {		/* Exactly two slashes */
	*second = 0;		/* Split filename from disk */
	sprintf(vmsname, "%s%s:%s", nodename, filename+1, second+1);
	*second = '/';	/* restore */
    } else { 				/* More than two slashes */
	char * p;
	*second = 0;		/* Split disk from directories */
	*last = 0;		/* Split dir from filename */
	sprintf(vmsname, "%s%s:[%s]%s",
		nodename, filename+1, second+1, last+1);
	*second = *last = '/';	/* restore filename */
	for (p=strchr(vmsname, '['); *p!=']'; p++)
	    if (*p=='/') *p='.';	/* Convert dir sep.  to dots */
    }
    FREE(nodename);
    FREE(filename);
    return vmsname;
}

/*
**	The code below is for directory browsing by VMS Curses clients.
**	It is based on the newer WWWLib's HTDirBrw.c. - Foteos Macrides
*/
PUBLIC int HTStat ARGS2(
	CONST char *, filename,
	struct stat *, info)
{
   /*
      the following stuff does not work in VMS with a normal stat...
      -->   /disk$user/duns/www if www is a directory
		is statted like: 	/disk$user/duns/www.dir
		after a normal stat has failed
      -->   /disk$user/duns	if duns is a toplevel directory
		is statted like:	/disk$user/000000/duns.dir
      -->   /disk$user since disk$user is a device
		is statted like:	/disk$user/000000/000000.dir
      -->   /
		searches all devices, no solution yet...
      -->   /vxcern!/disk$cr/wwwteam/login.com
		is not statted but granted with fake information...
   */
int Result;
int Len;
char *Ptr, *Ptr2;
char Name[256];

   /* try normal stat... */
   Result = stat((char *)filename,info);
   if (Result == 0)
      return(Result);

   /* make local copy */
   strcpy(Name,filename);

#ifdef NOT_USED
   /* if filename contains a node specification (! or ::), we will try to access
      the file via DECNET, but we do not stat it..., just return success
      with some fake information... */
   if (HTVMS_checkDecnet(Name))
   {
      /* set up fake info, only the one we use... */
      info->st_dev = NULL;
      info->st_ino[0] = 0;
      info->st_ino[1] = 0;
      info->st_ino[2] = 0;
      info->st_mode = S_IFREG | S_IREAD;	/* assume it is a regular Readable file */
      info->st_nlink = NULL;
      info->st_uid = 0;
      info->st_gid = 0;
      info->st_rdev = 0;
      info->st_size = 0;
      info->st_atime = time(NULL);
      info->st_mtime = time(NULL);
      info->st_ctime = time(NULL);

      return(0);
   }
#endif /* NOT_USED */

   /* failed,so do device search in case root is requested */
   if (!strcmp(Name,"/"))
   {  /* root requested */
      return(-1);
   }

   /* failed so this might be a directory, add '.dir' */
   Len = strlen(Name);
   if (Name[Len-1] == '/')
      Name[Len-1] = '\0';

   /* fail in case of device */
   Ptr = strchr(Name+1,'/');
   if ((Ptr == NULL) && (Name[0] == '/'))
   {  /* device only... */
      strcat(Name,"/000000/000000");
   }

   if (Ptr != NULL)
   {  /* correct filename in case of toplevel dir */
      Ptr2 = strchr(Ptr+1,'/');
      if ((Ptr2 == NULL) && (Name[0] == '/'))
      {
         char End[256];
         strcpy(End,Ptr);
         *(Ptr+1) = '\0';
         strcat(Name,"000000");
         strcat(Name,End);
      }
   }

   /* try in case a file on toplevel directory or .DIR was alreadyt specified */
   Result = stat(Name,info);
   if (Result == 0)
      return(Result);

   /* add .DIR and try again */
   strcat(Name,".dir");
   Result = stat(Name,info);
   return(Result);
}

#ifndef	_POSIX_SOURCE
#define	d_ino	d_fileno	/* compatability */
#ifndef	NULL
#define	NULL	0
#endif
#endif	/* !_POSIX_SOURCE */

typedef	struct __dirdesc {
#if 0
	int	dd_fd;		/* file descriptor */
	long	dd_loc;		/* buf offset of entry from last readddir() */
	long	dd_size;	/* amount of valid data in buffer */
	long	dd_bsize;	/* amount of entries read at a time */
	long	dd_off;		/* Current offset in dir (for telldir) */
	char	*dd_buf;	/* directory data buffer */
#endif
	long 	context;	/* context descriptor for LIB$FIND_FILE calls */
	char	dirname[255+1];	/* keeps the directory name, including *.* */
	struct dsc$descriptor_s dirname_desc;	/* descriptor of dirname */
} DIR;

PRIVATE	DIR *HTVMSopendir(char *dirname);
PRIVATE	struct dirent *HTVMSreaddir(DIR *dirp);
PRIVATE	int HTVMSclosedir(DIR *dirp);
#if 0
#ifndef	_POSIX_SOURCE
extern	void seekdir(/* DIR *dirp, int loc */);
extern	long telldir(/* DIR *dirp */);
#endif	/* POSIX_SOURCE */
extern	void rewinddir(/* DIR *dirp */);

#ifndef	lint
#define	rewinddir(dirp)	seekdir((dirp), (long)0)
#endif
#endif /* not defined for VMS */

/*** #include <sys_dirent.h> ***/
/*** "sys_dirent.h" ***/
struct	dirent {
#if 0
	off_t		d_off;		/* offset of next disk dir entry */
#endif
	unsigned long	d_fileno;	/* file number of entry */
#if 0
	unsigned short	d_reclen;	/* length of this record */
#endif
	unsigned short	d_namlen;	/* length of string in d_name */
	char		d_name[255+1];	/* name (up to MAXNAMLEN + 1) */
};

#ifndef	_POSIX_SOURCE
/*
 * It's unlikely to change, but make sure that sizeof d_name above is
 * at least MAXNAMLEN + 1 (more may be added for padding).
 */
#define	MAXNAMLEN	255
/*
 * The macro DIRSIZ(dp) gives the minimum amount of space required to represent
 * a directory entry.  For any directory entry dp->d_reclen >= DIRSIZ(dp).
 * Specific filesystem types may use this macro to construct the value
 * for d_reclen.
 */
#undef	DIRSIZ
#define	DIRSIZ(dp) \
	(((sizeof(struct dirent) - (MAXNAMLEN+1) + ((dp)->d_namlen+1)) +3) & ~3)

#endif	/* !_POSIX_SOURCE */


PRIVATE DIR *HTVMSopendir(char *dirname)
{
static DIR dir;
char *closebracket;
long status;
struct dsc$descriptor_s entryname_desc;
struct dsc$descriptor_s dirname_desc;
char DirEntry[256];
char VMSentry[256];
char UnixEntry[256];
int index;
char *dot;

   /* check if directory exists */
   /* dirname can look like /disk$user/duns/www/test/multi    */
   /* or like               /disk$user/duns/www/test/multi/   */
   /* DirEntry should look like     disk$user:[duns.www.test]multi in both cases */
   /* dir.dirname should look like  disk$user:[duns.www.test.multi] */
   strcpy(UnixEntry,dirname);
   if (UnixEntry[strlen(UnixEntry)-1] != '/')
      strcat(UnixEntry,"/");

   strcpy(DirEntry, HTVMS_name("",UnixEntry));
   strcpy(dir.dirname, DirEntry);
   index = strlen(DirEntry) - 1;

   if (DirEntry[index] == ']')
      DirEntry[index] = '\0';

   if ((dot = strrchr(DirEntry,'.')) == NULL)
   {  /* convert disk$user:[duns] into disk$user:[000000]duns.dir */
      char *openbr = strrchr(DirEntry,'[');
      if (!openbr)
      { /* convert disk$user: into disk$user:[000000]000000.dir */
         strcpy(dir.dirname, DirEntry);
         strcat(dir.dirname, "[000000]");
         strcat(DirEntry,"[000000]000000.dir");
      }
      else
      {
         char End[256];
         strcpy(End,openbr+1);
         *(openbr+1) = '\0';
         strcat(DirEntry,"000000]");
         strcat(DirEntry,End);
         strcat(DirEntry,".dir");
      }
   }
   else
   {
      *dot = ']';
      strcat(DirEntry,".dir");
   }

   dir.context = 0;
   dirname_desc.dsc$w_length = strlen(DirEntry);
   dirname_desc.dsc$b_dtype = DSC$K_DTYPE_T;
   dirname_desc.dsc$b_class = DSC$K_CLASS_S;
   dirname_desc.dsc$a_pointer = (char *)&(DirEntry);

   /* look for the directory */
   entryname_desc.dsc$w_length = 255;
   entryname_desc.dsc$b_dtype = DSC$K_DTYPE_T;
   entryname_desc.dsc$b_class = DSC$K_CLASS_S;
   entryname_desc.dsc$a_pointer = VMSentry;

   status = lib$find_file(&(dirname_desc),
                          &entryname_desc,
                          &(dir.context),
                          0,0,0,0);
   if (!(status & 0x01))
   { /* directory not found */
      return(NULL);
   }

#if 0
   /* now correct dirname, which looks like disk$user:[duns.www.test]multi */
   /* and should look like disk$user:[duns.www.test.multi] */
   closebracket = strchr(dir.dirname,']');
   *closebracket = '.';
   closebracket = strstr(dir.dirname,".dir");
   *closebracket = '\0';
   strcat(dir.dirname,"]");
#endif

   if (HTVMSFileVersions)
       strcat(dir.dirname,"*.*;*");
   else
       strcat(dir.dirname,"*.*");
   dir.context = 0;
   dir.dirname_desc.dsc$w_length = strlen(dir.dirname);
   dir.dirname_desc.dsc$b_dtype = DSC$K_DTYPE_T;
   dir.dirname_desc.dsc$b_class = DSC$K_CLASS_S;
   dir.dirname_desc.dsc$a_pointer = (char *)&(dir.dirname);
   return(&dir);
}

PRIVATE struct dirent *HTVMSreaddir(DIR *dirp)
{
static struct dirent entry;
long status;
struct dsc$descriptor_s entryname_desc;
char *space, *slash;
char VMSentry[256];
char *UnixEntry;

   entryname_desc.dsc$w_length = 255;
   entryname_desc.dsc$b_dtype = DSC$K_DTYPE_T;
   entryname_desc.dsc$b_class = DSC$K_CLASS_S;
   entryname_desc.dsc$a_pointer = VMSentry;

   status = lib$find_file(&(dirp->dirname_desc),
                          &entryname_desc,
                          &(dirp->context),
                          0,0,0,0);
   if (status == RMS$_NMF)
   { /* no more files */
      return(NULL);
   }
   else
   { /* ok */
      if (!(status & 0x01)) return(0);
      if (HTVMSFileVersions)
          space = strchr(VMSentry,' ');
      else
          space = strchr(VMSentry,';');
      if (space)
         *space = '\0';

      /* convert to unix style... */
      UnixEntry = HTVMS_wwwName(VMSentry);
      slash = strrchr(UnixEntry,'/') + 1;
      strcpy(entry.d_name,slash);
      entry.d_namlen = strlen(entry.d_name);
      entry.d_fileno = 1;
      return(&entry);
   }
}

PRIVATE int HTVMSclosedir(DIR *dirp)
{
long status;

   status = lib$find_file_end(&(dirp->context));
   if (!(status & 0x01)) exit(status);
   dirp->context = 0;
   return(0);
}

#include <HTAnchor.h>
#include <HTParse.h>
#include <HTBTree.h>
#include <HTFile.h>	/* For HTFileFormat() */
#include <HTAlert.h>
/*
**  Hypertext object building machinery.
*/
#include <HTML.h>
#define PUTC(c) (*targetClass.put_character)(target, c)
#define PUTS(s) (*targetClass.put_string)(target, s)
#define START(e) (*targetClass.start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*targetClass.end_element)(target, e, 0)
#define FREE_TARGET (*targetClass._free)(target)
#define ABORT_TARGET (*targetClass._free)(target)
struct _HTStructured {
	CONST HTStructuredClass *	isa;
	/* ... */
};

#define STRUCT_DIRENT struct dirent

PRIVATE char * months[12] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

typedef struct _VMSEntryInfo {
    char *       filename;
    char *       type;
    char *       date;
    unsigned int size;
    BOOLEAN      display;  /* show this entry? */
} VMSEntryInfo;

PRIVATE void free_VMSEntryInfo_contents ARGS1(VMSEntryInfo *,entry_info)
{
    if (entry_info) {
	FREE(entry_info->filename);
	FREE(entry_info->type);
	FREE(entry_info->date);
    }
   /* dont free the struct */
}

#define FILE_BY_NAME 0
#define FILE_BY_TYPE 1
#define FILE_BY_SIZE 2
#define FILE_BY_DATE 3
extern BOOLEAN HTfileSortMethod;  /* specifies the method of sorting */

PUBLIC int compare_VMSEntryInfo_structs ARGS2(VMSEntryInfo *,entry1,
					      VMSEntryInfo *,entry2)
{
    int i, status;
    char date1[16], date2[16], time1[8], time2[8], month[4];

    switch(HTfileSortMethod)
      {
        case FILE_BY_SIZE:
			/* both equal or both 0 */
                        if(entry1->size == entry2->size)
			    return(strcasecomp(entry1->filename,
					       entry2->filename));
			else
			    if(entry1->size > entry2->size)
				return(1);
			    else
				return(-1);
                        break;
        case FILE_BY_TYPE:
                        if(entry1->type && entry2->type) {
                            status = strcasecomp(entry1->type, entry2->type);
			    if(status)
				return(status);
			    /* else fall to filename comparison */
			}
                        return (strcasecomp(entry1->filename,
					    entry2->filename));
                        break;
        case FILE_BY_DATE:
                        if(entry1->date && entry2->date) {
			    /*
			    ** Make sure we have the correct length. - FM
			    */
			    if (strlen(entry1->date) != 12 ||
			        strlen(entry2->date) != 12) {
				return (strcasecomp(entry1->filename,
						    entry2->filename));
			    }
			    /*
			    ** Set up for sorting in reverse
			    ** chronological order. - FM
			    */
			    if (entry1->date[7] != ' ') {
			        strcpy(date1, "9999");
				strcpy(time1, (char *)&entry1->date[7]);
			    } else {
				strcpy(date1, (char *)&entry1->date[8]);
			        strcpy(time1, "00:00");
			    }
			    strncpy(month, entry1->date, 3);
			    month[3] = '\0';
			    for (i = 0; i < 12; i++) {
			        if (!strcasecomp(month, months[i])) {
				    break;
				}
			    }
			    i++;
			    sprintf(month, "%s%d", (i < 10 ? "0" : ""), i);
			    strcat(date1, month);
			    strncat(date1, (char *)&entry1->date[4], 2);
			    date1[8] = '\0';
			    if (date1[6] == ' ') {
			        date1[6] = '0';
			    }
			    strcat(date1, time1);
			    if (entry2->date[7] != ' ') {
			        strcpy(date2, "9999");
				strcpy(time2, (char *)&entry2->date[7]);
			    } else {
				strcpy(date2, (char *)&entry2->date[8]);
			        strcpy(time2, "00:00");
			    }
			    strncpy(month, entry2->date, 3);
			    month[3] = '\0';
			    for (i = 0; i < 12; i++) {
			        if (!strcasecomp(month, months[i])) {
				    break;
				}
			    }
			    i++;
			    sprintf(month, "%s%d", (i < 10 ? "0" : ""), i);
			    strcat(date2, month);
			    strncat(date2, (char *)&entry2->date[4], 2);
			    date2[8] = '\0';
			    if (date2[6] == ' ') {
			        date2[6] = '0';
			    }
			    strcat(date2, time2);
			    /*
			    ** Do the comparison. - FM
			    */
                            status = strcasecomp(date2, date1);
			    if(status)
				return(status);
			    /* else fall to filename comparison */
			}
                        return (strcasecomp(entry1->filename,
					    entry2->filename));
                        break;
        case FILE_BY_NAME:
        default:
                        return (strcmp(entry1->filename,
					    entry2->filename));
      }
}


/*						    	HTVMSBrowseDir()
**
**	This function generates a directory listing as an HTML-object
**	for local file URL's.  It assumes the first two elements of
**	of the path are a device followed by a directory:
**
**		file://localhost/device/directory[/[foo]]
**
**	Will not accept 000000 as a directory name.
**	Will offer links to parent through the top directory, unless
**	a terminal slash was included in the calling URL.
**
**	Returns HT_LOADED on success, HTLoadError() messages on error.
**
**	Developed for Lynx by Foteos Macrides (macrides@sci.wfeb.edu).
*/
PUBLIC int HTVMSBrowseDir ARGS4(
	CONST char *,		address,
	HTParentAnchor *,	anchor,
	HTFormat,		format_out,
	HTStream *,		sink
)
{
    HTStructured* target;
    HTStructuredClass targetClass;
    char *pathname = HTParse(address, "", PARSE_PATH + PARSE_PUNCTUATION);
    char *tail = NULL;
    char *title = NULL;
    char *header = NULL;
    char *parent = NULL;
    char *relative = NULL;
    char *cp, *cp1;
    int  pathend, len;
    DIR *dp;
    struct stat file_info;
    time_t NowTime;
    static char ThisYear[8];
    VMSEntryInfo *entry_info=0;
    char string_buffer[64];
    extern BOOLEAN no_dotfiles, show_dotfiles;

    HTUnEscape(pathname);
    CTRACE(tfp,"HTVMSBrowseDir: Browsing `%s\'\n", pathname);

    /*
     *  Require at least two elements (presumably a device and directory)
     *  and disallow the device root (000000 directory).  Symbolic paths
     *  (e.g., sys$help) should have been translated and expanded (e.g.,
     *  to /sys$sysroot/syshlp) before calling this routine.
     */
    if (((*pathname != '/') ||
    	 (cp=strchr(pathname+1, '/')) == NULL ||
	 *(cp+1) == '\0' ||
	 0==strncmp((cp+1), "000000", 6)) ||
        (dp=HTVMSopendir(pathname)) == NULL) {
        FREE(pathname);
    	return HTLoadError(sink, 403, COULD_NOT_ACCESS_DIR);
    }

    /*
     *  Set up the output stream.
     */
    _HTProgress (BUILDING_DIR_LIST);
    if (UCLYhndl_HTFile_for_unspec >= 0) {
	HTAnchor_setUCInfoStage(anchor,
				UCLYhndl_HTFile_for_unspec,
				UCT_STAGE_PARSER,
				UCT_SETBY_DEFAULT);
    }
    target = HTML_new(anchor, format_out, sink);
    targetClass = *(target->isa);

    /*
     *  Set up the offset string of the anchor reference,
     *  and strings for the title and header.
     */
    cp = strrchr(pathname, '/');  /* find lastslash */
    StrAllocCopy(tail, (cp+1)); /* take slash off the beginning */
    if (*tail != '\0') {
        StrAllocCopy(title, tail);
	*cp = '\0';
	if ((cp1=strrchr(pathname, '/')) != NULL &&
	    cp1 != pathname &&
	    strncmp((cp1+1), "000000", 6))
	    StrAllocCopy(parent, (cp1+1));
	*cp = '/';
    } else {
        pathname[strlen(pathname)-1] = '\0';
	cp = strrchr(pathname, '/');
	StrAllocCopy(title, (cp+1));
	pathname[strlen(pathname)] = '/';
    }
    StrAllocCopy(header, pathname);

    /*
     *  Initialize path name for HTStat().
     */
    pathend = strlen(pathname);
    if (*(pathname+pathend-1) != '/') {
	StrAllocCat(pathname, "/");
	pathend++;
    }

    /*
     *  Output the title and header.
     */
    START(HTML_HTML);
    PUTC('\n');
    START(HTML_HEAD);
    PUTC('\n');
    HTUnEscape(title);
    START(HTML_TITLE);
    PUTS(title);
    PUTS(" directory");
    END(HTML_TITLE);
    PUTC('\n');
    FREE(title);
    END(HTML_HEAD);
    PUTC('\n');
    START(HTML_BODY);
    PUTC('\n');
    HTUnEscape(header);
    START(HTML_H1);
    PUTS(header);
    END(HTML_H1);
    PUTC('\n');
    if (HTDirReadme == HT_DIR_README_TOP) {
        FILE * fp;
	if (header[strlen(header)-1] != '/')
	    StrAllocCat(header, "/");
	StrAllocCat(header, HT_DIR_README_FILE);
        if ((fp = fopen(header,  "r")) != NULL) {
	    START(HTML_PRE);
	    for(;;) {
	        char c = fgetc(fp);
	        if (c == (char)EOF)
		    break;
#ifdef NOTDEFINED
	        switch (c) {
	    	    case '&':
		    case '<':
		    case '>':
			PUTC('&');
			PUTC('#');
			PUTC((char)(c / 10));
			PUTC((char) (c % 10));
			PUTC(';');
			break;
		    default:
			PUTC(c);
	        }
#else
		PUTC(c);
#endif /* NOTDEFINED */
	    }
	    END(HTML_PRE);
	    fclose(fp);
        }
    }
    FREE(header);
    if (parent) {
	relative = (char*) malloc(strlen(tail) + 4);
	if (relative == NULL)
		outofmem(__FILE__, "HTVMSBrowseDir");
	sprintf(relative, "%s/..", tail);
	HTStartAnchor(target, "", relative);
	PUTS("Up to ");
	HTUnEscape(parent);
	PUTS(parent);
	END(HTML_A);
	START(HTML_P);
	PUTC('\n');
	FREE(relative);
	FREE(parent);
    }

    /*
     *  Set up the date comparison.
     */
    NowTime = time(NULL);
    strcpy(ThisYear, (char *)ctime(&NowTime)+20);
    ThisYear[4] = '\0';

    /*
     * Now, generate the Btree and put it out to the output stream.
     */
    {
	char dottest = 2;	/* To avoid two strcmp() each time */
	STRUCT_DIRENT *dirbuf;
	HTBTree *bt;

	/* Set up sort key and initialize BTree */
	bt = HTBTree_new((HTComparer) compare_VMSEntryInfo_structs);

	/* Build tree */
	while ((dirbuf = HTVMSreaddir(dp))) {
	    HTAtom *encoding = NULL;
	    HTFormat format;

	    /* Skip if not used */
	    if (!dirbuf->d_ino)	{
		continue;
	    }

	    /* Current and parent directories are never shown in list */
	    if (dottest && (!strcmp(dirbuf->d_name, ".") ||
			    !strcmp(dirbuf->d_name, ".."))) {
		dottest--;
		continue;
	    }

	    /* Don't show the selective enabling file
	     * unless version numbers are included */
	    if (!strcasecomp(dirbuf->d_name, HT_DIR_ENABLE_FILE)) {
		continue;
	    }

	    /* Skip files beginning with a dot? */
	    if ((no_dotfiles || !show_dotfiles) && *dirbuf->d_name == '.') {
		continue;
	    }

	    /* OK, make an lstat() and get a key ready. */
	    *(pathname+pathend) = '\0';
	    StrAllocCat(pathname, dirbuf->d_name);
	    if (HTStat(pathname, &file_info)) {
		/* for VMS the failure here means the file is not readable...
		   we however continue to browse through the directory... */
                continue;
	    }
            entry_info = (VMSEntryInfo *)malloc(sizeof(VMSEntryInfo));
	    if (entry_info == NULL)
		outofmem(__FILE__, "HTVMSBrowseDir");
	    entry_info->type = 0;
	    entry_info->size = 0;
	    entry_info->date = 0;
	    entry_info->filename = 0;
	    entry_info->display = TRUE;

	    /* Get the type */
	    format = HTFileFormat(dirbuf->d_name, &encoding,
				  (CONST char **)&cp);
	    if (!cp) {
		if(!strncmp(HTAtom_name(format), "application",11))
		{
		    cp = HTAtom_name(format) + 12;
		    if(!strncmp(cp,"x-", 2))
			cp += 2;
		}
		else
		    cp = HTAtom_name(format);
	    }
	    StrAllocCopy(entry_info->type, cp);

	    StrAllocCopy(entry_info->filename, dirbuf->d_name);
	    if (S_ISDIR(file_info.st_mode)) {
	        /* strip .DIR part... */
                char *dot;
                dot = strstr(entry_info->filename, ".DIR");
                if (dot)
                   *dot = '\0';
		LYLowerCase(entry_info->filename);
		StrAllocCopy(entry_info->type, "Directory");
	    } else {
	        if ((cp = strstr(entry_info->filename, "READ")) == NULL) {
	            cp = entry_info->filename;
		} else {
		    cp += 4;
		    if (!strncmp(cp, "ME", 2)) {
		        cp += 2;
			while (cp && *cp && *cp != '.') {
			    cp++;
			}
		    } else if (!strncmp(cp, ".ME", 3)) {
		        cp = (entry_info->filename +
			      strlen(entry_info->filename));
		    } else {
		        cp = entry_info->filename;
		    }
		}
		LYLowerCase(cp);
		if (((len = strlen(entry_info->filename)) > 2) &&
		    entry_info->filename[len-1] == 'z') {
		    if (entry_info->filename[len-2] == '.' ||
		        entry_info->filename[len-2] == '_')
			entry_info->filename[len-1] = 'Z';
		}
	    }

	    /* Get the date */
	    {
	        char *t = (char *)ctime((CONST time_t *)&file_info.st_ctime);
		*(t+24) = '\0';

	        StrAllocCopy(entry_info->date, (t+4));
		*((entry_info->date)+7) = '\0';
		if ((atoi((t+19))) < atoi(ThisYear))
		    StrAllocCat(entry_info->date,  (t+19));
		else {
		    StrAllocCat(entry_info->date, (t+11));
		    *((entry_info->date)+12) = '\0';
		}
	    }

	    /* Get the size */
	    if (!S_ISDIR(file_info.st_mode))
	        entry_info->size = (unsigned int)file_info.st_size;
	    else
	        entry_info->size = 0;

	    /* Now, update the BTree etc. */
	    if(entry_info->display)
	      {
		 CTRACE(tfp,"Adding file to BTree: %s\n",
						      entry_info->filename);
	         HTBTree_add(bt, (VMSEntryInfo *)entry_info);
	      }

	} /* End while HTVMSreaddir() */

	FREE(pathname);
	HTVMSclosedir(dp);

	START(HTML_PRE);
	/*
	 * Run through the BTree printing out in order
	 */
	{
	    HTBTElement * ele;
	    int i;
	    for (ele = HTBTree_next(bt, NULL);
		 ele != NULL;
		 ele = HTBTree_next(bt, ele))
	    {
		entry_info = (VMSEntryInfo *)HTBTree_object(ele);

		/* Output the date */
		if(entry_info->date)
		       {
		             PUTS(entry_info->date);
		             PUTS("  ");
		       }
		else
			PUTS("     * ");

		/* Output the type */
		if(entry_info->type)
		  {
		    for(i = 0; entry_info->type[i] != '\0' && i < 15; i++)
		        PUTC(entry_info->type[i]);
		    for(; i < 17; i++)
		        PUTC(' ');

		  }

		/* Output the link for the name */
		HTDirEntry(target, tail, entry_info->filename);
		PUTS(entry_info->filename);
		END(HTML_A);

                /* Output the size */
		if(entry_info->size)
		  {
		          if(entry_info->size < 1024)
			      sprintf(string_buffer,"  %d bytes",
							entry_info->size);
			  else
			      sprintf(string_buffer,"  %dKb",
							entry_info->size/1024);
			  PUTS(string_buffer);
		  }

		PUTC('\n'); /* end of this entry */

		free_VMSEntryInfo_contents(entry_info);
	    }
	}

	HTBTreeAndObject_free(bt);

    } /* End of both BTree loops */

    /*
     *  Complete the output stream.
     */
    END(HTML_PRE);
    PUTC('\n');
    END(HTML_BODY);
    PUTC('\n');
    END(HTML_HTML);
    PUTC('\n');
    FREE(tail);
    FREE_TARGET;

    return HT_LOADED;

} /* End of directory reading section */

/*
 * Remove all versions of the given file.  We assume there are no permissions
 * problems, since we do this mainly for removing temporary files.
 */
int HTVMS_remove(char *filename)
{
    int code = remove(filename);	/* return the first status code */
    while (remove(filename) == 0)
	;
    return code;
}

/*
 * Remove all older versions of the given file.  We may fail to remove some
 * version due to permissions -- the loop stops either at that point, or when
 * we run out of older versions to remove.
 */
void HTVMS_purge(char *filename)
{
    char *older_file = 0;
    char *oldest_file = 0;
    struct stat sb;

    StrAllocCopy(older_file, filename);
    StrAllocCat(older_file, ";-1");

    while (remove(older_file) == 0)
	;
    /*
     * If we do not have any more older versions, it is safe to rename the
     * current file to version #1.
     */
    if (stat(older_file, &sb) != 0) {
	StrAllocCopy(oldest_file, filename);
	StrAllocCat(oldest_file, ";1");
	rename(older_file, oldest_file);
	FREE(oldest_file);
    }

    FREE(older_file);
}
