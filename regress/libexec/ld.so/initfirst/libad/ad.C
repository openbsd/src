/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ad.C,v 1.1.1.1 2011/11/29 03:38:26 kurt Exp $
 */

#include <iostream>

class AD
{
	public:
		AD();
		~AD();
};

AD::AD()
{
   std::cout << "D";
}

AD::~AD()
{
   std::cout << "d";
}

AD d;
