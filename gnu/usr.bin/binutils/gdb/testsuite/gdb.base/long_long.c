/* Test long long expression; test printing in general.
 *
 * /CLO/BUILD_ENV/Exports/cc -g +e -o long_long long_long.c
 *
 * or
 *
 * cc +e +DA2.0 -g -o long_long long_long.c
 */

#ifdef PROTOTYPES
long long callee(long long i)
#else
long long callee( i )
long long i;
#endif
{
   register long long result;

   result  = 0x12345678;
   result  = result << i;
   result += 0x9abcdef0;

   return result;
}

int known_types()
{
   long long bin = 0, oct = 0, dec = 0, hex = 0;

   /* Known values, filling the full 64 bits.
    */
   bin = 0x123456789abcdefLL; /* 64 bits = 16 hex digits */
   oct = 01234567123456701234567LL; /*  = 21+ octal digits */
   dec = 12345678901234567890ULL;    /*  = 19+ decimal digits */

   /* Stop here and look!
    */
   hex = bin - dec | oct;

   return 0;
}

int main() {

   register long long x, y;
   register long long i;
   
   x = (long long) 0xfedcba9876543210LL;
   y = x++;
   x +=y;
   i = 11;
   x = callee( i );
   y += x;

   known_types();
   
   return 0;
}



