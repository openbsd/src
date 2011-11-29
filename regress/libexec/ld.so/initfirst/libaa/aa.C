/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: aa.C,v 1.1.1.1 2011/11/29 03:38:26 kurt Exp $
 */

#include <iostream>

class AA
{
	public:
		AA();
		~AA();
};

AA::AA()
{
   std::cout << "A";
}

AA::~AA()
{
   std::cout << "a";
}

AA a;
