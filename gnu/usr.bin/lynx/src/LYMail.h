
#ifndef LYMAIL_H
#define LYMAIL_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern BOOLEAN term_letter;

extern void mailform PARAMS((
	CONST char *	mailto_address,
	CONST char *	mailto_subject,
	CONST char *	mailto_content,
	CONST char *	mailto_type));
extern void mailmsg PARAMS((
	int 		cur,
	char *		owner_address,
	char *		filename,
	char *		linkname));
extern void reply_by_mail PARAMS((
	char *		mail_address,
	char *		filename,
	CONST char *	title,
	CONST char *	refid));

#endif /* LYMAIL_H */
