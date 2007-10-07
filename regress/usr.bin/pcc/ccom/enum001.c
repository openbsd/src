/* code should produce exactly one warning */

enum foo { bar };
enum footoo { bar1 };

enum foo f(void) { return bar; }
double g(void) { return 0.0; }

struct baz {
	enum foo (*ff)(void);
};

struct baz a[] = { {f},
	{g} };


static int h(enum foo *);
static int h(enum foo *arg)
{
	return 0;
}

int main()
{
	return 0;
}
