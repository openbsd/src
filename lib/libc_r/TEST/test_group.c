/*	$OpenBSD: test_group.c,v 1.2 2000/01/08 09:01:29 d Exp $	*/

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

pthread_cond_t done;
volatile int done_count;

pthread_mutex_t display;
pthread_mutex_t display2;

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
	int count1, count2;
	char *s;
	char *oname;
	char *opasswd;

	/* Acquire lock for running first part. */
	CHECKr(pthread_mutex_lock(&display));

	/* Store magic name to test for non-alteration */
	grpbuf.gr_name = fail;

	/* Call getgrgid_r() */
	printf("gid %d\n", gid);
	CHECKn(grp = getgrgid_r(gid, &grpbuf));

	/* Test for non-alteration of group structure */
	ASSERT(grp->gr_name != fail);

	/* We must get the right group */
	ASSERT(grp->gr_gid == gid);

	s = buf;	/* Keep our private buffer on the stack */

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

#if 0
	printf("now:    %s:%s:%d:", grp->gr_name, grp->gr_passwd, grp->gr_gid);
	for (p = grp->gr_mem; *p; p++) 
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	printf("\n");
#endif

#ifdef DEBUG /* debugging this program */
	printf("buf = \"");
	for (i = 0; i < s - buf; i++)
		if (buf[i] == '\0')	printf("\\0");
		else printf("%c", buf[i]);
	printf("\"\n");
#endif

	/* Inform main that we have finished */
	done_count++;
	CHECKr(pthread_cond_signal(&done));

	/* Allow other threads to run first part */
	CHECKr(pthread_mutex_unlock(&display));

	/* Acquire lock for the second part */
	CHECKr(pthread_mutex_lock(&display2));

	count1 = 0;
	printf("before: %s:%s:%d:", oname, opasswd, ogid);
	for (p = cpy; *p; p++)  {
		count1++;
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	}
	printf("\n");

	count2 = 0;
	printf("after:  %s:%s:%d:", grp->gr_name, grp->gr_passwd, grp->gr_gid);
	for (p = grp->gr_mem; *p; p++)  {
		count2++;
		printf("%s%s", *p, *(p+1) == NULL ? "": ",");
	}
	printf("\n");

	CHECKr(pthread_mutex_unlock(&display2));

	if (count1 != count2)
		return "gr_mem length changed";
	for (i = 0; i < count1; i++)
		if (strcmp(cpy[i], grp->gr_mem[i]) != 0)
			return "gr_mem list changed";
	if (strcmp(grp->gr_name, oname) != 0)
		return "gr_name changed";
	if (strcmp(grp->gr_passwd, opasswd) != 0)
		return "gr_passwd changed";
	if (grp->gr_gid != ogid)
		return "gr_gid changed";
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
	CHECKr(pthread_mutex_init(&display2, NULL));

	CHECKr(pthread_cond_init(&done, NULL));
	done_count = 0;

	pthread_mutex_lock(&display);
	pthread_mutex_lock(&display2);

	/* Get separate threads to do a group open separately */
	for (gid = 0; gid < NGRPS; gid++) {
		CHECKr(pthread_create(&thread[gid], NULL, test, (void *)gid));
	}

	/* Allow all threads to run their first part */
	while (done_count < NGRPS) 
		pthread_cond_wait(&done, &display);

	/* Allow each thread to run the 2nd part of its test */
	CHECKr(pthread_mutex_unlock(&display2));

	/* Wait for each thread to terminate, collecting results. */
	failed = 0;
	for (gid = 0; gid < NGRPS; gid++) {
		CHECKr(pthread_join(thread[gid], &result));
		if (result != NULL) {
			fprintf(stderr, "gid %d: %s\n", gid, (char *)result);
			failed++;
		}
	}

	if (!failed) {
		SUCCEED;
	} else {
		exit(1);
	}
}
