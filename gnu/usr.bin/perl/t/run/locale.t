#!./perl
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';    # for fresh_perl_is() etc
}

use strict;

########
# These tests are here instead of lib/locale.t because
# some bugs depend on in the internal state of the locale
# settings and pragma/locale messes up that state pretty badly.
# We need "fresh runs".
BEGIN {
    eval { require POSIX; POSIX->import("locale_h") };
    if ($@) {
	skip_all("could not load the POSIX module"); # running minitest?
    }
}
use Config;
my $have_setlocale = $Config{d_setlocale} eq 'define';
$have_setlocale = 0 if $@;
# Visual C's CRT goes silly on strings of the form "en_US.ISO8859-1"
# and mingw32 uses said silly CRT
$have_setlocale = 0 if (($^O eq 'MSWin32' || $^O eq 'NetWare') && $Config{cc} =~ /^(cl|gcc)/i);
skip_all("no setlocale available") unless $have_setlocale;
my @locales;
if (-x "/usr/bin/locale" && open(LOCALES, "/usr/bin/locale -a 2>/dev/null|")) {
    while(<LOCALES>) {
        chomp;
        push(@locales, $_);
    }
    close(LOCALES);
}
skip_all("no locales available") unless @locales;

plan tests => &last;
fresh_perl_is("for (qw(@locales)) {\n" . <<'EOF',
    use POSIX qw(locale_h);
    use locale;
    setlocale(LC_NUMERIC, "$_") or next;
    my $s = sprintf "%g %g", 3.1, 3.1;
    next if $s eq '3.1 3.1' || $s =~ /^(3.+1) \1$/;
    print "$_ $s\n";
}
EOF
    "", {}, "no locales where LC_NUMERIC breaks");

fresh_perl_is("for (qw(@locales)) {\n" . <<'EOF',
    use POSIX qw(locale_h);
    use locale;
    my $in = 4.2;
    my $s = sprintf "%g", $in; # avoid any constant folding bugs
    next if $s eq "4.2";
    print "$_ $s\n";
}
EOF
    "", {}, "LC_NUMERIC without setlocale() has no effect in any locale");


# try to find out a locale where LC_NUMERIC makes a difference
my $original_locale = setlocale(LC_NUMERIC);

my ($base, $different, $difference);
for ("C", @locales) { # prefer C for the base if available
    BEGIN {
        if($Config{d_setlocale}) {
            require locale; import locale;
        }
    }
    setlocale(LC_NUMERIC, $_) or next;
    my $in = 4.2; # avoid any constant folding bugs
    if ((my $s = sprintf("%g", $in)) eq "4.2")  {
	$base ||= $_;
    } else {
	$different ||= $_;
	$difference ||= $s;
    }

    last if $base && $different;
}
setlocale(LC_NUMERIC, $original_locale);

SKIP: {
    skip("no locale available where LC_NUMERIC makes a difference", &last - 2)
	if !$different;
    note("using the '$different' locale for LC_NUMERIC tests");
    for ($different) {
	local $ENV{LC_NUMERIC} = $_;
	local $ENV{LC_ALL}; # so it never overrides LC_NUMERIC

	fresh_perl_is(<<'EOF', "4.2", {},
format STDOUT =
@.#
4.179
.
write;
EOF
	    "format() does not look at LC_NUMERIC without 'use locale'");

        {
	    fresh_perl_is(<<'EOF', $difference, {},
use locale;
format STDOUT =
@.#
4.179
.
write;
EOF
	    "format() looks at LC_NUMERIC with 'use locale'");
        }

        {
	    fresh_perl_is(<<'EOF', "4.2", {},
format STDOUT =
@.#
4.179
.
{ require locale; import locale; write; }
EOF
	    "too late to look at the locale at write() time");
        }

        {
	    fresh_perl_is(<<'EOF', $difference, {},
use locale;
format STDOUT =
@.#
4.179
.
{ no locale; write; }
EOF
	    "too late to ignore the locale at write() time");
        }
    }

    {
        # do not let "use 5.000" affect the locale!
        # this test is to prevent regression of [rt.perl.org #105784]
        fresh_perl_is(<<"EOF",
            BEGIN {
                if($Config{d_setlocale}) {
                    require locale; import locale;
                }
            }
            use POSIX;
            my \$i = 0.123;
            POSIX::setlocale(POSIX::LC_NUMERIC(),"$different");
            \$a = sprintf("%.2f", \$i);
            require version;
            \$b = sprintf("%.2f", \$i);
            print ".\$a \$b" unless \$a eq \$b
EOF
            "", {}, "version does not clobber version");

        fresh_perl_is(<<"EOF",
            use locale;
            use POSIX;
            my \$i = 0.123;
            POSIX::setlocale(POSIX::LC_NUMERIC(),"$different");
            \$a = sprintf("%.2f", \$i);
            eval "use v5.0.0";
            \$b = sprintf("%.2f", \$i);
            print "\$a \$b" unless \$a eq \$b
EOF
            "", {}, "version does not clobber version (via eval)");
    }


    for ($different) {
	local $ENV{LC_NUMERIC} = $_;
	local $ENV{LC_ALL}; # so it never overrides LC_NUMERIC
	fresh_perl_is(<<'EOF', "$difference "x4, {},
        use locale;
	    use POSIX qw(locale_h);
	    setlocale(LC_NUMERIC, "");
	    my $in = 4.2;
	    printf("%g %g %s %s ", $in, 4.2, sprintf("%g", $in), sprintf("%g", 4.2));
EOF
	"sprintf() and printf() look at LC_NUMERIC regardless of constant folding");
    }
} # SKIP

sub last { 9 }
