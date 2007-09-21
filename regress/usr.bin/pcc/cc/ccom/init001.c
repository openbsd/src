/* Based on OpenBSD PR 5586 from TAKAHASHI Tamotsu */

struct s {
	int f;
	int g[1][1];
};

struct s v = { 0x99, {{0x100}} };

int
main()
{
	if (v.f != 0x99)
		errx(1, "wrong");
	return 0;
}
