
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :   interface.c
 * DESCRIPTION  :   Perl parsing and running functions.
 * Author       :   SGP
 * Date Created :   January 2001.
 * Date Modified:   July 2nd 2001. 
 */



#ifndef __Interface_H__
#define __Interface_H__


#include "iperlhost.h"


class ClsPerlHost :	public IPerlHost
{
public:
	ClsPerlHost(void);
	virtual ~ClsPerlHost(void);

	int VersionNumber();

	int PerlCreate(PerlInterpreter *my_perl);
	int PerlParse(PerlInterpreter *my_perl, int argc, char** argv, char** env);
	int PerlRun(PerlInterpreter *my_perl);
	void PerlDestroy(PerlInterpreter *my_perl);
	void PerlFree(PerlInterpreter *my_perl);

	//bool RegisterWithThreadTable(void);
	//bool UnregisterWithThreadTable(void);
};


#endif	// __Interface_H__

