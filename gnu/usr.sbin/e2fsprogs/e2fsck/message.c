/*
 * message.c --- print e2fsck messages (with compression)
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * print_e2fsck_message() prints a message to the user, using
 * compression techniques and expansions of abbreviations.
 *
 * The following % expansions are supported:
 *
 * 	%b	<blk>			block number
 * 	%B	<blkcount>		integer
 * 	%di	<dirent>->ino		inode number
 * 	%dn	<dirent>->name		string
 * 	%D	<dir> 			inode number
 * 	%g	<group>			integer
 * 	%i	<ino>			inode number
 * 	%Is	<inode> -> i_size
 * 	%Ib	<inode> -> i_blocks
 * 	%Il	<inode> -> i_links_count
 * 	%Im	<inode> -> i_mode
 * 	%IM	<inode> -> i_mtime
 * 	%IF	<inode> -> i_faddr
 * 	%If	<inode> -> i_file_acl
 * 	%Id	<inode> -> i_dir_acl
 * 	%j	<ino2>			inode number
 * 	%N	<num>
 *	%p	ext2fs_get_pathname of directory <ino>
 * 	%P	ext2fs_get_pathname of <dirent>->ino with <ino2> as
 * 			the containing directory.  (If dirent is NULL
 * 			then return the pathname of directory <ino2>)
 * 	%q	ext2fs_get_pathname of directory <dir>
 * 	%Q	ext2fs_get_pathname of directory <ino> with <dir> as
 * 			the containing directory.
 *
 * The following '@' expansions are supported:
 *
 * 	@b	block
 * 	@B	bitmap
 * 	@C	conflicts with some other fs block
 * 	@i	inode
 * 	@D	deleted
 * 	@d	directory
 * 	@e	entry
 * 	@E	Entry '%Dn' in %p (%i)
 * 	@F	for @i %i (%Q) is
 * 	@g	group
 * 	@l	lost+found
 * 	@L	is a link
 * 	@u	unattached
 * 	@r	root inode
 * 	@z	zero-length
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include "e2fsck.h"

#include "problem.h"

#ifdef __GNUC__
#define _INLINE_ __inline__
#else
#define _INLINE_
#endif

/*
 * This structure defines the abbreviations used by the text strings
 * below.  The first character in the string is the index letter.  An
 * abbreviation of the form '@<i>' is expanded by looking up the index
 * letter <i> in the table below.
 */
static const char *abbrevs[] = {
	"bblock",
	"Bbitmap",
	"Cconflicts with some other fs @b",
	"iinode",
	"Ddeleted",
	"ddirectory",
	"eentry",
	"E@e '%Dn' in %p (%i)",
	"Ffor @i %i (%Q) is",
	"ggroup",
	"llost+found",
	"Lis a link",
	"uunattached",
	"rroot @i",
	"sshould be",
	"zzero-length",
	"@@",
	0
	};

/*
 * Give more user friendly names to the "special" inodes.
 */
#define num_special_inodes	7
static const char *special_inode_name[] =
{
	"<The NULL inode>",			/* 0 */
	"<The bad blocks inode>", 		/* 1 */
	"/",					/* 2 */
	"<The ACL index inode>",		/* 3 */
	"<The ACL data inode>",			/* 4 */
	"<The boot loader inode>",		/* 5 */
	"<The undelete directory inode>"	/* 6 */
};

/*
 * This function prints a pathname, using the ext2fs_get_pathname
 * function
 */
static void print_pathname(ext2_filsys fs, ino_t dir, ino_t ino)
{
	errcode_t	retval;
	char		*path;

	if (!dir && (ino < num_special_inodes)) {
		fputs(special_inode_name[ino], stdout);
		return;
	}
	
	retval = ext2fs_get_pathname(fs, dir, ino, &path);
	if (retval)
		fputs("???", stdout);
	else {
		fputs(path, stdout);
		free(path);
	}
}

/*
 * This function handles the '@' expansion.  We allow recursive
 * expansion; an @ expression can contain further '@' and '%'
 * expressions. 
 */
static _INLINE_ void expand_at_expression(ext2_filsys fs, char ch,
					  struct problem_context *ctx,
					  int *first)
{
	const char **cpp, *str;
	
	/* Search for the abbreviation */
	for (cpp = abbrevs; *cpp; cpp++) {
		if (ch == *cpp[0])
			break;
	}
	if (*cpp) {
		str = (*cpp) + 1;
		if (*first && islower(*str)) {
			*first = 0;
			fputc(toupper(*str++), stdout);
		}
		print_e2fsck_message(fs, str, ctx, *first);
	} else
		printf("@%c", ch);
}

/*
 * This function expands '%kX' expressions
 */
static _INLINE_ void expand_inode_expression(char ch,
					       struct problem_context *ctx)
{
	struct ext2_inode	*inode;
	char *			time_str;
	time_t			t;

	if (!ctx || !ctx->inode)
		goto no_inode;
	
	inode = ctx->inode;
	
	switch (ch) {
	case 's':
		printf("%u", inode->i_size);
		break;
	case 'b':
		printf("%u", inode->i_blocks);
		break;
	case 'l':
		printf("%d", inode->i_links_count);
		break;
	case 'm':
		printf("0%o", inode->i_mode);
		break;
	case 'M':
		t = inode->i_mtime;
		time_str = ctime(&t);
		printf("%.24s", time_str);
		break;
	case 'F':
		printf("%u", inode->i_faddr);
		break;
	case 'f':
		printf("%u", inode->i_file_acl);
		break;
	case 'd':
		printf("%u", inode->i_dir_acl);
		break;
	default:
	no_inode:
		printf("%%I%c", ch);
		break;
	}
}

/*
 * This function expands '%dX' expressions
 */
static _INLINE_ void expand_dirent_expression(char ch,
					      struct problem_context *ctx)
{
	struct ext2_dir_entry	*dirent;
	int	len;
	
	if (!ctx || !ctx->dirent)
		goto no_dirent;
	
	dirent = ctx->dirent;
	
	switch (ch) {
	case 'i':
		printf("%u", dirent->inode);
		break;
	case 'n':
		len = dirent->name_len;
		if (len > EXT2_NAME_LEN)
			len = EXT2_NAME_LEN;
		if (len > dirent->rec_len)
			len = dirent->rec_len;
		printf("%.*s", dirent->name_len, dirent->name);
		break;
	default:
	no_dirent:
		printf("%%D%c", ch);
		break;
	}
}

static _INLINE_ void expand_percent_expression(ext2_filsys fs, char ch,
					       struct problem_context *ctx)
{
	if (!ctx)
		goto no_context;
	
	switch (ch) {
	case '%':
		fputc('%', stdout);
		break;
	case 'b':
		printf("%u", ctx->blk);
		break;
	case 'B':
		printf("%d", ctx->blkcount);
		break;
	case 'd':
		printf("%lu", ctx->dir);
		break;
	case 'g':
		printf("%d", ctx->group);
		break;
	case 'i':
		printf("%lu", ctx->ino);
		break;
	case 'j':
		printf("%lu", ctx->ino2);
		break;
	case 'N':
		printf("%u", ctx->num);
		break;
	case 'p':
		print_pathname(fs, ctx->ino, 0);
		break;
	case 'P':
		print_pathname(fs, ctx->ino2,
			       ctx->dirent ? ctx->dirent->inode : 0);
		break;
	case 'q':
		print_pathname(fs, ctx->dir, 0);
		break;
	case 'Q':
		print_pathname(fs, ctx->dir, ctx->ino);
		break;
	default:
	no_context:
		printf("%%%c", ch);
		break;
	}
}	

void print_e2fsck_message(ext2_filsys fs, const char *msg,
			  struct problem_context *ctx, int first)
{
	const char *	cp;
	int		i;
	
	for (cp = msg; *cp; cp++) {
		if (cp[0] == '@') {
			cp++;
			expand_at_expression(fs, *cp, ctx, &first);
		} else if (cp[0] == '%' && cp[1] == 'I') {
			cp += 2;
			expand_inode_expression(*cp, ctx);
		} else if (cp[0] == '%' && cp[1] == 'D') {
			cp += 2;
			expand_dirent_expression(*cp, ctx);
		} else if ((cp[0] == '%')) {
			cp++;
			expand_percent_expression(fs, *cp, ctx);
		} else {
			for (i=0; cp[i]; i++)
				if ((cp[i] == '@') || cp[i] == '%')
					break;
			printf("%.*s", i, cp);
			cp += i-1;
		}
		first = 0;
	}
}
