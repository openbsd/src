#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <md4.h>

const unsigned char data[] = "1234567890abcdefghijklmnopqrstuvwxyz";

int
main(int argc, char **argv)
{
	MD4_CTX ctx;
	char ret[33];

	MD4Init(&ctx);
	MD4Data(data, sizeof data - 1, ret);
	printf("%s\n", ret);
}
