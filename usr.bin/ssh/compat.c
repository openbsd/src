#include "includes.h"
#include "ssh.h"

int compat13=0;
void enable_compat13(void){
	log("Enabling compatibility mode for protocol 1.3");
	compat13=1;
}
