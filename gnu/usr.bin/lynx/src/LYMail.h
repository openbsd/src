#ifndef LYMAIL_H
#define LYMAIL_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef SH_EX
#define USE_BLAT_MAILER 1
#else
#define USE_BLAT_MAILER 0
#endif

#ifdef VMS
#define USE_VMS_MAILER 1
#else
#define USE_VMS_MAILER 0
#endif

/*
 * Ifdef's in case we have a working popen/pclose, useful for piping to the
 * mail program.
 */
#if !defined(HAVE_POPEN) || USE_VMS_MAILER || defined(DOSPATH) || defined(__CYGWIN__)
#define CAN_PIPE_TO_MAILER 0
#else
#define CAN_PIPE_TO_MAILER 1
#endif

extern BOOLEAN term_letter;

extern BOOLEAN LYMailPMDF NOPARAMS;
extern FILE *LYPipeToMailer NOPARAMS;
extern int LYSendMailFile PARAMS((
	char *		the_address,
	char *		the_filename,
	char *		the_subject,
	char *		the_ccaddr,
	char *		message));
extern void mailform PARAMS((
	CONST char *	mailto_address,
	CONST char *	mailto_subject,
	CONST char *	mailto_content,
	CONST char *	mailto_type));
extern void mailmsg PARAMS((
	int		cur,
	char *		owner_address,
	char *		filename,
	char *		linkname));
extern void reply_by_mail PARAMS((
	char *		mail_address,
	char *		filename,
	CONST char *	title,
	CONST char *	refid));

#endif /* LYMAIL_H */
