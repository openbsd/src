
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :   iperlhost.h
 * DESCRIPTION  :   IPerlHost class file.
 * Author       :   SGP
 * Date Created :   January 2001.
 * Date Modified:   July 2nd 2001.
 */



#ifndef __iPerlHost_H__
#define __iPerlHost_H__


#include "EXTERN.h"
#include "perl.h"


class IPerlHost
{
public:
	virtual int VersionNumber() = 0;

	virtual int PerlCreate(PerlInterpreter *my_perl) = 0;
	virtual int PerlParse(PerlInterpreter *my_perl,int argc, char** argv, char** env) = 0;
	virtual int PerlRun(PerlInterpreter *my_perl) = 0;
	virtual void PerlDestroy(PerlInterpreter *my_perl) = 0;
	virtual void PerlFree(PerlInterpreter *my_perl) = 0;

	//virtual bool RegisterWithThreadTable(void)=0;
	//virtual bool UnregisterWithThreadTable(void)=0;
};

extern "C" IPerlHost* AllocStdPerl();
extern "C" void FreeStdPerl(IPerlHost* pPerlHost);


#endif	// __iPerlHost_H__

