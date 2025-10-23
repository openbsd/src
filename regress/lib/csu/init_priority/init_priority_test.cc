/* $OpenBSD: init_priority_test.cc,v 1.5 2025/10/23 19:06:10 miod Exp $ */

#include <cassert>
#include <cstdio>

namespace {
const int kNumTests = 10;
int counter = 0;
int log[kNumTests];

struct Test {
	Test(int x);
};

Test::Test(int x)
{
	if (counter < kNumTests)
		log[counter] = x;
	fprintf(stderr, "%d\n", x);
	counter++;
}

#define TEST(n) Test test_##n __attribute__((init_priority (n))) (n)
TEST(12597);
TEST(20840);
TEST(31319);
TEST(17071);
TEST(47220);
TEST(40956);
TEST(28373);
TEST(8742);
TEST(14117);
TEST(6407);
#undef TEST
}

int
main()
{
	int i;

	assert(counter == kNumTests);
	for (i = 1; i < kNumTests; i++)
		assert(log[i] >= log[i - 1]);

	return (0);
}
