/* Resources for GNU AS. */

#include "SysTypes.r"

/* Version resources. */

resource 'vers' (1)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	VERSION_STRING  " (c) 1986-95 FSF, Inc. "
};

resource 'vers' (2, purgeable)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	"GAS " /* fill in major/minor versions */ "for MPW"
};

#ifdef WANT_CFRG

#include "CodeFragmentTypes.r"

resource 'cfrg' (0) {
	{
		kPowerPC,
		kFullLib,
		kNoVersionNum, kNoVersionNum,
		0,0,
		kIsApp, kOnDiskFlat, kZeroOffset, kWholeFork,
		"as"
	}
};

#endif /* WANT_CFRG */
