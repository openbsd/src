int readlink(char *path, char *buf, int bufsiz)
{
  /* OpenVMS dosen't have symbolic links in the UNIX sense */
  return -1;
}
