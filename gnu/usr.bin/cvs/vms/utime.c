/* This is REALLY gross, but at least it is a full implementation */ 

#include <ctype.h>
#include <time.h>
#include "vmsmunch.h"

#define ASCTIMEMAX 23

struct utimbuf {
  long actime;
  long modtime;
 }; 

utime(file, buf)
char *file;
struct utimbuf *buf; 
{
  static struct VMStimbuf vtb;
  static char conversion_buf[80];
  static char vms_actime[80];
  static char vms_modtime[80];

  strcpy(conversion_buf, ctime(&buf->actime));
  conversion_buf[ASCTIMEMAX + 1] = '\0';
  sprintf(vms_actime, "%2.2s-%3.3s-%4.4s %8.5s.00",
      &(conversion_buf[8]), &(conversion_buf[4]),
      &(conversion_buf[20]), &(conversion_buf[11]));
  vms_actime[4] = _toupper(vms_actime[4]);
  vms_actime[5] = _toupper(vms_actime[5]);

  strcpy(conversion_buf, ctime(&buf->modtime));
  conversion_buf[ASCTIMEMAX + 1] = '\0';
  sprintf(vms_modtime, "%2.2s-%3.3s-%4.4s %8.5s.00",
      &(conversion_buf[8]), &(conversion_buf[4]),
      &(conversion_buf[20]), &(conversion_buf[11]));
  vms_modtime[4] = _toupper(vms_modtime[4]);
  vms_modtime[4] = _toupper(vms_modtime[5]);

  vtb.actime = vms_actime;    
  vtb.modtime = vms_modtime;
  VMSmunch(file, SET_TIMES, &vtb); 
}
