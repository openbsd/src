/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ae.C,v 1.2 2017/08/04 18:26:54 kettenis Exp $
 */

#include <cstdio>

class AE
{
	public:
		AE();
		~AE();
};

AE::AE()
{
   std::printf("E");
}

AE::~AE()
{
   std::printf("e");
}

AE e;
