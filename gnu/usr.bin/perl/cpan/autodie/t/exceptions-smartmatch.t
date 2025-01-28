#!/usr/bin/perl -w
use strict;
use Test::More;
use Fatal ();

BEGIN { plan skip_all => "requires perl with smartmatch support" unless Fatal::SMARTMATCH_ALLOWED; }

# These are tests that depend upon smartmatch.
# Basic tests should go in basic_exceptions.t

use 5.010;
use warnings ();
use constant NO_SUCH_FILE => 'this_file_had_better_not_exist_xyzzy';
no if Fatal::SMARTMATCH_CATEGORY, 'warnings', Fatal::SMARTMATCH_CATEGORY;

plan 'no_plan';

eval {
	use autodie ':io';
	open(my $fh, '<', NO_SUCH_FILE);
};

ok($@,			"Exception thrown"		        );
ok('open' ~~ $@,	"Exception from open"		        );
ok(':file' ~~ $@,	"Exception from open / class :file"	);
ok(':io' ~~ $@,		"Exception from open / class :io"	);
ok(':all' ~~ $@,	"Exception from open / class :all"	);

eval {
	use autodie ':io';
	close(THIS_FILEHANDLE_AINT_OPEN);
};

ok('close' ~~ $@,	"Exception from close"		        );
ok(':file' ~~ $@,	"Exception from close / class :file"	);
ok(':io' ~~ $@,		"Exception from close / class :io"	);
ok(':all' ~~ $@,	"Exception from close / class :all"	);
