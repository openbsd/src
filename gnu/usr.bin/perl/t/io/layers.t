#!./perl

my $PERLIO;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    unless (find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
    eval 'use Encode';
    if ($@ =~ /dynamic loading not available/) {
        print "1..0 # miniperl cannot load Encode\n";
	exit 0;
    }
    # Makes testing easier.
    $ENV{PERLIO} = 'stdio' if exists $ENV{PERLIO} && $ENV{PERLIO} eq '';
    if (exists $ENV{PERLIO} && $ENV{PERLIO} !~ /^(stdio|perlio|mmap)$/) {
	# We are not prepared for anything else.
	print "1..0 # PERLIO='$ENV{PERLIO}' unknown\n";
	exit 0;
    }
    $PERLIO = exists $ENV{PERLIO} ? $ENV{PERLIO} : "(undef)";
}

use Config;

my $DOSISH    = $^O =~ /^(?:MSWin32|os2|dos|NetWare|mint)$/ ? 1 : 0;
   $DOSISH    = 1 if !$DOSISH and $^O =~ /^uwin/;
my $NONSTDIO  = exists $ENV{PERLIO} && $ENV{PERLIO} ne 'stdio'     ? 1 : 0;
my $FASTSTDIO = $Config{d_faststdio} && $Config{usefaststdio}      ? 1 : 0;

my $NTEST = 43 - (($DOSISH || !$FASTSTDIO) ? 7 : 0) - ($DOSISH ? 5 : 0);

plan tests => $NTEST;

print <<__EOH__;
# PERLIO    = $PERLIO
# DOSISH    = $DOSISH
# NONSTDIO  = $NONSTDIO
# FASTSTDIO = $FASTSTDIO
__EOH__

SKIP: {
    skip("This perl does not have Encode", $NTEST)
	unless " $Config{extensions} " =~ / Encode /;

    sub check {
	my ($result, $expected, $id) = @_;
	# An interesting dance follows where we try to make the following
	# IO layer stack setups to compare equal:
	#
	# PERLIO     UNIX-like                   DOS-like
	#
	# unset / "" unix perlio / stdio [1]     unix crlf
	# stdio      unix perlio / stdio [1]     stdio
	# perlio     unix perlio                 unix perlio
	# mmap       unix mmap                   unix mmap
	#
	# [1] "stdio" if Configure found out how to do "fast stdio" (depends
	# on the stdio implementation) and in Perl 5.8, otherwise "unix perlio"
	#
	if ($NONSTDIO) {
	    # Get rid of "unix".
	    shift @$result if $result->[0] eq "unix";
	    # Change expectations.
	    if ($FASTSTDIO) {
		$expected->[0] = $ENV{PERLIO};
	    } else {
		$expected->[0] = $ENV{PERLIO} if $expected->[0] eq "stdio";
	    }
	} elsif (!$FASTSTDIO && !$DOSISH) {
	    splice(@$result, 0, 2, "stdio")
		if @$result >= 2 &&
		   $result->[0] eq "unix" &&
		   $result->[1] eq "perlio";
	} elsif ($DOSISH) {
	    splice(@$result, 0, 2, "stdio")
		if @$result >= 2 &&
		   $result->[0] eq "unix" &&
		   $result->[1] eq "crlf";
	}
	if ($DOSISH && grep { $_ eq 'crlf' } @$expected) {
	    # 5 tests potentially skipped because
	    # DOSISH systems already have a CRLF layer
	    # which will make new ones not stick.
	    @$expected = grep { $_ ne 'crlf' } @$expected;
	}
	my $n = scalar @$expected;
	is($n, scalar @$expected, "$id - layers == $n");
	for (my $i = 0; $i < $n; $i++) {
	    my $j = $expected->[$i];
	    if (ref $j eq 'CODE') {
		ok($j->($result->[$i]), "$id - $i is ok");
	    } else {
		is($result->[$i], $j,
		   sprintf("$id - $i is %s",
			   defined $j ? $j : "undef"));
	    }
	}
    }

    check([ PerlIO::get_layers(STDIN) ],
	  [ "stdio" ],
	  "STDIN");

    open(F, ">:crlf", "afile");

    check([ PerlIO::get_layers(F) ],
	  [ qw(stdio crlf) ],
	  "open :crlf");

    binmode(F, ":encoding(sjis)"); # "sjis" will be canonized to "shiftjis"

    check([ PerlIO::get_layers(F) ],
	  [ qw[stdio crlf encoding(shiftjis) utf8] ],
	  ":encoding(sjis)");
    
    binmode(F, ":pop");

    check([ PerlIO::get_layers(F) ],
	  [ qw(stdio crlf) ],
	  ":pop");

    binmode(F, ":raw");

    check([ PerlIO::get_layers(F) ],
	  [ "stdio" ],
	  ":raw");

    binmode(F, ":utf8");

    check([ PerlIO::get_layers(F) ],
	  [ qw(stdio utf8) ],
	  ":utf8");

    binmode(F, ":bytes");

    check([ PerlIO::get_layers(F) ],
	  [ "stdio" ],
	  ":bytes");

    binmode(F, ":encoding(utf8)");

    check([ PerlIO::get_layers(F) ],
	    [ qw[stdio encoding(utf8) utf8] ],
	    ":encoding(utf8)");

    binmode(F, ":raw :crlf");

    check([ PerlIO::get_layers(F) ],
	  [ qw(stdio crlf) ],
	  ":raw:crlf");

    binmode(F, ":raw :encoding(latin1)"); # "latin1" will be canonized

    # 7 tests potentially skipped.
    unless ($DOSISH || !$FASTSTDIO) {
	my @results = PerlIO::get_layers(F, details => 1);

	# Get rid of the args and the flags.
	splice(@results, 1, 2) if $NONSTDIO;

	check([ @results ],
	      [ "stdio",    undef,        sub { $_[0] > 0 },
		"encoding", "iso-8859-1", sub { $_[0] & PerlIO::F_UTF8() } ],
	      ":raw:encoding(latin1)");
    }

    binmode(F);

    check([ PerlIO::get_layers(F) ],
	  [ "stdio" ],
	  "binmode");

    close F;

    {
	use open(IN => ":crlf", OUT => ":encoding(cp1252)");

	open F, "<afile";
	open G, ">afile";

	check([ PerlIO::get_layers(F, input  => 1) ],
	      [ qw(stdio crlf) ],
	      "use open IN");
	
	check([ PerlIO::get_layers(G, output => 1) ],
	      [ qw[stdio encoding(cp1252) utf8] ],
	      "use open OUT");

	close F;
	close G;
    }

    1 while unlink "afile";
}
