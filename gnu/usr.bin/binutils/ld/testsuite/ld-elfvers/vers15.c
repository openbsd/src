/*
 * Testcase to make sure that if we externally reference a versioned symbol
 * that we always get the right one.
 */


foo_1()
{
  return 1034;
}

foo_2()
{
  return 1343;
}

foo_3()
{
  return 1334;
}

main()
{
  printf("Expect 4,    get %d\n", foo_1());
  printf("Expect 13,   get %d\n", foo_2());
  printf("Expect 103,  get %d\n", foo_3());
}

__asm__(".symver foo_1,show_foo@");
__asm__(".symver foo_2,show_foo@VERS_1.1");
__asm__(".symver foo_3,show_foo@@VERS_1.2");
