#ifdef DOSREAD

#include <sys/param.h>

short doserrno;
short doshandle = -1;
void bcopy(), pcpy();

void _read();

char iobuf[DEV_BSIZE];

char *doserrors[] ={
  /* 00 */ "no error",
         /* 01 */ "function number invalid",
         /* 02 */ "file not found",
         /* 03 */ "path not found",
         /* 04 */ "too many open files (no handles available)",
         /* 05 */ "access denied",
         /* 06 */ "invalid handle",
         /* 07 */ "memory control block destroyed",
         /* 08 */ "insufficient memory",
         /* 09 */ "memory block address invalid",
         /* 0A */ "environment invalid (usually >32K in length)",
         /* 0B */ "format invalid",
         /* 0C */ "access code invalid",
         /* 0D */ "data invalid",
         /* 0E */ "reserved",
         /* 0F */ "invalid drive",
         /* 10 */ "attempted to remove current directory",
         /* 11 */ "not same device",
         /* 12 */ "no more files",
         /* 13 */ "disk write-protected",
         /* 14 */ "unknown unit",
         /* 15 */ "drive not ready",
         /* 16 */ "unknown command",
         /* 17 */ "data error (CRC)",
         /* 18 */ "bad request structure length",
         /* 19 */ "seek error",
         /* 1A */ "unknown media type (non-DOS disk)",
         /* 1B */ "sector not found",
         /* 1C */ "printer out of paper",
         /* 1D */ "write fault",
         /* 1E */ "read fault",
         /* 1F */ "general failure",
         /* 20 */ "sharing violation",
         /* 21 */ "lock violation",
         /* 22 */ "disk change invalid (ES:DI -> media ID structure)(see #0839)",
         /* 23 */ "FCB unavailable",
         /* 24 */ "sharing buffer overflow",
         /* 25 */ "(DOS 4+) code page mismatch",
         /* 26 */ "(DOS 4+) cannot complete file operation (out of input)",
         /* 27 */ "(DOS 4+) insufficient disk space",
         /* 28 */ "Reserved error (0x28)",
         /* 29 */ "Reserved error (0x29)",
         /* 2A */ "Reserved error (0x2A)",
         /* 2B */ "Reserved error (0x2B)",
         /* 2C */ "Reserved error (0x2C)",
         /* 2D */ "Reserved error (0x2D)",
         /* 2E */ "Reserved error (0x2E)",
         /* 2F */ "Reserved error (0x2F)",
         /* 30 */ "Reserved error (0x30)",
         /* 31 */ "Reserved error (0x31)",
         /* 32 */ "network request not supported",
         /* 33 */ "remote computer not listening",
         /* 34 */ "duplicate name on network",
         /* 35 */ "network name not found",
         /* 36 */ "network busy",
         /* 37 */ "network device no longer exists",
         /* 38 */ "network BIOS command limit exceeded",
         /* 39 */ "network adapter hardware error",
         /* 3A */ "incorrect response from network",
         /* 3B */ "unexpected network error",
         /* 3C */ "incompatible remote adapter",
         /* 3D */ "print queue full",
         /* 3E */ "queue not full",
         /* 3F */ "not enough space to print file",
         /* 40 */ "network name was deleted",
         /* 41 */ "network: Access denied",
         /* 42 */ "network device type incorrect",
         /* 43 */ "network name not found",
         /* 44 */ "network name limit exceeded",
         /* 45 */ "network BIOS session limit exceeded",
         /* 46 */ "temporarily paused",
         /* 47 */ "network request not accepted",
         /* 48 */ "network print/disk redirection paused",
         /* 49 */ "network software not installed",
         /* 4A */ "unexpected adapter close",
         /* 4B */ "(LANtastic) password expired",
         /* 4C */ "(LANtastic) login attempt invalid at this time",
         /* 4D */ "(LANtastic v3+) disk limit exceeded on network node",
         /* 4E */ "(LANtastic v3+) not logged in to network node",
         /* 4F */ "reserved",
         /* 50 */ "file exists",
         /* 51 */ "reserved",
         /* 52 */ "cannot make directory",
         /* 53 */ "fail on INT 24h",
         /* 54 */ "(DOS 3.3+) too many redirections",
         /* 55 */ "(DOS 3.3+) duplicate redirection",
         /* 56 */ "(DOS 3.3+) invalid password",
         /* 57 */ "(DOS 3.3+) invalid parameter",
         /* 58 */ "(DOS 3.3+) network write fault",
         /* 59 */ "(DOS 4+) function not supported on network",
         /* 5A */ "(DOS 4+) required system component not installed",
         /* 64 */ "(MSCDEX) unknown error",
         /* 65 */ "(MSCDEX) not ready",
         /* 66 */ "(MSCDEX) EMS memory no longer valid",
         /* 67 */ "(MSCDEX) not High Sierra or ISO-9660 format",
         /* 68 */ "(MSCDEX) door open",
       };


void __dosread(buffer, count, copy)
      char *buffer;
      int count;
      void (*copy)();
{
  int size;
  int cnt2;

  while (count) {
    size=count;

    if (size>DEV_BSIZE)
      size=DEV_BSIZE;

    size=dosread(doshandle,iobuf,size);
    twiddle();
    copy(iobuf , buffer, size);
    buffer += size;
    count -= size;
  }
}

char *printdoserror(char *header)
{
  static char buf[32];
  int max=sizeof(doserrors)/sizeof(doserrors[0]);
  if (doserrno<max && doserrno>=0)
    printf("%s: %s\n",header,doserrors[doserrno]);
  else
    printf("%s: Unknown error %d\n",header,doserrno);
}

doclose()
{
  if (doshandle>=0) {
    if (dosclose(doshandle)<0) {
      printdoserror("Dosclose");
      doshandle = -1;
      return -1;
    }
  }
  return 0;
}

dosopenrd(char *cp)
{
  if (doshandle<0) {
    doshandle=dosopen(cp);
    if (doshandle<0) {
      printdoserror("dosopen");
      return -1;
    }
  }
  return 0;
}

#endif
