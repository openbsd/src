#include <stdio.h>

int main(int argc, char **argv) {
	char *buf;
	char buf2[10];
	snprintf(buf2, -sizeof(buf) + 100, "%s", "foo");
	return 1;
}
