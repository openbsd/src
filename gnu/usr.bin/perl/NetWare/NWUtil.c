
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWUtil.c
 * DESCRIPTION	:	Utility functions for NetWare implementation of Perl.
 * Author		:	HYAK
 * Date			:	Januray 2001.
 *
 */



#include "stdio.h"
#include "string.h"

#include <nwdsdefs.h>		// For "MAX_DN_BYTES"
#include <malloc.h>			// For "malloc" and "free"
#include <stdlib.h>			// For "getenv"
#include <ctype.h>			// For "isspace"

#include <process.h>
#include <unistd.h>
#include <errno.h>
#include <nwerrno.h>

#include <nwlocale.h>
#include <nwadv.h>

#include "nwutil.h"


#define TRUE	1
#define FALSE	0


/**
  Global variables used for better token parsing. When these were absent,
  token parsing was not correct when there were more number of arguments passed.
  These are used in fnCommandLineParser, fnSkipToken and fnScanToken to get/return
  the correct and updated pointer to the command line string.
**/
char *s1 = NULL;	// Used in fnScanToken.
char *s2 = NULL;	// Used in fnSkipToken.




/*============================================================================================

 Function		:	fnSkipWhite

 Description	:	This function skips the white space characters in the given string and
					returns the resultant value.

 Parameters 	:	s	(IN)	-	Input string.

 Returns		:	String.

==============================================================================================*/

char *fnSkipWhite(char *s)
{
	while (isspace(*s))
		s++;
	return s;
}



/*============================================================================================

 Function		:	fnNwGetEnvironmentStr

 Description	:	This function returns the NetWare environment string if available,
					otherwise returns the supplied default value

 Parameters 	:	name			(IN)	-	To hold the NetWare environment value.
					defaultvalue	(IN)	-	Default value.


 Returns		:	String.

==============================================================================================*/

char *fnNwGetEnvironmentStr(char *name, char *defaultvalue)
{
	char* ret = getenv(name);
	if (ret == NULL)
		ret = defaultvalue;
	return ret;
}



/*============================================================================================

 Function		:	fnCommandLineParser

 Description	:	This function parses the command line into argc/argv style of
					Number of params and array of params.

 Parameters 	:	pclp		(IN)	-	CommandLine structure.
					commandLine	(IN)	-	CommandLine String.
					preserverQuotes	(IN)	-	Indicates whether to preserve/copy the quotes or not.

 Returns		:	Nothing.

==============================================================================================*/

void fnCommandLineParser(PCOMMANDLINEPARSER pclp, char * commandLine, BOOL preserveQuotes)
{
	char *buffer = NULL;

	int index = 0;
	int do_delete = 1;
	int i=0, j=0, k=0;


	// +1 makes room for the terminating NULL
	buffer = (char *) malloc((strlen(commandLine) + 1) * sizeof(char));
	if (buffer == NULL)
	{
		pclp->m_isValid = FALSE;
		return;
	}

	if (preserveQuotes)
	{
		// No I/O redirection nor quote processing if preserveQuotes

		char *s = NULL;
		char *sSkippedToken = NULL;


		strcpy(buffer, commandLine);
		s = buffer;
		s = fnSkipWhite(s);		// Skip white spaces.

		s2 = s;	// Update the global pointer.


		pclp->sSkippedToken = (char *) malloc(MAX_DN_BYTES * sizeof(char));
		if(pclp->sSkippedToken == NULL)
		{
			pclp->m_isValid = FALSE;
			return;
		}

		while (*s && pclp->m_isValid)
		{
/****
// Commented since only one time malloc and free is enough as is done outside this while loop.
// It is not required to do them everytime the execution comes into this while loop.
// Still retained here. Remove this  once things are proved to be working fine to a good confident level,

			if(pclp->sSkippedToken)
			{
				free(pclp->sSkippedToken);
				pclp->sSkippedToken = NULL;
			}

			if(pclp->sSkippedToken == NULL)
			{
				pclp->sSkippedToken = (char *) malloc(MAX_DN_BYTES * sizeof(char));
				if(pclp->sSkippedToken == NULL)
				{
					pclp->m_isValid = FALSE;
					return;
				}
			}
****/

			// Empty the string.
			strncpy(pclp->sSkippedToken, "", (MAX_DN_BYTES * sizeof(char)));

			// s is advanced by fnSkipToken
			pclp->sSkippedToken = fnSkipToken(s, pclp->sSkippedToken);	// Collect the next command-line argument.

			s2 = fnSkipWhite(s2);	// s2 is already updated by fnSkipToken.
			s = s2;		// Update the local pointer too.

			fnAppendArgument(pclp, pclp->sSkippedToken);	// Append the argument into an array.
		}

		if(pclp->sSkippedToken)
		{
			free(pclp->sSkippedToken);
			pclp->sSkippedToken = NULL;
		}
	}
	else
	{
		char *s = NULL;

		strcpy(buffer, commandLine);
		s = buffer;
		s = fnSkipWhite(s);

		s1 = s;	// Update the global pointer.

		while (*s && pclp->m_isValid)
		{
			// s is advanced by fnScanToken
			// Check for I/O redirection here, *outside* of
			// fnScanToken(), so that quote-protected angle
			// brackets do NOT cause redirection.
			if (*s == '<')
			{
				s = fnSkipWhite(s+1); // get stdin redirection

				if(pclp->m_redirInName)
				{
					free(pclp->m_redirInName);
					pclp->m_redirInName = NULL;
				}

				if(pclp->m_redirInName == NULL)
				{
					pclp->m_redirInName = (char *) malloc(MAX_DN_BYTES * sizeof(char));
					if(pclp->m_redirInName == NULL)
					{
						pclp->m_isValid = FALSE;
						return;
					}
				}

				// Collect the next command-line argument.
				pclp->m_redirInName = fnScanToken(s, pclp->m_redirInName);

				s1 = fnSkipWhite(s1);	// s1 is already updated by fnScanToken.
				s = s1;		// Update the local pointer too.
			}
			else if (*s == '>')
			{
				s = fnSkipWhite(s+1); //get stdout redirection

				if(pclp->m_redirOutName)
				{
					free(pclp->m_redirOutName);
					pclp->m_redirOutName = NULL;
				}

				if(pclp->m_redirOutName == NULL)
				{
					pclp->m_redirOutName = (char *) malloc(MAX_DN_BYTES * sizeof(char));
					if(pclp->m_redirOutName == NULL)
					{
						pclp->m_isValid = FALSE;
						return;
					}
				}

				// Collect the next command-line argument.
				pclp->m_redirOutName = fnScanToken(s, pclp->m_redirOutName);

				s1 = fnSkipWhite(s1);	// s1 is already updated by fnScanToken.
				s = s1;		// Update the local pointer too.
			}
			else if (*s == '2' && s[1] == '>')
			{
				s = fnSkipWhite(s+2); // get stderr redirection

				if(pclp->m_redirErrName)
				{
					free(pclp->m_redirErrName);
					pclp->m_redirErrName = NULL;
				}

				if(pclp->m_redirErrName == NULL)
				{
					pclp->m_redirErrName = (char *) malloc(MAX_DN_BYTES * sizeof(char));
					if(pclp->m_redirErrName == NULL)
					{
						pclp->m_isValid = FALSE;
						return;
					}
				}

				// Collect the next command-line argument.
				pclp->m_redirErrName = fnScanToken(s, pclp->m_redirErrName);

				s1 = fnSkipWhite(s1);	// s1 is already updated by fnScanToken.
				s = s1;		// Update the local pointer too.
			}
			else if (*s == '&' && s[1] == '>')
			{
				s = fnSkipWhite(s+2); // get stdout+stderr redirection

				if(pclp->m_redirBothName)
				{
					free(pclp->m_redirBothName);
					pclp->m_redirBothName = NULL;
				}

				if(pclp->m_redirBothName == NULL)
				{
					pclp->m_redirBothName = (char *) malloc(MAX_DN_BYTES * sizeof(char));
					if(pclp->m_redirBothName == NULL)
					{
						pclp->m_isValid = FALSE;
						return;
					}
				}

				// Collect the next command-line argument.
				pclp->m_redirBothName = fnScanToken(s, pclp->m_redirBothName);

				s1 = fnSkipWhite(s1);	// s1 is already updated by fnScanToken.
				s = s1;		// Update the local pointer too.
			}
			else
			{
				if(pclp->nextarg)
				{
					free(pclp->nextarg);
					pclp->nextarg = NULL;
				}

				if(pclp->nextarg == NULL)
				{
					pclp->nextarg = (char *) malloc(MAX_DN_BYTES * sizeof(char));
					if(pclp->nextarg == NULL)
					{
						pclp->m_isValid = FALSE;
						return;
					}
				}

				// Collect the next command-line argument.
				pclp->nextarg = fnScanToken(s, pclp->nextarg);

				s1 = fnSkipWhite(s1);	// s1 is already updated by fnScanToken.
				s = s1;		// Update the local pointer too.

				// Append the next command-line argument into an array.
				fnAppendArgument(pclp, pclp->nextarg);
			}
		}
	}


	// The -{ option, the --noscreen option, the --autodestroy option, if present,
	// are processed now and removed from the argument vector.
	for(index=0; index < pclp->m_argc; )
	{
		// "-q" is replaced by "-{", because of clash with GetOpt - sgp - 7th Nov 2000
		// Copied from NDK build - Jan 5th 2001
		if (strncmp(pclp->m_argv[index], (char *)"-{", 2) == 0)
		{
			// found a -q option; grab the semaphore number
			sscanf(pclp->m_argv[index], (char *)"-{%x", &pclp->m_qSemaphore);
			fnDeleteArgument(pclp, index);		// Delete the argument from the list.
		}
		else if (strcmp(pclp->m_argv[index], (char *)"--noscreen") == 0)
		{
			// found a --noscreen option
			pclp->m_noScreen = 1;
			fnDeleteArgument(pclp, index);
		}
		else if (strcmp(pclp->m_argv[index], (char *)"--autodestroy") == 0)
		{
			// found a --autodestroy option - create a screen but close automatically
			pclp->m_AutoDestroy = 1;
			fnDeleteArgument(pclp, index);
		}
		else
			index++;
	}

	// pclp->m_isValid is TRUE if there are more than 2 command line parameters  OR
	// if there is only one command and if it is the comman PERL.
	pclp->m_isValid = ((pclp->m_argc >= 2) || ((pclp->m_argc > 0) && (stricmp(pclp->m_argv[0], LOAD_COMMAND) != 0)));

	if(buffer)
	{
		free(buffer);
		buffer = NULL;
	}

	return;
}



/*============================================================================================

 Function		:	fnAppendArgument

 Description	:	This function appends the arguments into a list.

 Parameters 	:	pclp	(IN)	-	CommandLine structure.
					new_arg	(IN)	-	The new argument to be appended.

 Returns		:	Nothing.

==============================================================================================*/

void fnAppendArgument(PCOMMANDLINEPARSER pclp, char *new_arg)
{
	char **new_argv = pclp->new_argv;

	int new_argv_len = pclp->m_argv_len*2;
	int i = 0, j = 0;


	// Lengthen the argument vector if there's not room for another.
	// Testing for 'm_argc+2' rather than 'm_argc+1' in the test guarantees 
	// that there'll always be a NULL terminator at the end of argv.
	if ((pclp->m_argc + 2) > pclp->m_argv_len)
	{
		new_argv = (char **) malloc(new_argv_len * sizeof(char*));	// get a longer arg-vector
		if (new_argv == NULL)
		{
			pclp->m_isValid = FALSE;
			return;
		}
		for(i=0; i<new_argv_len; i++)
		{
			new_argv[i] = (char *) malloc(MAX_DN_BYTES * sizeof(char));
			if (new_argv[i] == NULL)
			{
				for(j=0; j<i; j++)
				{
					if(new_argv[j])
					{
						free(new_argv[j]);
						new_argv[j] = NULL;
					}
				}
				if(new_argv)
				{
					free(new_argv);
					new_argv = NULL;
				}

				pclp->m_isValid = FALSE;
				return;
			}
		}

		for (i=0; i<pclp->m_argc; i++)
			strcpy(new_argv[i], pclp->m_argv[i]);  // copy old arg strings

		for(i=0; i<(pclp->m_argv_len); i++)
		{
			if(pclp->m_argv[i])
			{
				free(pclp->m_argv[i]);
				pclp->m_argv[i] = NULL;
			}
		}
		if (pclp->m_argv != NULL)
		{
			free(pclp->m_argv);
			pclp->m_argv = NULL;
		}


		pclp->m_argv = new_argv;
		pclp->m_argv_len = new_argv_len;

	}

	// Once m_argv is guaranteed long enough, appending the argument is a direct job.
	strcpy(pclp->m_argv[pclp->m_argc], new_arg);	// Appended the new argument.
	pclp->m_argc++;		// Increment the number of parameters appended.

	// The char array is emptied for all elements upto the end so that there are no
	// junk characters. If this is not done, then the issue is like this:
	// - Simple perl command like "perl" on the system console works fine for the first time.
	// - When "perl" is executed the second time, a new blank screen should come up
	//   which allows for editing also. This was not consistently working well.
	//   More so when the command was like, "perl   ", that is the name "perl" followed
	//   by a few blank spaces, it used to give error in opening file:
	//   "unable to open the file" since the filename would have some junk characters.
	// 
	// These issues are fixed through the code below.
	for(i=pclp->m_argc; i<pclp->m_argv_len; i++)
		strncpy(pclp->m_argv[i], "", (MAX_DN_BYTES * sizeof(char)));	// MAX_DN_BYTES is the size of pclp->m_argv[].


	// Fix for empty command line double quote abend - perl <.pl> ""
	if ((new_arg==NULL) || ((strlen(new_arg))<=0))
	{
		pclp->m_argc--;		// Decrement the number of parameters appended.
		pclp->m_isValid = FALSE;
		return;
	}


	return;
}



/*============================================================================================

 Function		:	fnSkipToken

 Description	:	This function collects the next command-line argument, breaking on
					unquoted white space. The quote symbols are copied into the output.
					White space has already been skipped.

 Parameters 	:	s	(IN)	-	Input string in which the token is skipped.
					r	(IN)	-	The resultant return string.

 Returns		:	String.

==============================================================================================*/

char *fnSkipToken(char *s, char *r)
{
	register char *t=NULL;
	register char quote = '\0'; // NULL, single quote, or double quote
	char ch = '\0';

	for (t=s; t[0]; t++)
	{
		ch = t[0];
		if (!quote)
		{
			if (isspace(ch))				// if unquoted whitespace...
			{
				break;						// ...end of token found
			}
			else if (ch=='"' || ch=='\'')	// if opening quote...
			{
				quote = ch;					// ...enter quote mode
			}
		}
		else
		{
			if (ch=='\\' && t[1]==quote)	// if escaped quote...
			{
				t++;						// ...skip backslash
			}
			else if (ch==quote)				// if close quote...
			{
				quote = 0;					// ...leave quote mode
			} 
		}
	}

	r = fnStashString(s, r, t-s);  // get heap-allocated token string
	t = fnSkipWhite(t);                // skip any trailing white space
	s = t;                           // return updated source pointer

	s2 = t;                           // return updated global source pointer

	return r;                        // return heap-allocated token string
}



/*============================================================================================

 Function		:	fnScanToken

 Description	:	This function collects the next command-line argument, breaking on
					unquoted white space or I/O redirection symbols. Quote symbols are not
					copied into the output.
					When called, any leading white space has already been skipped.

 Parameters 	:	x	(IN)	-	Input string in which the token is scanned.
					r	(IN)	-	The resultant return string.

 Returns		:	String.

==============================================================================================*/

char *fnScanToken(char *x, char *r)
{
	register char *s = x; // input string position
	register char *t = x; // output string position
	register char quote = '\0'; // either NULL, or single quote, or double quote
	register char ch = '\0';
	register char c = '\0';

	while (*s)
	{
		ch = *s; // invariant: ch != 0
		
		// look to see if we've reached the end of the token
		if (!quote)		// but don't look for token break if we're inside quotes
		{
			if (isspace(ch))
				break;          // break on whitespace
			if (ch=='>')
				break;              // break on ">"  (redirect stdout)
			if (ch=='<')
				break;              // break on "<"  (redirect stdin)
			if (ch=='&' && x[1]=='>')
				break; // break on "&>" (redirect both stdout & stderr)
		}
		
		// process the next source character
		if (ch=='\\' && (c=s[1]) && (c=='\\'||c=='>'||c=='<'||c==quote))
		{
			//-----------------if an escaped '\\', '>', '<', or quote...
			s++;            // ...skip over the backslash...
			*t++ = *s++;    // ...and copy the escaped character
		}
		else if (ch==quote)		// (won't match unless inside quotes because invariant ch!=0)
		{
			//-----------------if close quote...
			s++;            // ...skip over the quote...
			quote=0;        // ...and leave quote mode
		}
		else if (!quote && (ch=='"' || ch=='\''))
		{
			//-----------------if opening quote...
			quote = *s++;   // ...enter quote mode (remembering quote char, and skipping the quote)
		}
		else
		{		//----------if normal character...
			*t++ = *s++;    // ...copy the character
		}
	}

	// clean up return values
	r = fnStashString(x, r, t-x);  // get heap-allocated token string
	s = fnSkipWhite(s);                // skip any trailing white space
	x = s;                           // return updated source pointer

	s1 = s;                           // return updated global source pointer

	return r;
}



/*============================================================================================

 Function		:	fnStashString

 Description	:	This function return the heap-allocated token string.

 Parameters 	:	s	(IN)	-	Input string from which the token is extracted.
					buffer	(IN)	-	Return string.
					length	(IN)	-	Length of the token to be extracted.

 Returns		:	String.

==============================================================================================*/

char *fnStashString(char *s, char *buffer, int length)
{
	if (length <= 0)
	{
		// Copy "" instead of NULL since "" indicates that there is memory allocated having no/null value.
		// NULL indicates that there is no memory allocated to it!
		strcpy(buffer, "");
	}
	else
	{
		strncpy(buffer, s, length);
		buffer[length] = '\0';
	}

	return buffer;
}



/*============================================================================================

 Function		:	fnDeleteArgument

 Description	:	This function deletes an argument (that was originally appended) from the list.

 Parameters 	:	pclp	(IN)	-	CommandLine structure.
					index	(IN)	-	Index of the argument to be deleted.

 Returns		:	Nothing.

==============================================================================================*/

void fnDeleteArgument(PCOMMANDLINEPARSER pclp, int index)
{
	int i = index;


	// If index is greater than the no. of arguments, just return.
	if (index >= pclp->m_argc)
		return;

	// Move all the arguments after the index one up.
	while(i < (pclp->m_argv_len-1))
	{
		strcpy(pclp->m_argv[i], pclp->m_argv[i+1]);
		i++;
	}


	// Delete the last one and free memory.
	if ( pclp->m_argv[i] )
	{
		free(pclp->m_argv[i]);
		pclp->m_argv[i] = NULL;
	}


	pclp->m_argc--;		// Decrement the number of arguments.
	pclp->m_argv_len--;

	return;
}



/*============================================================================================

 Function		:	fnMy_MkTemp

 Description	:	This is a standard ANSI C mktemp for NetWare

 Parameters 	:	templatestr	(IN)	-	Input temp filename.

 Returns		:	String.

==============================================================================================*/

char* fnMy_MkTemp(char* templatestr)
{
	char* pXs=NULL;
	char numbuf[50]={'\0'};
	int count=0;
	char* pPid=NULL;

	char termchar = '\0';
	char letter = 'a';
	char letter1 = 'a';


	if (templatestr && (pXs = strstr(templatestr, (char *)"XXXXXX")))
	{
		// generate temp name
		termchar = pXs[6];
		ltoa(GetThreadID(), numbuf, 16);
//		numbuf[sizeof(numbuf)-1] = '\0';
		numbuf[strlen(numbuf)-1] = '\0';
		// beware! thread IDs are 8 hex digits on NW 4.11 and only the
		// lower digits seem to change, whereas on NW 5 they are in the
		// range of < 1000 hex or 3 hex digits in length. So the following
		// logic ensures we use the least significant portion of the number.
		if (strlen(numbuf) > 5)
			pPid = &numbuf[strlen(numbuf)-5];
		else
			pPid = numbuf;

/**
		Backtick operation uses temp files that are stored under NWDEFPERLTEMP
		directory. They are temporarily used and then cleaned up after usage.
		In cases where multiple backtick operations are used that call some
		complex scripts, new temp files will be created before the old ones are
		deleted. So, we need to have a provision to create many temp files.
		Hence the below logic. It is found that provision for 26 files may
		not be enough in some cases.

		This below logic allows 26 files (like, pla00015.tmp through plz00015.tmp)
		plus 6x26=676 (like, plaa0015.tmp through plzz0015.tmp)
**/

		letter = 'a';
		do
		{
			sprintf(pXs, (char *)"%c%05.5s", letter, pPid);
			pXs[6] = termchar;
			if (access(templatestr, 0) != 0)	// File does not exist
			{
				return templatestr;
			}
			letter++;
		} while (letter <= 'z');

		letter1 = 'a';
		do
		{
			letter = 'a';
			do
			{
				sprintf(pXs, (char *)"%c%c%04.5s", letter1, letter, pPid);
				pXs[6] = termchar;
				if (access(templatestr, 0) != 0)	// File does not exist
				{
					return templatestr;
				}
				letter++;
			} while (letter <= 'z');
			letter1++;
		} while (letter1 <= 'z');

		errno = ENOENT;
		return NULL;
	}
	else
	{
		errno = EINVAL;
		return NULL;
	}
}



/*============================================================================================

 Function		:	fnSystemCommand

 Description	:	This function constructs a system command from the given
					null-terminated argv array and runs the command on the system console.

 Parameters 	:	argv	(IN)	-	Array of input commands.
					argc	(IN)	-	Number of input parameters.

 Returns		:	Nothing.

==============================================================================================*/

void fnSystemCommand (char** argv, int argc)
{
	// calculate the size of a temp buffer needed
	int k = 0;
	int totalSize = 0;
	int bytes = 0;
	char* tempCmd = NULL;
	char* tptr = NULL;


	for(k=0; k<argc; k++)
		totalSize += strlen(argv[k]) + 1;

	tempCmd = (char *) malloc((totalSize+1) * sizeof(char));
	if (!tempCmd)
		return;
	tptr = tempCmd;

	for(k=0; k<argc; k++)
		tptr += sprintf(tptr, (char *)"%s ", argv[k]);
	*tptr = 0;

	if (stricmp(argv[0], PERL_COMMAND_NAME) == 0)
		fnInternalPerlLaunchHandler(tempCmd);	// Launch perl.
	else
		system(tempCmd);


	free(tempCmd);
	tempCmd = NULL;
	return;
}

