void x()
{
  void (*fp)() = 0;
  fp();
}

int
main()
{
  x();
  return 0;
}
