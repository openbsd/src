/* COPYRIGHT  (C)  1998
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 * 
 * PERMISSION IS GRANTED TO USE, COPY, CREATE DERIVATIVE WORKS 
 * AND REDISTRIBUTE THIS SOFTWARE AND SUCH DERIVATIVE WORKS 
 * FOR ANY PURPOSE, SO LONG AS THE NAME OF THE UNIVERSITY OF 
 * MICHIGAN IS NOT USED IN ANY ADVERTISING OR PUBLICITY 
 * PERTAINING TO THE USE OR DISTRIBUTION OF THIS SOFTWARE 
 * WITHOUT SPECIFIC, WRITTEN PRIOR AUTHORIZATION.  IF THE 
 * ABOVE COPYRIGHT NOTICE OR ANY OTHER IDENTIFICATION OF THE 
 * UNIVERSITY OF MICHIGAN IS INCLUDED IN ANY COPY OF ANY 
 * PORTION OF THIS SOFTWARE, THEN THE DISCLAIMER BELOW MUST 
 * ALSO BE INCLUDED.
 * 
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION 
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY 
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF 
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING 
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE 
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE 
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR 
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING 
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN 
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGES.
 */

#include "arla_local.h"

RCSID("$KTH: darla.c,v 1.6 2000/02/12 19:23:54 assar Exp $");

int DARLA_Open(DARLA_file *Dfp, char *fname, int oflag)
{

  int fd;

  fd = open(fname, oflag, 0600);
  arla_log(ADEBMISC, "DARLA_Open: errno=%d", errno); 
  if (fd > 0)
  {
    Dfp->fd = fd;
    Dfp->offset=0;
    Dfp->log_entries = 0;
  }

  return fd;
}

int DARLA_Close(DARLA_file *Dfp)
{
  int ret;
  
  ret = close(Dfp->fd);
  Dfp->fd = 0;
  Dfp->offset =0;
  arla_log(ADEBMISC, "DARLA_Close: ret=%d", ret);
  return ret;
}

int DARLA_Read(DARLA_file *Dfp, char *cp, int len)
{
  ssize_t read_size;
  
  if (Dfp->fd)
  {
    read_size = read (Dfp->fd, cp, len);
  }
  else
    read_size = 0;

  return read_size;
}

int DARLA_Write(DARLA_file *Dfp, char *cp, int len)
{
  ssize_t write_size;

  if (Dfp->fd)
  {
    write_size = write(Dfp->fd, cp, len);
  }
  else
    write_size = 0;
  
  return write_size;
}

int DARLA_Seek(DARLA_file *Dfp, int offset, int whence)
{

  off_t lseek_off;

  if (Dfp->fd)
  {
    lseek_off = lseek(Dfp->fd, offset, whence);
  }
  else
    lseek_off = 0;

  return lseek_off;
}
