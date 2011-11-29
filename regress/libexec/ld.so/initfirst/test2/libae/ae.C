/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ae.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <iostream>

class AE
{
	public:
		AE();
		~AE();
};

AE::AE()
{
   std::cout << "E";
}

AE::~AE()
{
   std::cout << "e";
}

AE e;
