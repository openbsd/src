#include <stdlib.h>

@interface Foo
+ (void) bar;
@end

@implementation Foo
+ (void) bar {
  exit(0);
}
@end

int main (void)
{
  [Foo bar];

  return 1;
}
