/* Tests regarding examination of prologues.  */

int
inner (z)
     int z;
{
  return 2 * z;
}

int
middle (x)
     int x;
{
  if (x == 0)
    return inner (5);
  else
    return inner (6);
}

int
top (y)
     int y;
{
  return middle (y + 1);
}

int
main (argc, argv)
{
  return top (-1) + top (1);
}
