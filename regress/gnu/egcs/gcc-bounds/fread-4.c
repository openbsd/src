#include <stdio.h>

int main(int argc, char **argv) {
	char buf[100];
	FILE *fp;
	fread(buf, sizeof(long), sizeof(buf), fp);
	return 1;
}
