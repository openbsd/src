/* dl_netware.xs
 * 
 * Platform:	NetWare 
 * Author:	SGP
 * Created:	21st July 2000
 * Last Modified: 23rd Oct 2000
 * Note: !!!Any modification to the xs file to be done to the one which is under netware directory!!!
 * Modification History
 * 23rd Oct - Failing to find nlms with long names fixed - sdbm_file
 */

/* 

NetWare related modifications done on dl_win32.xs file created by Wei-Yuen Tan to get this file.

*/


#include <nwthread.h> 
#include <nwerrno.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


//function pointer for UCSInitialize
typedef void (*PFUCSINITIALIZE) ();

#include "dlutils.c"	/* SaveError() etc	*/

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}


MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


void *
dl_load_file(filename,flags=0)
    char *		filename
    int			flags
    PREINIT:
    CODE:
  {
	char* mod_name = filename;

	//Names with more than 8 chars can't be found with FindNLMHandle
	//8 - Name, 1 - Period, 3 - Extension, 1 - String terminator
	char mod_name8[13]={'\0'};
	char *p=NULL;
	char *buffer=NULL;
	int nNameLength=0;
	unsigned int nlmHandle=0;

	while (*mod_name) mod_name++;
	
	//Get the module name with extension to see if it is already loaded
	while (mod_name > filename && mod_name[-1] != '/' && mod_name[-1] != '\\') mod_name--;

    DLDEBUG(1,PerlIO_printf(Perl_debug_log,"dl_load_file(%s):\n", filename));

	buffer = strdup(mod_name);
	p = strtok (buffer, "."); 
	if (p) {
		nNameLength = (strlen(p)>8)?8:strlen(p);
		memcpy(mod_name8,p,nNameLength);
		*(mod_name8 + nNameLength) = '.';
		*(mod_name8 + nNameLength+1) ='\0';
		p = strtok (NULL, ".");
		if (p){
			strcat(mod_name8,p);

			if ( (nlmHandle = FindNLMHandle(mod_name8)) == NULL )
			{
				//NLM/NLP not loaded, load it and get the handle
				if(spawnlp(P_NOWAIT, filename, filename, NULL)!=0)
				{
					//failed to load the NLM/NLP, this unlikely
					//If multiple scripts are executed for the first time before running any other
					//ucs script, sometimes there used to be an abend.
					switch(NetWareErrno)
					{
					case LOAD_CAN_NOT_LOAD_MULTIPLE_COPIES:
						nlmHandle = FindNLMHandle(mod_name8);
						break;
					case LOAD_ALREADY_IN_PROGRESS:
#ifdef MPK_ON
							kYieldThread();
#else
							ThreadSwitch();
#endif	//MPK_ON
						nlmHandle = FindNLMHandle(mod_name8);
						break;
					default:
						nlmHandle = 0;
					}
				}
				else
				{
					nlmHandle = FindNLMHandle(mod_name8);
				}
			}
			//use Perl2UCS or UCSExt encountered :
			//initialize UCS, this has to be terminated when the script finishes execution
			//Is the script intending to use UCS Extensions?
			//This should be done once per script execution
			if ((strcmp(mod_name,"Perl2UCS.nlm")==0) || (strcmp(mod_name,"UCSExt.nlm")==0))
			{
				unsigned int moduleHandle = 0;
				moduleHandle = FindNLMHandle("UCSCORE.NLM");
				if (moduleHandle)
				{
					PFUCSINITIALIZE ucsinit = (PFUCSINITIALIZE)ImportSymbol(moduleHandle,"UCSInitialize");
					if (ucsinit!=NULL)
						(*ucsinit)();
				}
			}

			DLDEBUG(2,PerlIO_printf(Perl_debug_log," libref=%x\n", nlmHandle));
			ST(0) = sv_newmortal() ;
			if (nlmHandle == NULL)
			//SaveError(aTHX_ "load_file:%s",
			//	  OS_Error_String(aTHX)) ;
			ConsolePrintf("load_file error :  %s\n", mod_name8);
			else
			sv_setiv( ST(0), (IV)nlmHandle);
		}
	}
	free(buffer);

	
  }

void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_find_symbol(handle=%x, symbol=%s)\n",
		      libhandle, symbolname));

	//import the symbol that the dynaloader is asking for.
	RETVAL = (void *)ImportSymbol((int)libhandle, symbolname);

    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"  symbolref = %x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	//SaveError(aTHX_ "find_symbol:%s",
	//	  OS_Error_String(aTHX)) ;
	ConsolePrintf("find_symbol error \n");
    else
	sv_setiv( ST(0), (IV)RETVAL);

void
dl_undef_symbols()
    PPCODE:


# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    char *		filename
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_install_xsub(name=%s, symref=%x)\n",
		      perl_name, symref));
    ST(0) = sv_2mortal(newRV((SV*)newXS(perl_name,
					(void(*)(pTHX_ CV *))symref,
					filename)));


char *
dl_error()
    CODE:
    dMY_CXT;
    RETVAL = dl_last_error ;
    OUTPUT:
    RETVAL

# end.


