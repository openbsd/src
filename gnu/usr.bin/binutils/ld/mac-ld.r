#ifdef WANT_CFRG

#include "CodeFragmentTypes.r"

resource 'cfrg' (0) {
	{
		kPowerPC,
		kFullLib,
		kNoVersionNum, kNoVersionNum,
		0, 0,
		kIsApp, kOnDiskFlat, kZeroOffset, kWholeFork,
		"ld"
	}
};

#endif /* WANT_CFRG */
