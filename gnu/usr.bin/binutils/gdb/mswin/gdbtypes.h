#ifndef GDBTYPES_H
#define GDBTYPES_H

class CTargetAddr 
{
 public:
  char * addr;
 char * togdb() { return addr; };
  CTargetAddr ( char *x) { addr = ( char *)x; };
  CTargetAddr (int x) { addr = ( char *)x; };
  CTargetAddr () {}

  int operator - (CTargetAddr a)
    {
      return addr - a.addr;
    }

  CTargetAddr operator += (int a)
    {
      return CTargetAddr(addr += a);
    }

  CTargetAddr operator + (int a)
    {
      return CTargetAddr(addr + a);
    }
  int operator == (CTargetAddr a)
    {
      return addr == a.addr;
    }
  int operator != (CTargetAddr a)
    {
      return addr != a.addr;
    }
  int operator >= (CTargetAddr a)
    {
      return addr >= a.addr;
    }
  int operator <= (CTargetAddr a)
    {
      return addr <= a.addr;
    }
  int operator > (CTargetAddr a)
    {
      return addr > a.addr;
    }
  int operator < (CTargetAddr a)
    {
      return addr < a.addr;
    }
};
#endif
