#!/usr/bin/perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;

BEGIN {
    if (!-c "/dev/null") {
	print "1..0 # Skip: no /dev/null\n";
	exit 0;
    }
    if (!-c "/dev/tty") {
	print "1..0 # Skip: no /dev/tty\n";
	exit 0;
    }
}

plan(1);

sub rc {
    open RC, ">", ".perldb" or die $!;
    print RC @_;
    close(RC);
    # overly permissive perms gives "Must not source insecure rcfile"
    # and hangs at the DB(1> prompt
    chmod 0644, ".perldb";
}

my $target = '../lib/perl5db/t/eval-line-bug';

rc(
    qq|
    &parse_options("NonStop=0 TTY=db.out LineInfo=db.out");
    \n|,

    qq|
    sub afterinit {
	push(\@DB::typeahead,
	    'b 23',
	    'n',
	    'n',
	    'n',
	    'c', # line 23
	    'n',
	    "p \\\@{'main::_<$target'}",
	    'q',
	);
    }\n|,
);

{
    local $ENV{PERLDB_OPTS} = "ReadLine=0";
    runperl(switches => [ '-d' ], progfile => $target);
}

my $contents;
{
    local $/;
    open I, "<", 'db.out' or die $!;
    $contents = <I>;
    close(I);
}

like($contents, qr/sub factorial/,
    'The ${main::_<filename} variable in the debugger was not destroyed'
);

# clean up.

END {
    unlink qw(.perldb db.out);
}
