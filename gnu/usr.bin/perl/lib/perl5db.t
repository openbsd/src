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
my $dev_tty = '/dev/tty';
   $dev_tty = 'TT:' if ($^O eq 'VMS');
    if (!-c $dev_tty) {
	print "1..0 # Skip: no $dev_tty\n";
	exit 0;
    }
    if ($ENV{PERL5DB}) {
	print "1..0 # Skip: \$ENV{PERL5DB} is already set to '$ENV{PERL5DB}'\n";
	exit 0;
    }
}

plan(2);

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

{
    local $ENV{PERLDB_OPTS} = "ReadLine=0";
    my $output = runperl(switches => [ '-d' ], progfile => '../lib/perl5db/t/lvalue-bug');
    like($output, qr/foo is defined/, 'lvalue subs work in the debugger');
}

# clean up.

END {
    1 while unlink qw(.perldb db.out);
}
