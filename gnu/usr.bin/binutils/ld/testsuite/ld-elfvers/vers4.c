/*
 * Testcase to make sure that a versioned symbol definition in an
 * application correctly defines the version node, if and only if
 * the actual symbol is exported.  This is built both with and without
 * -export-dynamic.
 */
int bar()
{
	return 3;
}

new_foo()
{
	return 1000+bar();

}

__asm__(".symver new_foo,foo@@VERS_2.0");

main()
{
  printf("%d\n", foo());
}
