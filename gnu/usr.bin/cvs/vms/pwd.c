#include "pwd.h"
#include <stdio.h>
#include <unixlib.h>

static struct passwd pw;

struct passwd *getpwuid(uid_t uid)
{
  pw.pw_name = getlogin();
  pw.pw_uid = getuid();
  pw.pw_gid = getgid();

  return &pw;
}

struct passwd *getpwnam(char *name)
{
  pw.pw_name = getlogin();
  pw.pw_uid = getuid();
  pw.pw_gid = getgid();

  return &pw;
}

char *getlogin()
{
  static char login[256];
  return cuserid(login);
} 
