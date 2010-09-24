#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;
use Config;

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

plan(8);

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

{
    local $ENV{PERLDB_OPTS} = "ReadLine=0 NonStop=1";
    my $output = runperl(switches => [ '-d' ], progfile => '../lib/perl5db/t/symbol-table-bug');
    like($output, qr/Undefined symbols 0/, 'there are no undefined values in the symbol table');
}

SKIP: {
    if ( $Config{usethreads} ) {
        skip('This perl has threads, skipping non-threaded debugger tests');
    } else {
        my $error = 'This Perl not built to support threads';
        my $output = runperl( switches => [ '-dt' ], stderr => 1 );
        like($output, qr/$error/, 'Perl debugger correctly complains that it was not built with threads');
    }

}
SKIP: {
    if ( $Config{usethreads} ) {
        local $ENV{PERLDB_OPTS} = "ReadLine=0 NonStop=1";
        my $output = runperl(switches => [ '-dt' ], progfile => '../lib/perl5db/t/symbol-table-bug');
        like($output, qr/Undefined symbols 0/, 'there are no undefined values in the symbol table when running with thread support');
    } else {
        skip("This perl is not threaded, skipping threaded debugger tests");
    }
}


# Test [perl #61222]
{
    rc(
        qq|
        &parse_options("NonStop=0 TTY=db.out LineInfo=db.out");
        \n|,

        qq|
        sub afterinit {
            push(\@DB::typeahead,
                'm Pie',
                'q',
            );
        }\n|,
    );

    my $output = runperl(switches => [ '-d' ], stderr => 1, progfile => '../lib/perl5db/t/rt-61222');
    my $contents;
    {
        local $/;
        open I, "<", 'db.out' or die $!;
        $contents = <I>;
        close(I);
    }
    unlike($contents, qr/INCORRECT/, "[perl #61222]");
}



# Test for Proxy constants
{
    rc(
        qq|
        &parse_options("NonStop=0 ReadLine=0 TTY=db.out LineInfo=db.out");
        \n|,

        qq|
        sub afterinit {
            push(\@DB::typeahead,
                'm main->s1',
                'q',
            );
        }\n|,
    );

    my $output = runperl(switches => [ '-d' ], stderr => 1, progfile => '../lib/perl5db/t/proxy-constants');
    is($output, "", "proxy constant subroutines");
}


# [perl #66110] Call a subroutine inside a regex
{
    local $ENV{PERLDB_OPTS} = "ReadLine=0 NonStop=1";
    my $output = runperl(switches => [ '-d' ], stderr => 1, progfile => '../lib/perl5db/t/rt-66110');
    like($output, "All tests successful.", "[perl #66110]");
}


# clean up.

END {
    1 while unlink qw(.perldb db.out);
}
