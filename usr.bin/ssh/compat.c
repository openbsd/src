#include "includes.h"
RCSID("$Id: compat.c,v 1.2 1999/10/16 22:29:01 markus Exp $");

#include "ssh.h"

int compat13=0;
void enable_compat13(void){
	log("Enabling compatibility mode for protocol 1.3");
	compat13=1;
}
