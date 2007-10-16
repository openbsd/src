main()
{
         long long foo = 10;
         unsigned int d = 0xffffffffUL;

         if (foo + d != 0x100000009LL)
		return 1;

         foo += d;
         if (foo != 0x100000009LL)
		return 1;
	 return 0;
}

