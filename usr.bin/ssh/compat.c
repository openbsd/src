#include "includes.h"
RCSID("$Id: compat.c,v 1.3 1999/11/22 21:02:38 markus Exp $");

#include "ssh.h"

int compat13=0;
void enable_compat13(void){
	verbose("Enabling compatibility mode for protocol 1.3");
	compat13=1;
}
