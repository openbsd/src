/*	$OpenBSD: sc.h,v 1.12 2002/06/17 07:10:52 deraadt Exp $ */

/*
 * Smartcard commander.
 * Written by Jim Rees and others at University of Michigan.
 */

/*
 * copyright 2001
 * the regents of the university of michigan
 * all rights reserved
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

extern int port, fd, cla, aut0_vfyd;
extern FILE *cmdf;

extern struct dispatchtable {
	char	*cmd, *help;
	int	(*action) (int argc, char *argv[]);
} dispatch_table[];

int	dispatch(int argc, char *argv[]);
int	help(int argc, char *argv[]);
int	reset(int argc, char *argv[]);
int	dclose(int argc, char *argv[]);
int	quit(int argc, char *argv[]);
int	apdu(int argc, char *argv[]);
int	selfid(int argc, char *argv[]);
int	isearch(int argc, char *argv[]);
int	csearch(int argc, char *argv[]);
int	class(int argc, char *argv[]);
int	dread(int argc, char *argv[]);
int	dwrite(int argc, char *argv[]);
int	challenge(int argc, char *argv[]);
int	vfypin(int argc, char *argv[]);
int	chpin(int argc, char *argv[]);
int	ls(int argc, char *argv[]);
int	acl(int argc, char *argv[]);
int	jcreate(int argc, char *argv[]);
int	jdelete(int argc, char *argv[]);
int	jdefault(int argc, char *argv[]);
int	jatr(int argc, char *argv[]);
int	jdata(int argc, char *argv[]);
int	jlogin(int argc, char *argv[]);
int	jaut(int argc, char *argv[]);
int	jload(int argc, char *argv[]);
int	junload(int argc, char *argv[]);
int	jsetpass(int argc, char *argv[]);
