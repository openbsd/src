#include "includes.h"
RCSID("$Id: compat.c,v 1.4 1999/11/23 22:25:53 markus Exp $");

#include "ssh.h"

int compat13 = 0;

void 
enable_compat13(void)
{
	verbose("Enabling compatibility mode for protocol 1.3");
	compat13 = 1;
}
