#include <unistd.h>

int main(int argc, char **argv) {
	char buf[10];
	getcwd(buf, sizeof buf);
	return 1;
}
