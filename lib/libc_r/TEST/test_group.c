/*	$OpenBSD: test_group.c,v 1.1 2000/01/08 08:05:43 d Exp $	*/

/*
 * Test getgrgid_r() across multiple threads to see if the members list changes.
 */

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <grp.h>
#include <sys/types.h>
#include "test.h"

struct group * getgrgid_r(gid_t, struct group *);

char fail[] = "fail";

int done;
pthread_mutex_t display;
pthread_cond_t done1;
pthread_cond_t test2;

void*
test(void* arg)
{
	gid_t gid = (int)arg;
	gid_t ogid;
	struct group grpbuf;
	struct group *grp;
	char **p;
	char buf[2048];
	char *cpy[128];
	int i;
	char *s;
	char *oname;
	char *opasswd;

	CHECKr(pthread_mutex_lock(&display));

	printf("gid %d\n", gid);

#if 1
	grpbuf.gr_name = fail;
	CHECKn(grp = getgrgid_r(gid, &grpbuf));
	ASSERT(grp->gr_name != fail);
#else
	CHECKn(grp = getgrgid(gid));
#endif

	s = buf;

	/* copy gr_name */
	strcpy(oname = s, grp->gr_name);
	s += 1 + strlen(s);

	/* copy gr_passwd */
	strcpy(opasswd = s, grp->gr_passwd);
	s += 1 + strlen(s);

	/* copy gr_gid */
	ogid = grp->gr_gid;

	/* copy gr_mem */
	for (i = 0, p = grp->gr_mem; *p; p++) {
		strcpy(cpy[i] = s, *p); i++;
		s += 1 + strlen(s);
	}
	cpy[i] = NULL;

	printf("now:    %s:%s:%d:", grp->gr_name, grp->gr_passwd, grp->gr_gid);
	for (p = grp->gr_mem; *p; p++) 
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	printf("\n");

#ifdef DEBUG /* debugging this program */
	printf("buf = \"");
	for (i = 0; i < s - buf; i++)
		if (buf[i] == '\0')	printf("\\0");
		else printf("%c", buf[i]);
	printf("\"\n");
#endif

	CHECKr(pthread_cond_signal(&done1));	/* wake up main */

	CHECKr(pthread_cond_wait(&test2, &display));

	printf("before: %s:%s:%d:", oname, opasswd, ogid);
	for (p = cpy; *p; p++) 
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	printf("\n");

	printf("after:  %s:%s:%d:", grp->gr_name, grp->gr_passwd, grp->gr_gid);
	for (p = grp->gr_mem; *p; p++) 
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	printf("\n");

	CHECKr(pthread_mutex_unlock(&display));
	CHECKr(pthread_cond_signal(&test2));	/* wake another */

	return NULL;
}


#define NGRPS	5
int
main()
{
	pthread_t thread[NGRPS];
	int gid;
	int failed;
	void *result;

	CHECKr(pthread_mutex_init(&display, NULL));

	CHECKr(pthread_cond_init(&done1, NULL));
	CHECKr(pthread_cond_init(&test2, NULL));

	/* Get separate threads to do a group open separately */
	for (gid = 0; gid < NGRPS; gid++) {
		CHECKr(pthread_create(&thread[gid], NULL, test, (void *)gid));
	}

	sleep(1); /* XXX */

	/* now get each thread to print it out again */
	CHECKr(pthread_cond_signal(&test2));

	failed = 0;
	for (gid = 0; gid < NGRPS; gid++) {
		CHECKr(pthread_join(thread[gid], &result));
		if (result != NULL)
			failed++;
	}

	/* (I'm too lazy to write the check code at the moment) */
	fprintf(stderr, "[needs visual check]\n");

	if (!failed) {
		SUCCEED;
	} else {
		fprintf(stderr, "one of the test failed\n");
		exit(1);
	}
}
