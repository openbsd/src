/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ad.C,v 1.2 2017/08/04 18:26:54 kettenis Exp $
 */

#include <cstdio>

class AD
{
	public:
		AD();
		~AD();
};

AD::AD()
{
   std::printf("D");
}

AD::~AD()
{
   std::printf("d");
}

AD d;
