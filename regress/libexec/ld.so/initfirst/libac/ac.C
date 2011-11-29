/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ac.C,v 1.1.1.1 2011/11/29 03:38:26 kurt Exp $
 */

#include <iostream>

class AC
{
	public:
		AC();
		~AC();
};

AC::AC()
{
   std::cout << "C";
}

AC::~AC()
{
   std::cout << "c";
}

AC c;
