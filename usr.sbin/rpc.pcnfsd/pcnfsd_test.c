/*	$OpenBSD: pcnfsd_test.c,v 1.5 2003/02/15 12:15:04 deraadt Exp $	*/
/*	$NetBSD: pcnfsd_test.c,v 1.2 1995/07/25 22:21:01 gwr Exp $	*/

/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_v1.c	1.1	9/3/91
**
** pcnfsd is copyrighted software, but is freely licensed. This
** means that you are free to redistribute it, modify it, ship it
** in binary with your system, whatever, provided:
**
** - you leave the Sun copyright notice in the source code
** - you make clear what changes you have introduced and do
**   not represent them as being supported by Sun.
**
** If you make changes to this software, we ask that you do so in
** a way which allows you to build either the "standard" version or
** your custom version from a single source file. Test it, lint
** it (it won't lint 100%, very little does, and there are bugs in
** some versions of lint :-), and send it back to Sun via email
** so that we can roll it into the source base and redistribute
** it. We'll try to make sure your contributions are acknowledged
** in the source, but after all these years it's getting hard to
** remember who did what.
**=====================================================================
*/

#include <stdio.h>
#include <rpc/rpc.h>
#include <stdlib.h>
#include "pcnfsd.h"

CLIENT *cl;
CLIENT *cl2;
char *server;
char spooldirbuff[256];
char filenamebuff[256];
char last_id[32] = "";

void free_pr_list_item();
void free_pr_queue_item();
void good();
void bad();


main(argc, argv)
int argc;
char *argv[];
{

char *host_name;
char *printer;
char *user_name;
char *passwd;
char *transport = "udp";

	if((argc < 6) || (argc > 7)) {
		fprintf(stderr, "usage: %s server host printer user password [transport]\n",
			argv[0]);
		exit(1);
	}

	server = argv[1];
	host_name = argv[2];
	printer = argv[3];
	user_name = argv[4];
	passwd = argv[5];
	if (argc == 7)
		transport = argv[6];

	cl = clnt_create(server, PCNFSDPROG, PCNFSDVERS, transport);
	if(cl == NULL) {
		clnt_pcreateerror(server);
		exit(1);
	}
	cl2 = clnt_create(server, PCNFSDPROG, PCNFSDV2, transport);
	if(cl2 == NULL) {
		clnt_pcreateerror(server);
		exit(1);
	}
	good();
	test_v2_info();
	good();
	test_v2_auth(host_name, user_name, passwd);
	bad("Invalid password");
	test_v2_auth(host_name, user_name, "bogus");
	good();
	test_v2_list();
	good();
	test_v2_init(host_name, printer);
	good();
	test_v2_start(host_name, printer, user_name, "foo", "foo");
	good();
	test_v2_start(host_name, printer, user_name, "bar", "bar");
	bad("No such file to print");
	test_v2_start(host_name, printer, user_name, "bletch", "gack");
	good();
	test_v2_queue(printer, user_name, FALSE);
	if(strlen(last_id)) {
		bad("Cancelling job with bad username");
		test_v2_cancel(host_name, printer, "nosuchuser", last_id);
		good();
		test_v2_cancel(host_name, printer, user_name, last_id);
	}
	bad("Cancelling unknown job");
	test_v2_cancel(host_name, printer, user_name, "99999");
	bad("Cancelling job on invalid printer");
	test_v2_cancel(host_name, "nosuchprinter", user_name, last_id);
	good();
	test_v2_queue(printer, user_name, TRUE);
	bad("Checking queue on invalid printer");
	test_v2_queue("nosuchprinter", user_name, TRUE);
	good();
	test_v2_stat(printer);
	bad("Checking status of invalid printer");
	test_v2_stat("nosuchprinter");
	good();
	test_v2_map();
	exit(0);
/*NOTREACHED*/
}

#define zchar           0x5b

void
scramble(s1, s2)
char           *s1;
char           *s2;
{
        while (*s1) 
              {
              *s2++ = (*s1 ^ zchar) & 0x7f;
              s1++;
              }
        *s2 = 0;
}



test_v2_info()
{
v2_info_args a;
v2_info_results *rp;
int          *gp;
int             i;

	a.vers = "Sun Microsystems PCNFSD test subsystem V1";
	a.cm = "-";
	printf("\ninvoking pr_info_2\n");

	rp = pcnfsd2_info_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	
	printf("results: vers = '%s', cm = '%s'\n",
		rp->vers, rp->cm);
	printf("facilities_len = %d\n", rp->facilities.facilities_len);
	if (rp->facilities.facilities_len) {
		gp = rp->facilities.facilities_val;
		for(i = 0; i < rp->facilities.facilities_len; i++)
			printf(" procedure %2d: %6d\n", i, *gp++);
		printf("\n");
	}
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->facilities.facilities_val)
		free(rp->facilities.facilities_val);
	if(rp->vers)
		free(rp->vers);
	
	return(0);
}

test_v2_auth(host_name, user_name , pwrd)
char *host_name;
char *user_name;
char *pwrd;
{
v2_auth_args a;
v2_auth_results *rp;
char            uname[32];
char            pw[64];
u_int          *gp;
int             i;

	scramble(user_name, uname);
	scramble(pwrd, pw);
	a.system = host_name;
	a.id = uname;
	a.pw = pw;
	a.cm = "-";
	printf("\ninvoking pr_auth_2\n");

	rp = pcnfsd2_auth_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	
	if(rp->stat == AUTH_RES_FAIL)
		printf("results: stat = AUTH_RES_FAIL\n");
	else {
	printf("results: stat = %d, uid = %u, gid = %u,\n homedir= '%s', cm = '%s'\n",
		rp->stat, rp->uid, rp->gid, rp->home, rp->cm);
	printf("gids_len = %d", rp->gids.gids_len);
	if (rp->gids.gids_len) {
		gp = rp->gids.gids_val;
		for(i = 0; i < rp->gids.gids_len; i++)
			printf(" %u", *gp++);
		printf("\n");
	}
	}
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->gids.gids_val)
		free(rp->gids.gids_val);
	if(rp->home)
		free(rp->home);
	
	return(0);
}

test_v2_init(host_name, printer)
char *host_name;
char *printer;
{
v2_pr_init_args a;
v2_pr_init_results *rp;

	a.system = host_name;
	a.pn = printer;
	a.cm = "-";
	printf("\ninvoking pr_init_2\n");

	rp = pcnfsd2_pr_init_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: stat = %d, dir = '%s', cm = '%s'\n",
		rp->stat, rp->dir, rp->cm);
	strcpy(spooldirbuff, rp->dir);
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->dir)
		free(rp->dir);
	return(0);
}


test_v2_start(host_name, printer, user_name, tag1, tag2)
char *host_name;
char *printer;
char *user_name;
char *tag1;
char *tag2;
{
v2_pr_start_args a;
v2_pr_start_results *rp;
FILE *fp;
	printf("\ntesting start print v2\n");

	if(strcmp(server, "localhost")) {
		printf("sorry - can only test start print on 'localhost'\n");
		return(1);
	}

	snprintf(filenamebuff, sizeof filenamebuff, "%s/%s", spooldirbuff, tag1);

	fp = fopen(filenamebuff, "w");
	if(fp == NULL) {
		perror("creating test file");
		return(1);
	}
	(void)fputs("foo bar bletch\n", fp);
	(void)fclose(fp);

	a.system = host_name;
	a.pn = printer;
	a.user = user_name;
	a.file = tag2;
	a.opts = "xxxx";
	a.copies = 1;
	a.cm = "-";

	printf("\ninvoking pr_start_2\n");

	rp = pcnfsd2_pr_start_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: stat = %d, jobid = '%s', cm = '%s'\n",
		rp->stat, rp->id, rp->cm);
	if(rp->stat == PS_RES_OK)
		strcpy(last_id, rp->id);
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->id)
		free(rp->id);
	return(0);
}


test_v2_cancel(host_name, printer, user_name, id)
char *host_name;
char *printer;
char *user_name;
char *id;
{
v2_pr_cancel_args a;
v2_pr_cancel_results *rp;
	printf("\ntesting cancel print v2\n");

	a.system = host_name;
	a.pn = printer;
	a.user = user_name;
	a.id = id;
	a.cm = "-";

	printf("\ninvoking pr_cancel_2 for job %s on printer %s\n",
		id, printer);

	rp = pcnfsd2_pr_cancel_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: stat = %d, cm = '%s'\n",
		rp->stat, rp->cm);
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	return(0);
}
test_v2_list()
{
char a;
v2_pr_list_results *rp;
pr_list curr;


	printf("\ninvoking pr_list_2\n");

	rp = pcnfsd2_pr_list_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: cm = '%s', printerlist:\n", rp->cm);
	curr = rp->printers;
	while(curr) {
		printf("  name '%s' ", curr->pn);
		if(strlen(curr->remhost))
			printf("remote: srvr '%s', name '%s'",
				curr->remhost,
				curr->device);
		else
			printf("local device = '%s'", curr->device);
		printf(", cm = '%s'\n", curr->cm);
		curr = curr->pr_next;
	}
	printf("end of list\n");
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->printers) {
		printf("freeing results\n");
		free_pr_list_item(rp->printers);
	}
	return(0);
}


void
free_pr_list_item(curr)
pr_list curr;
{
	if(curr->pn)
		free(curr->pn);
	if(curr->remhost)
		free(curr->remhost);
	if(curr->device)
		free(curr->device);
	if(curr->cm)
		free(curr->cm);
	if(curr->pr_next)
		free_pr_list_item(curr->pr_next); /* recurse */
	free(curr);
}



test_v2_queue(printer, user_name, private)
char *printer;
char *user_name;
int private;
{
struct v2_pr_queue_args a;
v2_pr_queue_results *rp;
pr_queue curr;

	a.pn = printer;
	a.system = "foo";
	a.user = user_name;
	a.just_mine = private;
	a.cm = "no";

	printf("\ninvoking pr_queue_2 (just_mine = %d)\n", private);

	rp = pcnfsd2_pr_queue_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: stat = %d, qlen = %d, qshown = %d cm = '%s', queue:\n",
		rp->stat, rp->qlen, rp->qshown, rp->cm);
	curr = rp->jobs;
	while(curr) {
		printf("rank = %2d, id = '%s', size = '%s', status = '%s'\n",
			curr->position,
			curr->id,
			curr->size,
			curr->status);
		printf("            user = '%s', file = '%s', cm = '%s'\n",
			curr->user,
			curr->file,
			curr->cm);
		curr = curr->pr_next;
	}
	printf("end of list\n");
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->jobs) {
		printf("freeing results\n");
		free_pr_queue_item(rp->jobs);
	}
	return(0);
}



void
free_pr_queue_item(curr)
pr_queue curr;
{
	if(curr->id)
		free(curr->id);
	if(curr->size)
		free(curr->size);
	if(curr->status)
		free(curr->status);
	if(curr->system)
		free(curr->system);
	if(curr->user)
		free(curr->user);
	if(curr->file)
		free(curr->file);
	if(curr->cm)
		free(curr->cm);
	if(curr->pr_next)
		free_pr_queue_item(curr->pr_next); /* recurse */
	free(curr);
}



test_v2_stat(printer)
char *printer;
{
v2_pr_status_args a;
v2_pr_status_results *rp;

	printf("\ntesting status print v2\n");

	a.pn = printer;
	a.cm = "-";

	printf("\ninvoking pr_status_2\n");

	rp = pcnfsd2_pr_status_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: stat = %d, cm = '%s'\n",
		rp->stat, rp->cm);
	if(rp->stat == PI_RES_OK) {
		printf("avail = %s, ", (rp->avail ? "YES" : "NO"));
		printf("printing = %s, ", (rp->printing ? "YES" : "NO"));
		printf("needs_operator = %s, ", (rp->needs_operator ? "YES" : "NO"));
		printf("qlen = %d, status = '%s'\n", rp->qlen, rp->status);
	}
/* free up allocated strings */
	if(rp->cm)
		free(rp->cm);
	if(rp->status)
		free(rp->status);
	return(0);
}

struct mapreq_arg_item * make_mapreq_entry(t, i, n, next)
mapreq	t;
int i;
char *n;
struct mapreq_arg_item *next;
{
struct mapreq_arg_item *x;
	x = (struct mapreq_arg_item *)malloc(sizeof(struct mapreq_arg_item));
	if(x == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(123);
	}
	x->req = t;
	x->id = i;
	x->name = (n ? n : "");
	x->mapreq_next = next;
	return(x);
}

test_v2_map()
{
v2_mapid_args a;
v2_mapid_results *rp;
struct mapreq_res_item *rip;

	a.cm = "-";
	a.req_list = make_mapreq_entry(MAP_REQ_UID, 906, NULL,
		make_mapreq_entry(MAP_REQ_GID, 1, NULL,
		 make_mapreq_entry(MAP_REQ_UNAME, 0, "root",
		   make_mapreq_entry(MAP_REQ_GNAME, 0, "wheel", 
		      make_mapreq_entry(MAP_REQ_UNAME, 0, "bogus", NULL)))));

	printf("\ninvoking pr_mapid_2\n");
	rp = pcnfsd2_mapid_2(&a, cl2);

	if(rp == NULL) {
		clnt_perror(cl2, server);
		return(1);
	}
	printf("results: cm = '%s', result list %s\n",
		rp->cm, rp->res_list ? "follows" : "omitted");
	rip = rp->res_list;
	while(rip) {
		printf("request type = %d, status = %d, id = %d, name = '%s'\n",
			rip->req, rip->stat, rip->id, 
			(rip->name ? rip->name : "(NULL)"));
		rip = rip->mapreq_next;
	}
/* XXX should free up results */



return(0);
}


void
good()
{
printf("\n");
printf("********************************************************\n");
printf("********************************************************\n");
printf("**      The following test is expected to SUCCEED     **\n");
printf("********************************************************\n");
printf("********************************************************\n");
}

void
bad(reason)
char *reason;
{
printf("\n");
printf("********************************************************\n");
printf("********************************************************\n");
printf("**      The following test is expected to FAIL        **\n");
printf("**                    Reason:                         **\n");
printf("** %50s **\n", reason);
printf("********************************************************\n");
printf("********************************************************\n");
}
