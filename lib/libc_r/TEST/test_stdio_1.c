#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

char * base_name = "test_stdio_1.c";
char * dir_name = SRCDIR;
char * fullname;

/* Test fopen()/ftell()/getc() */
void
test_1()
{
	struct stat statbuf;
	FILE * fp;
	int i;

	CHECKe(stat(fullname, &statbuf));

	CHECKn((fp = fopen(fullname, "r")));

	/* Get the entire file */
	while ((i = getc(fp)) != EOF)
		;

	ASSERT(ftell(fp) == statbuf.st_size);

	CHECKe(fclose(fp));
}

/* Test fopen()/fclose() */
void
test_2()
{
	FILE *fp1, *fp2;

	CHECKn(fp1 = fopen(fullname, "r"));
	CHECKe(fclose(fp1));

	CHECKn(fp2 = fopen(fullname, "r"));
	CHECKe(fclose(fp2));

	ASSERT(fp1 == fp2);
}

/* Test sscanf()/sprintf() */
void
test_3(void)
{
	char * str = "10 4.53";
	char buf[64];
	double d;
	int    i;

	ASSERT(sscanf(str, "%d %lf", &i, &d) == 2);

	/* Should have a check */
	sprintf(buf, "%d %2.2f", i, d);
	ASSERT(strcmp(buf, str) == 0);
}

int
main()
{

	CHECKn(fullname = malloc (strlen (dir_name) + strlen (base_name) + 2));
	sprintf (fullname, "%s/%s", dir_name, base_name);

	test_1();
	test_2();
	test_3();

	SUCCEED;
}
