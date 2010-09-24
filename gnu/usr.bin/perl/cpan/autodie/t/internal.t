#!/usr/bin/perl -w
use strict;

use constant NO_SUCH_FILE => "this_file_or_dir_had_better_not_exist_XYZZY";

use Test::More tests => 6;

# Lexical tests using the internal interface.

eval { Fatal->import(qw(:lexical :void)) };
like($@, qr{:void cannot be used with lexical}, ":void can't be used with :lexical");

eval { Fatal->import(qw(open close :lexical)) };
like($@, qr{:lexical must be used as first}, ":lexical must come first");

{
	use Fatal qw(:lexical chdir);

	eval { chdir(NO_SUCH_FILE); };
	like ($@, qr/^Can't chdir/, "Lexical fatal chdir");

	no Fatal qw(:lexical chdir);

	eval { chdir(NO_SUCH_FILE); };
	is ($@, "", "No lexical fatal chdir");

}

eval { chdir(NO_SUCH_FILE); };
is($@, "", "Lexical chdir becomes non-fatal out of scope.");

eval { Fatal->import('2+2'); };
like($@,qr{Bad subroutine name},"Can't use fatal with invalid sub names");
