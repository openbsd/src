/*	$OpenBSD: dyn.c,v 1.1 1999/08/28 11:54:55 niklas Exp $	*/
/*	$EOM: dyn.c,v 1.2 1999/08/26 11:13:36 niklas Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <dlfcn.h>

#include "sysdep.h"

#include "dyn.h"
#include "log.h"

int
dyn_load (struct dynload_script *scr)
{
  int i;
  void **desc = 0;

  for (i = 0; scr[i].op != EOS; i++)
    switch (scr[i].op)
      {
      case LOAD:
	desc = scr[i].ptr;
	*desc = dlopen (scr[i].name, DL_LAZY);
	if (!*desc)
	  {
	    log_print ("dyn_load: dlopen (\"%s\", DL_LAZY) failed: %s",
		       scr[i].name, dlerror ());
	    return 0;
	  }
	break;

      case SYM:
	if (!desc || !*desc)
	  continue;
	*scr[i].ptr = dlsym (*desc, scr[i].name);
	if (!*scr[i].ptr)
	  {
	    log_print ("dyn_load: dlsym (\"%s\") failed: %s", scr[i].name,
		       dlerror ());
	    *desc = 0;
	    return 0;
	  }
	break;

      default:
	log_print ("dyn_load: bad operation (%d) on entry %d, ignoring",
		   scr[i].op, i);
      }
  return 1;
}
