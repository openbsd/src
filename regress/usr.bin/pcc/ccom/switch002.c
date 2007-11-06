/* Should not compile. */
int
main(int argc, char **argv)
{
	int *p = 0;

	switch (p) {
	}

	return 0;
}
