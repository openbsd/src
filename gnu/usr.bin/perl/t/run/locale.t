#!./perl
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';    # for fresh_perl_is() etc
    require './loc_tools.pl'; # to find locales
}

use strict;

########
# These tests are here instead of lib/locale.t because
# some bugs depend on the internal state of the locale
# settings and pragma/locale messes up that state pretty badly.
# We need "fresh runs".
BEGIN {
    eval { require POSIX; POSIX->import("locale_h") };
    if ($@) {
	skip_all("could not load the POSIX module"); # running minitest?
    }
}
use Config;
my $have_strtod = $Config{d_strtod} eq 'define';
my @locales = eval { find_locales( [ &LC_ALL, &LC_CTYPE, &LC_NUMERIC ],
                                  1 # Only return locales that work well with
                                    # Perl
                                 ) };
skip_all("no locales available") unless @locales;

# reset the locale environment
local @ENV{'LANG', (grep /^LC_/, keys %ENV)};

plan tests => &last;

my $non_C_locale;
foreach my $locale (@locales) {
    next if $locale eq "C" || $locale eq 'POSIX';
    $non_C_locale = $locale;
    last;
}

SKIP: {
    skip("no non-C locale available", 2 ) unless $non_C_locale;
    setlocale(LC_NUMERIC, $non_C_locale);
    isnt(setlocale(LC_NUMERIC), "C", "retrieving current non-C LC_NUMERIC doesn't give 'C'");
    setlocale(LC_ALL, $non_C_locale);
    isnt(setlocale(LC_ALL), "C", "retrieving current non-C LC_ALL doesn't give 'C'");
}

# Skip this locale on these cywgwin versions as the returned radix character
# length is wrong
my @test_numeric_locales = ($^O ne 'cygwin' || version->new(($Config{'osvers'} =~ /^(\d+(?:\.\d+)+)/)[0]) gt v2.4.1)
                           ? @locales
                           : grep { $_ !~ m/ps_AF/i } @locales;

fresh_perl_is("for (qw(@test_numeric_locales)) {\n" . <<'EOF',
    use POSIX qw(locale_h);
    use locale;
    setlocale(LC_NUMERIC, "$_") or next;
    my $s = sprintf "%g %g", 3.1, 3.1;
    next if $s eq '3.1 3.1' || $s =~ /^(3.+1) \1$/;
    print "$_ $s\n";
}
EOF
    "", {}, "no locales where LC_NUMERIC breaks");

SKIP: {
    skip("Windows stores locale defaults in the registry", 1 )
                                                            if $^O eq 'MSWin32';
    fresh_perl_is("for (qw(@locales)) {\n" . <<'EOF',
        use POSIX qw(locale_h);
        use locale;
        my $in = 4.2;
        my $s = sprintf "%g", $in; # avoid any constant folding bugs
        next if $s eq "4.2";
        print "$_ $s\n";
    }
EOF
    "", {}, "LC_NUMERIC without environment nor setlocale() has no effect in any locale");
}

# try to find out a locale where LC_NUMERIC makes a difference
my $original_locale = setlocale(LC_NUMERIC);

my ($base, $different, $comma, $difference, $utf8_radix);
my $radix_encoded_as_utf8;
for ("C", @locales) { # prefer C for the base if available
    use locale;
    setlocale(LC_NUMERIC, $_) or next;
    my $in = 4.2; # avoid any constant folding bugs
    if ((my $s = sprintf("%g", $in)) eq "4.2")  {
	$base ||= $_;
    } else {
	$different ||= $_;
	$difference ||= $s;
        my $radix = localeconv()->{decimal_point};

        # For utf8 locales with a non-ascii radix, it should be encoded as
        # UTF-8 with the internal flag so set.
        if (! defined $utf8_radix
            && $radix =~ /[[:^ascii:]]/
            && is_locale_utf8($_))
        {
            $utf8_radix = $_;
            $radix_encoded_as_utf8 = utf8::is_utf8($radix);
        }
        else {
            $comma ||= $_ if $radix eq ',';
        }
    }

    last if $base && $different && $comma && $utf8_radix;
}
setlocale(LC_NUMERIC, $original_locale);

SKIP: {
    skip("no UTF-8 locale available where LC_NUMERIC radix isn't ASCII", 1 )
        unless $utf8_radix;
    ok($radix_encoded_as_utf8 == 1, "UTF-8 locale '$utf8_radix' with non-ASCII"
                                    . " radix is marked UTF-8");
}

SKIP: {
    skip("no locale available where LC_NUMERIC makes a difference", &last - 7 )
	if !$different;     # -7 is 5 tests before this block; 2 after
    note("using the '$different' locale for LC_NUMERIC tests");
    {
	local $ENV{LC_NUMERIC} = $different;

	fresh_perl_is(<<'EOF', "4.2", {},
format STDOUT =
@.#
4.179
.
write;
EOF
	    "format() does not look at LC_NUMERIC without 'use locale'");

        {
	    fresh_perl_is(<<'EOF', "$difference\n", {},
use POSIX;
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
	    fresh_perl_is(<<'EOF', ",,", {},
print localeconv()->{decimal_point};
use POSIX;
use locale;
print localeconv()->{decimal_point};
EOF
	    "localeconv() looks at LC_NUMERIC with and without 'use locale'");
        }

        {
            my $categories = ":collate :characters :collate :ctype :monetary :time";
            fresh_perl_is(<<"EOF", "4.2", {},
use locale qw($categories);
format STDOUT =
@.#
4.179
.
write;
EOF
	    "format() does not look at LC_NUMERIC with 'use locale qw($categories)'");
        }

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

        for my $category (qw(collate characters collate ctype monetary time)) {
            for my $negation ("!", "not_") {
                fresh_perl_is(<<"EOF", $difference, {},
use locale ":$negation$category";
format STDOUT =
@.#
4.179
.
write;
EOF
                "format() looks at LC_NUMERIC with 'use locale \":"
                . "$negation$category\"'");
            }
        }

        {
	    fresh_perl_is(<<'EOF', $difference, {},
use locale ":numeric";
format STDOUT =
@.#
4.179
.
write;
EOF
	    "format() looks at LC_NUMERIC with 'use locale \":numeric\"'");
        }

        {
	    fresh_perl_is(<<'EOF', "4.2", {},
format STDOUT =
@.#
4.179
.
{ use locale; write; }
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
            use locale;
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

    {
	local $ENV{LC_NUMERIC} = $different;
	fresh_perl_is(<<'EOF', "$difference "x4, {},
            use locale;
	    use POSIX qw(locale_h);
	    my $in = 4.2;
	    printf("%g %g %s %s ", $in, 4.2, sprintf("%g", $in), sprintf("%g", 4.2));
EOF
	"sprintf() and printf() look at LC_NUMERIC regardless of constant folding");
    }

    {
	local $ENV{LC_NUMERIC} = $different;
	fresh_perl_is(<<'EOF', "$difference "x4, {},
            use locale;
	    use POSIX qw(locale_h);
	    my $in = 4.2;
	    printf("%g %g %s %s ", $in, 4.2, sprintf("%g", $in), sprintf("%g", 4.2));
EOF
	"Uses the above test to verify that on Windows the system default locale has lower priority than LC_NUMERIC");
    }


    # within this block, STDERR is closed. This is because fresh_perl_is()
    # forks a shell, and some shells (like bash) can complain noisily when
    # LC_ALL or similar is set to an invalid value

    {
        open my $saved_stderr, ">&STDERR" or die "Can't dup STDERR: $!";
        close STDERR;

        {
            local $ENV{LC_ALL} = "invalid";
            local $ENV{LC_NUMERIC} = "invalid";
            local $ENV{LANG} = $different;
            local $ENV{PERL_BADLANG} = 0;

            if (! fresh_perl_is(<<"EOF", "$difference", { },
                if (\$ENV{LC_ALL} ne "invalid") {
                    # Make the test pass if the sh didn't accept the ENV set
                    print "$difference\n";
                    exit 0;
                }
                use locale;
                use POSIX qw(locale_h);
                my \$in = 4.2;
                printf("%g", \$in);
EOF
            "LANG is used if LC_ALL, LC_NUMERIC are invalid"))
           {
              note "To see details change this .t, do not close STDERR";
           }
        }

        SKIP: {
            if ($^O eq 'MSWin32') {
                skip("Win32 uses system default locale in preference to \"C\"",
                        1);
            }
            else {
                local $ENV{LC_ALL} = "invalid";
                local $ENV{LC_NUMERIC} = "invalid";
                local $ENV{LANG} = "invalid";
                local $ENV{PERL_BADLANG} = 0;

                if (! fresh_perl_is(<<"EOF", 4.2, { },
                    if (\$ENV{LC_ALL} ne "invalid") {
                        print "$difference\n";
                        exit 0;
                    }
                    use locale;
                    use POSIX qw(locale_h);
                    my \$in = 4.2;
                    printf("%g", \$in);
EOF
                'C locale is used if LC_ALL, LC_NUMERIC, LANG are invalid'))
                {
                    note "To see details change this .t, do not close STDERR";
                }
            }
        }

    open STDERR, ">&", $saved_stderr or die "Can't dup \$saved_stderr: $!";
    }

    {
	local $ENV{LC_NUMERIC} = $different;
	fresh_perl_is(<<"EOF",
	    use POSIX qw(locale_h);

            BEGIN { setlocale(LC_NUMERIC, \"$different\"); };
            setlocale(LC_ALL, "C");
            use 5.008;
            print setlocale(LC_NUMERIC);
EOF
	 "C", { },
         "No compile error on v-strings when setting the locale to non-dot radix at compile time when default environment has non-dot radix");
    }

    unless ($comma) {
        skip("no locale available where LC_NUMERIC is a comma", 3);
    }
    else {

        fresh_perl_is(<<"EOF",
            my \$i = 1.5;
            {
                use locale;
                use POSIX;
                POSIX::setlocale(POSIX::LC_NUMERIC(),"$comma");
                print \$i, "\n";
            }
            print \$i, "\n";
EOF
            "1,5\n1.5", {}, "Radix print properly in locale scope, and without");

        fresh_perl_is(<<"EOF",
            my \$i = 1.5;   # Should be exactly representable as a base 2
                            # fraction, so can use 'eq' below
            use locale;
            use POSIX;
            POSIX::setlocale(POSIX::LC_NUMERIC(),"$comma");
            print \$i, "\n";
            \$i += 1;
            print \$i, "\n";
EOF
            "1,5\n2,5", {}, "Can do math when radix is a comma"); # [perl 115800]

        unless ($have_strtod) {
            skip("no strtod()", 1);
        }
        else {
            fresh_perl_is(<<"EOF",
                use POSIX;
                POSIX::setlocale(POSIX::LC_NUMERIC(),"$comma");
                my \$one_point_5 = POSIX::strtod("1,5");
                \$one_point_5 =~ s/0+\$//;  # Remove any trailing zeros
                print \$one_point_5, "\n";
EOF
            "1.5", {}, "POSIX::strtod() uses underlying locale");
        }
    }
} # SKIP

    {
        fresh_perl_is(<<"EOF",
                use locale;
                use POSIX;
                POSIX::setlocale(POSIX::LC_CTYPE(),"C");
                print "h" =~ /[g\\w]/i || 0;
                print "\\n";
EOF
            1, {}, "/il matching of [bracketed] doesn't skip POSIX class if fails individ char");
    }

    {
        fresh_perl_is(<<"EOF",
                use locale;
                use POSIX;
                POSIX::setlocale(POSIX::LC_CTYPE(),"C");
                print "0" =~ /[\\d[:punct:]]/l || 0;
                print "\\n";
EOF
            1, {}, "/l matching of [bracketed] doesn't skip non-first POSIX class");

    }

# IMPORTANT: When adding tests before the following line, be sure to update
# its skip count:
#       skip("no locale available where LC_NUMERIC makes a difference", ...)
sub last { 37 }
