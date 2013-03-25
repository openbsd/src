#!./perl -wT

# This tests plain 'use locale' and adorned 'use locale ":not_characters"'
# Because these pragmas are compile time, and I (khw) am trying to test
# without using 'eval' as much as possible, which might cloud the issue,  the
# crucial parts of the code are duplicated in a block for each pragma.

binmode STDOUT, ':utf8';
binmode STDERR, ':utf8';

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    unshift @INC, '.';
    require Config; import Config;
    if (!$Config{d_setlocale} || $Config{ccflags} =~ /\bD?NO_LOCALE\b/) {
	print "1..0\n";
	exit;
    }
    $| = 1;
}

use strict;
use feature 'fc';

my $debug = 0;

use Dumpvalue;

my $dumper = Dumpvalue->new(
                            tick => qq{"},
                            quoteHighBit => 0,
                            unctrl => "quote"
                           );
sub debug {
  return unless $debug;
  my($mess) = join "", @_;
  chop $mess;
  print $dumper->stringify($mess,1), "\n";
}

sub debugf {
    printf @_ if $debug;
}

my $have_setlocale = 0;
eval {
    require POSIX;
    import POSIX ':locale_h';
    $have_setlocale++;
};

# Visual C's CRT goes silly on strings of the form "en_US.ISO8859-1"
# and mingw32 uses said silly CRT
# This doesn't seem to be an issue any more, at least on Windows XP,
# so re-enable the tests for Windows XP onwards.
my $winxp = ($^O eq 'MSWin32' && defined &Win32::GetOSVersion &&
		join('.', (Win32::GetOSVersion())[1..2]) >= 5.1);
$have_setlocale = 0 if ((($^O eq 'MSWin32' && !$winxp) || $^O eq 'NetWare') &&
		$Config{cc} =~ /^(cl|gcc)/i);

# UWIN seems to loop after taint tests, just skip for now
$have_setlocale = 0 if ($^O =~ /^uwin/);

sub LC_ALL ();

$a = 'abc %';

my $test_num = 0;

sub ok {
    my ($result, $message) = @_;
    $message = "" unless defined $message;

    print 'not ' unless ($result);
    print "ok " . ++$test_num;
    print " $message";
    print "\n";
}

# First we'll do a lot of taint checking for locales.
# This is the easiest to test, actually, as any locale,
# even the default locale will taint under 'use locale'.

sub is_tainted { # hello, camel two.
    no warnings 'uninitialized' ;
    my $dummy;
    local $@;
    not eval { $dummy = join("", @_), kill 0; 1 }
}

sub check_taint ($) {
    ok is_tainted($_[0]), "verify that is tainted";
}

sub check_taint_not ($) {
    ok((not is_tainted($_[0])), "verify that isn't tainted");
}

use locale;	# engage locale and therefore locale taint.

check_taint_not   $a;

check_taint       uc($a);
check_taint       "\U$a";
check_taint       ucfirst($a);
check_taint       "\u$a";
check_taint       lc($a);
check_taint       fc($a);
check_taint       "\L$a";
check_taint       "\F$a";
check_taint       lcfirst($a);
check_taint       "\l$a";

check_taint_not  sprintf('%e', 123.456);
check_taint_not  sprintf('%f', 123.456);
check_taint_not  sprintf('%g', 123.456);
check_taint_not  sprintf('%d', 123.456);
check_taint_not  sprintf('%x', 123.456);

$_ = $a;	# untaint $_

$_ = uc($a);	# taint $_

check_taint      $_;

/(\w)/;	# taint $&, $`, $', $+, $1.
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

/(.)/;	# untaint $&, $`, $', $+, $1.
check_taint_not  $&;
check_taint_not  $`;
check_taint_not  $';
check_taint_not  $+;
check_taint_not  $1;
check_taint_not  $2;

/(\W)/;	# taint $&, $`, $', $+, $1.
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

/(\s)/;	# taint $&, $`, $', $+, $1.
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

/(\S)/;	# taint $&, $`, $', $+, $1.
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

check_taint_not  $_;

/(b)/;		# this must not taint
check_taint_not  $&;
check_taint_not  $`;
check_taint_not  $';
check_taint_not  $+;
check_taint_not  $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

check_taint_not  $_;

$b = uc($a);	# taint $b
s/(.+)/$b/;	# this must taint only the $_

check_taint      $_;
check_taint_not  $&;
check_taint_not  $`;
check_taint_not  $';
check_taint_not  $+;
check_taint_not  $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

s/(.+)/b/;	# this must not taint
check_taint_not  $_;
check_taint_not  $&;
check_taint_not  $`;
check_taint_not  $';
check_taint_not  $+;
check_taint_not  $1;
check_taint_not  $2;

$b = $a;	# untaint $b

($b = $a) =~ s/\w/$&/;
check_taint      $b;	# $b should be tainted.
check_taint_not  $a;	# $a should be not.

$_ = $a;	# untaint $_

s/(\w)/\l$1/;	# this must taint
check_taint      $_;
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

s/(\w)/\L$1/;	# this must taint
check_taint      $_;
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

s/(\w)/\u$1/;	# this must taint
check_taint      $_;
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

$_ = $a;	# untaint $_

s/(\w)/\U$1/;	# this must taint
check_taint      $_;
check_taint      $&;
check_taint      $`;
check_taint      $';
check_taint      $+;
check_taint      $1;
check_taint_not  $2;

# After all this tainting $a should be cool.

check_taint_not  $a;

{   # This is just the previous tests copied here with a different
    # compile-time pragma.

    use locale ':not_characters'; # engage restricted locale with different
                                  # tainting rules

    check_taint_not   $a;

    check_taint_not	uc($a);
    check_taint_not	"\U$a";
    check_taint_not	ucfirst($a);
    check_taint_not	"\u$a";
    check_taint_not	lc($a);
    check_taint_not	fc($a);
    check_taint_not	"\L$a";
    check_taint_not	"\F$a";
    check_taint_not	lcfirst($a);
    check_taint_not	"\l$a";

    check_taint_not  sprintf('%e', 123.456);
    check_taint_not  sprintf('%f', 123.456);
    check_taint_not  sprintf('%g', 123.456);
    check_taint_not  sprintf('%d', 123.456);
    check_taint_not  sprintf('%x', 123.456);

    $_ = $a;	# untaint $_

    $_ = uc($a);	# taint $_

    check_taint_not	$_;

    /(\w)/;	# taint $&, $`, $', $+, $1.
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    /(.)/;	# untaint $&, $`, $', $+, $1.
    check_taint_not  $&;
    check_taint_not  $`;
    check_taint_not  $';
    check_taint_not  $+;
    check_taint_not  $1;
    check_taint_not  $2;

    /(\W)/;	# taint $&, $`, $', $+, $1.
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    /(\s)/;	# taint $&, $`, $', $+, $1.
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    /(\S)/;	# taint $&, $`, $', $+, $1.
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    check_taint_not  $_;

    /(b)/;		# this must not taint
    check_taint_not  $&;
    check_taint_not  $`;
    check_taint_not  $';
    check_taint_not  $+;
    check_taint_not  $1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    check_taint_not  $_;

    $b = uc($a);	# taint $b
    s/(.+)/$b/;	# this must taint only the $_

    check_taint_not	$_;
    check_taint_not  $&;
    check_taint_not  $`;
    check_taint_not  $';
    check_taint_not  $+;
    check_taint_not  $1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    s/(.+)/b/;	# this must not taint
    check_taint_not  $_;
    check_taint_not  $&;
    check_taint_not  $`;
    check_taint_not  $';
    check_taint_not  $+;
    check_taint_not  $1;
    check_taint_not  $2;

    $b = $a;	# untaint $b

    ($b = $a) =~ s/\w/$&/;
    check_taint_not	$b;	# $b should be tainted.
    check_taint_not  $a;	# $a should be not.

    $_ = $a;	# untaint $_

    s/(\w)/\l$1/;	# this must taint
    check_taint_not	$_;
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    s/(\w)/\L$1/;	# this must taint
    check_taint_not	$_;
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    s/(\w)/\u$1/;	# this must taint
    check_taint_not	$_;
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    $_ = $a;	# untaint $_

    s/(\w)/\U$1/;	# this must taint
    check_taint_not	$_;
    check_taint_not	$&;
    check_taint_not	$`;
    check_taint_not	$';
    check_taint_not	$+;
    check_taint_not	$1;
    check_taint_not  $2;

    # After all this tainting $a should be cool.

    check_taint_not  $a;
}

# Here are in scope of 'use locale'

# I think we've seen quite enough of taint.
# Let us do some *real* locale work now,
# unless setlocale() is missing (i.e. minitest).

unless ($have_setlocale) {
    print "1..$test_num\n";
    exit;
}

# The test number before our first setlocale()
my $final_without_setlocale = $test_num;

# Find locales.

debug "# Scanning for locales...\n";

# Note that it's okay that some languages have their native names
# capitalized here even though that's not "right".  They are lowercased
# anyway later during the scanning process (and besides, some clueless
# vendor might have them capitalized erroneously anyway).

my $locales = <<EOF;
Afrikaans:af:za:1 15
Arabic:ar:dz eg sa:6 arabic8
Brezhoneg Breton:br:fr:1 15
Bulgarski Bulgarian:bg:bg:5
Chinese:zh:cn tw:cn.EUC eucCN eucTW euc.CN euc.TW Big5 GB2312 tw.EUC
Hrvatski Croatian:hr:hr:2
Cymraeg Welsh:cy:cy:1 14 15
Czech:cs:cz:2
Dansk Danish:dk:da:1 15
Nederlands Dutch:nl:be nl:1 15
English American British:en:au ca gb ie nz us uk zw:1 15 cp850
Esperanto:eo:eo:3
Eesti Estonian:et:ee:4 6 13
Suomi Finnish:fi:fi:1 15
Flamish::fl:1 15
Deutsch German:de:at be ch de lu:1 15
Euskaraz Basque:eu:es fr:1 15
Galego Galician:gl:es:1 15
Ellada Greek:el:gr:7 g8
Frysk:fy:nl:1 15
Greenlandic:kl:gl:4 6
Hebrew:iw:il:8 hebrew8
Hungarian:hu:hu:2
Indonesian:in:id:1 15
Gaeilge Irish:ga:IE:1 14 15
Italiano Italian:it:ch it:1 15
Nihongo Japanese:ja:jp:euc eucJP jp.EUC sjis
Korean:ko:kr:
Latine Latin:la:va:1 15
Latvian:lv:lv:4 6 13
Lithuanian:lt:lt:4 6 13
Macedonian:mk:mk:1 15
Maltese:mt:mt:3
Moldovan:mo:mo:2
Norsk Norwegian:no no\@nynorsk:no:1 15
Occitan:oc:es:1 15
Polski Polish:pl:pl:2
Rumanian:ro:ro:2
Russki Russian:ru:ru su ua:5 koi8 koi8r KOI8-R koi8u cp1251 cp866
Serbski Serbian:sr:yu:5
Slovak:sk:sk:2
Slovene Slovenian:sl:si:2
Sqhip Albanian:sq:sq:1 15
Svenska Swedish:sv:fi se:1 15
Thai:th:th:11 tis620
Turkish:tr:tr:9 turkish8
Yiddish:yi::1 15
EOF

if ($^O eq 'os390') {
    # These cause heartburn.  Broken locales?
    $locales =~ s/Svenska Swedish:sv:fi se:1 15\n//;
    $locales =~ s/Thai:th:th:11 tis620\n//;
}

sub in_utf8 () { $^H & 0x08 || (${^OPEN} || "") =~ /:utf8/ }

if (in_utf8) {
    require "lib/locale/utf8";
} else {
    require "lib/locale/latin1";
}

my @Locale;
my $Locale;
my @Alnum_;

sub trylocale {
    my $locale = shift;
    return if grep { $locale eq $_ } @Locale;
    return unless setlocale(LC_ALL, $locale);
    my $badutf8;
    {
        local $SIG{__WARN__} = sub {
            $badutf8 = $_[0] =~ /Malformed UTF-8/;
        };
        $Locale =~ /UTF-?8/i;
    }

    if ($badutf8) {
        ok(0, "Locale name contains malformed utf8");
        return;
    }
    push @Locale, $locale;
}

sub decode_encodings {
    my @enc;

    foreach (split(/ /, shift)) {
	if (/^(\d+)$/) {
	    push @enc, "ISO8859-$1";
	    push @enc, "iso8859$1";	# HP
	    if ($1 eq '1') {
		 push @enc, "roman8";	# HP
	    }
	} else {
	    push @enc, $_;
   	    push @enc, "$_.UTF-8";
	}
    }
    if ($^O eq 'os390') {
	push @enc, qw(IBM-037 IBM-819 IBM-1047);
    }

    return @enc;
}

trylocale("C");
trylocale("POSIX");
foreach (0..15) {
    trylocale("ISO8859-$_");
    trylocale("iso8859$_");
    trylocale("iso8859-$_");
    trylocale("iso_8859_$_");
    trylocale("isolatin$_");
    trylocale("isolatin-$_");
    trylocale("iso_latin_$_");
}

# Sanitize the environment so that we can run the external 'locale'
# program without the taint mode getting grumpy.

# $ENV{PATH} is special in VMS.
delete $ENV{PATH} if $^O ne 'VMS' or $Config{d_setenv};

# Other subversive stuff.
delete @ENV{qw(IFS CDPATH ENV BASH_ENV)};

if (-x "/usr/bin/locale" && open(LOCALES, "/usr/bin/locale -a 2>/dev/null|")) {
    while (<LOCALES>) {
	# It seems that /usr/bin/locale steadfastly outputs 8 bit data, which
	# ain't great when we're running this testPERL_UNICODE= so that utf8
	# locales will cause all IO hadles to default to (assume) utf8
	next unless utf8::valid($_);
        chomp;
	trylocale($_);
    }
    close(LOCALES);
} elsif ($^O eq 'VMS' && defined($ENV{'SYS$I18N_LOCALE'}) && -d 'SYS$I18N_LOCALE') {
# The SYS$I18N_LOCALE logical name search list was not present on
# VAX VMS V5.5-12, but was on AXP && VAX VMS V6.2 as well as later versions.
    opendir(LOCALES, "SYS\$I18N_LOCALE:");
    while ($_ = readdir(LOCALES)) {
        chomp;
        trylocale($_);
    }
    close(LOCALES);
} elsif ($^O eq 'openbsd' && -e '/usr/share/locale') {

   # OpenBSD doesn't have a locale executable, so reading /usr/share/locale
   # is much easier and faster than the last resort method.

    opendir(LOCALES, '/usr/share/locale');
    while ($_ = readdir(LOCALES)) {
        chomp;
        trylocale($_);
    }
    close(LOCALES);
} else {

    # This is going to be slow.

    foreach my $locale (split(/\n/, $locales)) {
	my ($locale_name, $language_codes, $country_codes, $encodings) =
	    split(/:/, $locale);
	my @enc = decode_encodings($encodings);
	foreach my $loc (split(/ /, $locale_name)) {
	    trylocale($loc);
	    foreach my $enc (@enc) {
		trylocale("$loc.$enc");
	    }
	    $loc = lc $loc;
	    foreach my $enc (@enc) {
		trylocale("$loc.$enc");
	    }
	}
	foreach my $lang (split(/ /, $language_codes)) {
	    trylocale($lang);
	    foreach my $country (split(/ /, $country_codes)) {
		my $lc = "${lang}_${country}";
		trylocale($lc);
		foreach my $enc (@enc) {
		    trylocale("$lc.$enc");
		}
		my $lC = "${lang}_\U${country}";
		trylocale($lC);
		foreach my $enc (@enc) {
		    trylocale("$lC.$enc");
		}
	    }
	}
    }
}

setlocale(LC_ALL, "C");

if ($^O eq 'darwin') {
    # Darwin 8/Mac OS X 10.4 and 10.5 have bad Basque locales: perl bug #35895,
    # Apple bug ID# 4139653. It also has a problem in Byelorussian.
    (my $v) = $Config{osvers} =~ /^(\d+)/;
    if ($v >= 8 and $v < 10) {
	debug "# Skipping eu_ES, be_BY locales -- buggy in Darwin\n";
	@Locale = grep ! m/^(eu_ES(?:\..*)?|be_BY\.CP1131)$/, @Locale;
    } elsif ($v < 12) {
	debug "# Skipping be_BY locales -- buggy in Darwin\n";
	@Locale = grep ! m/^be_BY\.CP1131$/, @Locale;
    }
}

@Locale = sort @Locale;

debug "# Locales =\n";
for ( @Locale ) {
    debug "# $_\n";
}

my %Problem;
my %Okay;
my %Testing;
my @Neoalpha;   # Alnums that aren't in the C locale.
my %test_names;

sub tryneoalpha {
    my ($Locale, $i, $test) = @_;
    unless ($test) {
	$Problem{$i}{$Locale} = 1;
	debug "# failed $i with locale '$Locale'\n";
    } else {
	push @{$Okay{$i}}, $Locale;
    }
}

my $first_locales_test_number = $final_without_setlocale + 1;
my $locales_test_number;
my $not_necessarily_a_problem_test_number;
my %setlocale_failed;   # List of locales that setlocale() didn't work on

foreach $Locale (@Locale) {
    $locales_test_number = $first_locales_test_number - 1;
    debug "# Locale = $Locale\n";

    unless (setlocale(LC_ALL, $Locale)) {
        $setlocale_failed{$Locale} = $Locale;
	next;
    }

    # We test UTF-8 locales only under ':not_characters'; otherwise they have
    # documented deficiencies.  Non- UTF-8 locales are tested only under plain
    # 'use locale', as otherwise we would have to convert everything in them
    # to Unicode.
    my $is_utf8_locale = $Locale =~ /UTF-?8/i;

    my %UPPER = ();
    my %lower = ();
    my %BoThCaSe = ();

    if (! $is_utf8_locale) {
        use locale;
        @Alnum_ = sort grep /\w/, map { chr } 0..255;

        debug "# w = ", join("",@Alnum_), "\n";

        # Sieve the uppercase and the lowercase.

        for (@Alnum_) {
            if (/[^\d_]/) { # skip digits and the _
                if (uc($_) eq $_) {
                    $UPPER{$_} = $_;
                }
                if (lc($_) eq $_) {
                    $lower{$_} = $_;
                }
            }
        }
    }
    else {
        use locale ':not_characters';
        @Alnum_ = sort grep /\w/, map { chr } 0..255;
        debug "# w = ", join("",@Alnum_), "\n";
        for (@Alnum_) {
            if (/[^\d_]/) { # skip digits and the _
                if (uc($_) eq $_) {
                    $UPPER{$_} = $_;
                }
                if (lc($_) eq $_) {
                    $lower{$_} = $_;
                }
            }
        }
    }
    foreach (keys %UPPER) {
	$BoThCaSe{$_}++ if exists $lower{$_};
    }
    foreach (keys %lower) {
	$BoThCaSe{$_}++ if exists $UPPER{$_};
    }
    foreach (keys %BoThCaSe) {
	delete $UPPER{$_};
	delete $lower{$_};
    }

    debug "# UPPER    = ", join("", sort keys %UPPER   ), "\n";
    debug "# lower    = ", join("", sort keys %lower   ), "\n";
    debug "# BoThCaSe = ", join("", sort keys %BoThCaSe), "\n";

    {   # Find the alphabetic characters that are not considered alphabetics
        # in the default (C) locale.

	no locale;

	@Neoalpha = ();
	for (keys %UPPER, keys %lower) {
	    push(@Neoalpha, $_) if (/\W/);
	}
    }

    @Neoalpha = sort @Neoalpha;

    debug "# Neoalpha = ", join("",@Neoalpha), "\n";

    my $first_Neoalpha_test_number =  $locales_test_number;
    my $final_Neoalpha_test_number =  $first_Neoalpha_test_number + 4;
    if (@Neoalpha == 0) {
	# If we have no Neoalphas the remaining tests are no-ops.
	debug "# no Neoalpha, skipping tests $locales_test_number..$final_Neoalpha_test_number for locale '$Locale'\n";
	foreach ($locales_test_number+1..$final_Neoalpha_test_number) {
	    push @{$Okay{$_}}, $Locale;
            $locales_test_number++;
	}
    } else {

	# Test \w.

	my $word = join('', @Neoalpha);

        ++$locales_test_number;
        $test_names{$locales_test_number} = 'Verify that alnums outside the C locale match \w';
        my $ok;
        if ($is_utf8_locale) {
            use locale ':not_characters';
	    $ok = $word =~ /^(\w+)$/;
        }
        else {
            # Already in 'use locale'; this tests that exiting scopes works
	    $ok = $word =~ /^(\w+)$/;
        }
        tryneoalpha($Locale, $locales_test_number, $ok);

	# Cross-check the whole 8-bit character set.

        ++$locales_test_number;
        $test_names{$locales_test_number} = 'Verify that \w and \W are mutually exclusive, as are \d, \D; \s, \S';
	for (map { chr } 0..255) {
            if ($is_utf8_locale) {
                use locale ':not_characters';
	        $ok =   (/\w/ xor /\W/) ||
			(/\d/ xor /\D/) ||
			(/\s/ xor /\S/);
            }
            else {
	        $ok =   (/\w/ xor /\W/) ||
			(/\d/ xor /\D/) ||
			(/\s/ xor /\S/);
            }
	    tryneoalpha($Locale, $locales_test_number, $ok);
	}

	# Test for read-only scalars' locale vs non-locale comparisons.

	{
	    no locale;
	    $a = "qwerty";
            if ($is_utf8_locale) {
                use locale ':not_characters';
                $ok = ($a cmp "qwerty") == 0;
            }
            else {
                use locale;
                $ok = ($a cmp "qwerty") == 0;
            }
            tryneoalpha($Locale, ++$locales_test_number, $ok);
            $test_names{$locales_test_number} = 'Verify that cmp works with a read-only scalar; no- vs locale';
	}

	{
	    my ($from, $to, $lesser, $greater,
		@test, %test, $test, $yes, $no, $sign);

            ++$locales_test_number;
            $test_names{$locales_test_number} = 'Verify that "le", "ne", etc work';
            $not_necessarily_a_problem_test_number = $locales_test_number;
	    for (0..9) {
		# Select a slice.
		$from = int(($_*@Alnum_)/10);
		$to = $from + int(@Alnum_/10);
		$to = $#Alnum_ if ($to > $#Alnum_);
		$lesser  = join('', @Alnum_[$from..$to]);
		# Select a slice one character on.
		$from++; $to++;
		$to = $#Alnum_ if ($to > $#Alnum_);
		$greater = join('', @Alnum_[$from..$to]);
                if ($is_utf8_locale) {
                    use locale ':not_characters';
                    ($yes, $no, $sign) = ($lesser lt $greater
				      ? ("    ", "not ", 1)
				      : ("not ", "    ", -1));
                }
                else {
                    use locale;
                    ($yes, $no, $sign) = ($lesser lt $greater
				      ? ("    ", "not ", 1)
				      : ("not ", "    ", -1));
                }
		# all these tests should FAIL (return 0).
		# Exact lt or gt cannot be tested because
		# in some locales, say, eacute and E may test equal.
		@test =
		    (
		     $no.'    ($lesser  le $greater)',  # 1
		     'not      ($lesser  ne $greater)', # 2
		     '         ($lesser  eq $greater)', # 3
		     $yes.'    ($lesser  ge $greater)', # 4
		     $yes.'    ($lesser  ge $greater)', # 5
		     $yes.'    ($greater le $lesser )', # 7
		     'not      ($greater ne $lesser )', # 8
		     '         ($greater eq $lesser )', # 9
		     $no.'     ($greater ge $lesser )', # 10
		     'not (($lesser cmp $greater) == -($sign))' # 11
		     );
		@test{@test} = 0 x @test;
		$test = 0;
		for my $ti (@test) {
                    if ($is_utf8_locale) {
                        use locale ':not_characters';
                        $test{$ti} = eval $ti;
                    }
                    else {
                        # Already in 'use locale';
                        $test{$ti} = eval $ti;
                    }
		    $test ||= $test{$ti}
		}
                tryneoalpha($Locale, $locales_test_number, $test == 0);
		if ($test) {
		    debug "# lesser  = '$lesser'\n";
		    debug "# greater = '$greater'\n";
		    debug "# lesser cmp greater = ",
		          $lesser cmp $greater, "\n";
		    debug "# greater cmp lesser = ",
		          $greater cmp $lesser, "\n";
		    debug "# (greater) from = $from, to = $to\n";
		    for my $ti (@test) {
			debugf("# %-40s %-4s", $ti,
			       $test{$ti} ? 'FAIL' : 'ok');
			if ($ti =~ /\(\.*(\$.+ +cmp +\$[^\)]+)\.*\)/) {
			    debugf("(%s == %4d)", $1, eval $1);
			}
			debug "\n#";
		    }

		    last;
		}
	    }
	}
    }

    if ($locales_test_number != $final_Neoalpha_test_number) {
        die("The delta for \$final_Neoalpha needs to be updated from "
            . ($final_Neoalpha_test_number - $first_Neoalpha_test_number)
            . " to "
            . ($locales_test_number - $first_Neoalpha_test_number)
            );
    }

    my $ok1;
    my $ok2;
    my $ok3;
    my $ok4;
    my $ok5;
    my $ok6;
    my $ok7;
    my $ok8;
    my $ok9;
    my $ok10;
    my $ok11;
    my $ok12;
    my $ok13;

    my $c;
    my $d;
    my $e;
    my $f;
    my $g;

    if (! $is_utf8_locale) {
        use locale;

        my ($x, $y) = (1.23, 1.23);

        $a = "$x";
        printf ''; # printf used to reset locale to "C"
        $b = "$y";
        $ok1 = $a eq $b;

        $c = "$x";
        my $z = sprintf ''; # sprintf used to reset locale to "C"
        $d = "$y";
        $ok2 = $c eq $d;
        {

            use warnings;
            my $w = 0;
            local $SIG{__WARN__} =
                sub {
                    print "# @_\n";
                    $w++;
                };

            # The == (among other ops) used to warn for locales
            # that had something else than "." as the radix character.

            $ok3 = $c == 1.23;
            $ok4 = $c == $x;
            $ok5 = $c == $d;
            {
                no locale;

                # The earlier test was $e = "$x".  But this fails [perl
                # #108378], and the "no locale" was commented out.  But doing
                # that made all the tests in the block after this one
                # meaningless, as originally it was testing the nesting of a
                # "no locale" scope, and how it recovers after that scope is
                # done.  So I (khw) filed a bug report and changed this so it
                # wouldn't fail.  It seemed too much work to add TODOs
                # instead.  Should this be fixed, the following test names
                # would need to be revised; they mostly don't really test
                # anything currently.
                $e = $x;

                $ok6 = $e == 1.23;
                $ok7 = $e == $x;
                $ok8 = $e == $c;
            }

            $f = "1.23";
            $g = 2.34;

            $ok9 = $f == 1.23;
            $ok10 = $f == $x;
            $ok11 = $f == $c;
            $ok12 = abs(($f + $g) - 3.57) < 0.01;
            $ok13 = $w == 0;
        }
    }
    else {
        use locale ':not_characters';

        my ($x, $y) = (1.23, 1.23);
        $a = "$x";
        printf ''; # printf used to reset locale to "C"
        $b = "$y";
        $ok1 = $a eq $b;

        $c = "$x";
        my $z = sprintf ''; # sprintf used to reset locale to "C"
        $d = "$y";
        $ok2 = $c eq $d;
        {
            use warnings;
            my $w = 0;
            local $SIG{__WARN__} =
                sub {
                    print "# @_\n";
                    $w++;
                };
            $ok3 = $c == 1.23;
            $ok4 = $c == $x;
            $ok5 = $c == $d;
            {
                no locale;
                $e = $x;

                $ok6 = $e == 1.23;
                $ok7 = $e == $x;
                $ok8 = $e == $c;
            }

            $f = "1.23";
            $g = 2.34;

            $ok9 = $f == 1.23;
            $ok10 = $f == $x;
            $ok11 = $f == $c;
            $ok12 = abs(($f + $g) - 3.57) < 0.01;
            $ok13 = $w == 0;
        }
    }

    tryneoalpha($Locale, ++$locales_test_number, $ok1);
    $test_names{$locales_test_number} = 'Verify that an intervening printf doesn\'t change assignment results';
    my $first_a_test = $locales_test_number;

    debug "# $first_a_test..$locales_test_number: \$a = $a, \$b = $b, Locale = $Locale\n";

    tryneoalpha($Locale, ++$locales_test_number, $ok2);
    $test_names{$locales_test_number} = 'Verify that an intervening sprintf doesn\'t change assignment results';

    my $first_c_test = $locales_test_number;

    tryneoalpha($Locale, ++$locales_test_number, $ok3);
    $test_names{$locales_test_number} = 'Verify that a different locale radix works when doing "==" with a constant';

    tryneoalpha($Locale, ++$locales_test_number, $ok4);
    $test_names{$locales_test_number} = 'Verify that a different locale radix works when doing "==" with a scalar';

    tryneoalpha($Locale, ++$locales_test_number, $ok5);
    $test_names{$locales_test_number} = 'Verify that a different locale radix works when doing "==" with a scalar and an intervening sprintf';

    debug "# $first_c_test..$locales_test_number: \$c = $c, \$d = $d, Locale = $Locale\n";

    tryneoalpha($Locale, ++$locales_test_number, $ok6);
    $test_names{$locales_test_number} = 'Verify that can assign numerically under inner no-locale block';
    my $first_e_test = $locales_test_number;

    tryneoalpha($Locale, ++$locales_test_number, $ok7);
    $test_names{$locales_test_number} = 'Verify that "==" with a scalar still works in inner no locale';

    tryneoalpha($Locale, ++$locales_test_number, $ok8);
    $test_names{$locales_test_number} = 'Verify that "==" with a scalar and an intervening sprintf still works in inner no locale';

    debug "# $first_e_test..$locales_test_number: \$e = $e, no locale\n";

    tryneoalpha($Locale, ++$locales_test_number, $ok9);
    $test_names{$locales_test_number} = 'Verify that after a no-locale block, a different locale radix still works when doing "==" with a constant';
    my $first_f_test = $locales_test_number;

    tryneoalpha($Locale, ++$locales_test_number, $ok10);
    $test_names{$locales_test_number} = 'Verify that after a no-locale block, a different locale radix still works when doing "==" with a scalar';

    tryneoalpha($Locale, ++$locales_test_number, $ok11);
    $test_names{$locales_test_number} = 'Verify that after a no-locale block, a different locale radix still works when doing "==" with a scalar and an intervening sprintf';

    tryneoalpha($Locale, ++$locales_test_number, $ok12);
    $test_names{$locales_test_number} = 'Verify that after a no-locale block, a different locale radix can participate in an addition and function call as numeric';

    tryneoalpha($Locale, ++$locales_test_number, $ok13);
    $test_names{$locales_test_number} = 'Verify that don\'t get warning under "==" even if radix is not a dot';

    debug "# $first_f_test..$locales_test_number: \$f = $f, \$g = $g, back to locale = $Locale\n";

    # Does taking lc separately differ from taking
    # the lc "in-line"?  (This was the bug 19990704.002, change #3568.)
    # The bug was in the caching of the 'o'-magic.
    if (! $is_utf8_locale) {
	use locale;

	sub lcA {
	    my $lc0 = lc $_[0];
	    my $lc1 = lc $_[1];
	    return $lc0 cmp $lc1;
	}

        sub lcB {
	    return lc($_[0]) cmp lc($_[1]);
	}

        my $x = "ab";
        my $y = "aa";
        my $z = "AB";

        tryneoalpha($Locale, ++$locales_test_number,
		    lcA($x, $y) == 1 && lcB($x, $y) == 1 ||
		    lcA($x, $z) == 0 && lcB($x, $z) == 0);
    }
    else {
	use locale ':not_characters';

	sub lcC {
	    my $lc0 = lc $_[0];
	    my $lc1 = lc $_[1];
	    return $lc0 cmp $lc1;
	}

        sub lcD {
	    return lc($_[0]) cmp lc($_[1]);
	}

        my $x = "ab";
        my $y = "aa";
        my $z = "AB";

        tryneoalpha($Locale, ++$locales_test_number,
		    lcC($x, $y) == 1 && lcD($x, $y) == 1 ||
		    lcC($x, $z) == 0 && lcD($x, $z) == 0);
    }
    $test_names{$locales_test_number} = 'Verify "lc(foo) cmp lc(bar)" is the same as using intermediaries for the cmp';

    # Does lc of an UPPER (if different from the UPPER) match
    # case-insensitively the UPPER, and does the UPPER match
    # case-insensitively the lc of the UPPER.  And vice versa.
    {
        use locale;
        no utf8;
        my $re = qr/[\[\(\{\*\+\?\|\^\$\\]/;

        my @f = ();
        ++$locales_test_number;
        $test_names{$locales_test_number} = 'Verify case insensitive matching works';
        foreach my $x (sort keys %UPPER) {
            if (! $is_utf8_locale) {
                my $y = lc $x;
                next unless uc $y eq $x;
                print "# UPPER $x lc $y ",
                        $x =~ /$y/i ? 1 : 0, " ",
                        $y =~ /$x/i ? 1 : 0, "\n" if 0;
                #
                # If $x and $y contain regular expression characters
                # AND THEY lowercase (/i) to regular expression characters,
                # regcomp() will be mightily confused.  No, the \Q doesn't
                # help here (maybe regex engine internal lowercasing
                # is done after the \Q?)  An example of this happening is
                # the bg_BG (Bulgarian) locale under EBCDIC (OS/390 USS):
                # the chr(173) (the "[") is the lowercase of the chr(235).
                #
                # Similarly losing EBCDIC locales include cs_cz, cs_CZ,
                # el_gr, el_GR, en_us.IBM-037 (!), en_US.IBM-037 (!),
                # et_ee, et_EE, hr_hr, hr_HR, hu_hu, hu_HU, lt_LT,
                # mk_mk, mk_MK, nl_nl.IBM-037, nl_NL.IBM-037,
                # pl_pl, pl_PL, ro_ro, ro_RO, ru_ru, ru_RU,
                # sk_sk, sk_SK, sl_si, sl_SI, tr_tr, tr_TR.
                #
                # Similar things can happen even under (bastardised)
                # non-EBCDIC locales: in many European countries before the
                # advent of ISO 8859-x nationally customised versions of
                # ISO 646 were devised, reusing certain punctuation
                # characters for modified characters needed by the
                # country/language.  For example, the "|" might have
                # stood for U+00F6 or LATIN SMALL LETTER O WITH DIAERESIS.
                #
                if ($x =~ $re || $y =~ $re) {
                    print "# Regex characters in '$x' or '$y', skipping test $locales_test_number for locale '$Locale'\n";
                    next;
                }
                # With utf8 both will fail since the locale concept
                # of upper/lower does not work well in Unicode.
                push @f, $x unless $x =~ /$y/i == $y =~ /$x/i;

                # fc is not a locale concept, so Perl uses lc for it.
                push @f, $x unless lc $x eq fc $x;
            }
            else {
                use locale ':not_characters';
                my $y = lc $x;
                next unless uc $y eq $x;
                print "# UPPER $x lc $y ",
                        $x =~ /$y/i ? 1 : 0, " ",
                        $y =~ /$x/i ? 1 : 0, "\n" if 0;

                # Here, we can fully test things, unlike plain 'use locale',
                # because this form does work well with Unicode
                push @f, $x unless $x =~ /$y/i && $y =~ /$x/i;

                # The places where Unicode's lc is different from fc are
                # skipped here by virtue of the 'next unless uc...' line above
                push @f, $x unless lc $x eq fc $x;
            }
        }

	foreach my $x (sort keys %lower) {
            if (! $is_utf8_locale) {
                my $y = uc $x;
                next unless lc $y eq $x;
                print "# lower $x uc $y ",
                    $x =~ /$y/i ? 1 : 0, " ",
                    $y =~ /$x/i ? 1 : 0, "\n" if 0;
                if ($x =~ $re || $y =~ $re) { # See above.
                    print "# Regex characters in '$x' or '$y', skipping test $locales_test_number for locale '$Locale'\n";
                    next;
                }
                # With utf8 both will fail since the locale concept
                # of upper/lower does not work well in Unicode.
                push @f, $x unless $x =~ /$y/i == $y =~ /$x/i;

                push @f, $x unless lc $x eq fc $x;
            }
            else {
                use locale ':not_characters';
                my $y = uc $x;
                next unless lc $y eq $x;
                print "# lower $x uc $y ",
                        $x =~ /$y/i ? 1 : 0, " ",
                        $y =~ /$x/i ? 1 : 0, "\n" if 0;
                push @f, $x unless $x =~ /$y/i && $y =~ /$x/i;

                push @f, $x unless lc $x eq fc $x;
            }
	}
	tryneoalpha($Locale, $locales_test_number, @f == 0);
	if (@f) {
	    print "# failed $locales_test_number locale '$Locale' characters @f\n"
	}
    }
}

my $final_locales_test_number = $locales_test_number;

# Recount the errors.

foreach ($first_locales_test_number..$final_locales_test_number) {
    if (%setlocale_failed) {
        print "not ";
    }
    elsif ($Problem{$_} || !defined $Okay{$_} || !@{$Okay{$_}}) {
	if (defined $not_necessarily_a_problem_test_number
            && $_ == $not_necessarily_a_problem_test_number)
        {
	    print "# The failure of test $not_necessarily_a_problem_test_number is not necessarily fatal.\n";
	    print "# It usually indicates a problem in the environment,\n";
	    print "# not in Perl itself.\n";
	}
	print "not ";
    }
    print "ok $_";
    print " $test_names{$_}" if defined $test_names{$_};
    print "\n";
}

# Give final advice.

my $didwarn = 0;

foreach ($first_locales_test_number..$final_locales_test_number) {
    if ($Problem{$_}) {
	my @f = sort keys %{ $Problem{$_} };
	my $f = join(" ", @f);
	$f =~ s/(.{50,60}) /$1\n#\t/g;
	print
	    "#\n",
            "# The locale ", (@f == 1 ? "definition" : "definitions"), "\n#\n",
	    "#\t", $f, "\n#\n",
	    "# on your system may have errors because the locale test $_\n",
            "# failed in ", (@f == 1 ? "that locale" : "those locales"),
            ".\n";
	print <<EOW;
#
# If your users are not using these locales you are safe for the moment,
# but please report this failure first to perlbug\@perl.com using the
# perlbug script (as described in the INSTALL file) so that the exact
# details of the failures can be sorted out first and then your operating
# system supplier can be alerted about these anomalies.
#
EOW
	$didwarn = 1;
    }
}

# Tell which locales were okay and which were not.

if ($didwarn) {
    my (@s, @F);

    foreach my $l (@Locale) {
	my $p = 0;
        if ($setlocale_failed{$l}) {
            $p++;
        }
        else {
            foreach my $t
                        ($first_locales_test_number..$final_locales_test_number)
            {
                $p++ if $Problem{$t}{$l};
            }
	}
	push @s, $l if $p == 0;
        push @F, $l unless $p == 0;
    }

    if (@s) {
        my $s = join(" ", @s);
        $s =~ s/(.{50,60}) /$1\n#\t/g;

        warn
    	    "# The following locales\n#\n",
            "#\t", $s, "\n#\n",
	    "# tested okay.\n#\n",
    } else {
        warn "# None of your locales were fully okay.\n";
    }

    if (@F) {
        my $F = join(" ", @F);
        $F =~ s/(.{50,60}) /$1\n#\t/g;

        warn
          "# The following locales\n#\n",
          "#\t", $F, "\n#\n",
          "# had problems.\n#\n",
    } else {
        warn "# None of your locales were broken.\n";
    }
}

$test_num = $final_locales_test_number;

# Test that tainting and case changing works on utf8 strings.  These tests are
# placed last to avoid disturbing the hard-coded test numbers that existed at
# the time these were added above this in this file.
# This also tests that locale overrides unicode_strings in the same scope for
# non-utf8 strings.
setlocale(LC_ALL, "C");
{
    use locale;
    use feature 'unicode_strings';

    foreach my $function ("uc", "ucfirst", "lc", "lcfirst", "fc") {
        my @list;   # List of code points to test for $function

        # Used to calculate the changed case for ASCII characters by using the
        # ord, instead of using one of the functions under test.
        my $ascii_case_change_delta;
        my $above_latin1_case_change_delta; # Same for the specific ords > 255
                                            # that we use

        # We test an ASCII character, which should change case and be tainted;
        # a Latin1 character, which shouldn't change case under this C locale,
        #   and is tainted.
        # an above-Latin1 character that when the case is changed would cross
        #   the 255/256 boundary, so doesn't change case and isn't tainted
        # (the \x{149} is one of these, but changes into 2 characters, the
        #   first one of which doesn't cross the boundary.
        # the final one in each list is an above-Latin1 character whose case
        #   does change, and shouldn't be tainted.  The code below uses its
        #   position in its list as a marker to indicate that it, unlike the
        #   other code points above ASCII, has a successful case change
        if ($function =~ /^u/) {
            @list = ("", "a", "\xe0", "\xff", "\x{fb00}", "\x{149}", "\x{101}");
            $ascii_case_change_delta = -32;
            $above_latin1_case_change_delta = -1;
        }
        else {
            @list = ("", "A", "\xC0", "\x{1E9E}", "\x{100}");
            $ascii_case_change_delta = +32;
            $above_latin1_case_change_delta = +1;
        }
        foreach my $is_utf8_locale (0 .. 1) {
            foreach my $j (0 .. $#list) {
                my $char = $list[$j];

                for my $encoded_in_utf8 (0 .. 1) {
                    my $should_be;
                    my $changed;
                    if (! $is_utf8_locale) {
                        $should_be = ($j == $#list)
                            ? chr(ord($char) + $above_latin1_case_change_delta)
                            : (length $char == 0 || ord($char) > 127)
                            ? $char
                            : chr(ord($char) + $ascii_case_change_delta);

                        # This monstrosity is in order to avoid using an eval,
                        # which might perturb the results
                        $changed = ($function eq "uc")
                                    ? uc($char)
                                    : ($function eq "ucfirst")
                                      ? ucfirst($char)
                                      : ($function eq "lc")
                                        ? lc($char)
                                        : ($function eq "lcfirst")
                                          ? lcfirst($char)
                                          : ($function eq "fc")
                                            ? fc($char)
                                            : die("Unexpected function \"$function\"");
                    }
                    else {
                        {
                            no locale;

                            # For utf8-locales the case changing functions
                            # should work just like they do outside of locale.
                            # Can use eval here because not testing it when
                            # not in locale.
                            $should_be = eval "$function('$char')";
                            die "Unexpected eval error $@ from 'eval \"$function('$char')\"'" if  $@;

                        }
                        use locale ':not_characters';
                        $changed = ($function eq "uc")
                                    ? uc($char)
                                    : ($function eq "ucfirst")
                                      ? ucfirst($char)
                                      : ($function eq "lc")
                                        ? lc($char)
                                        : ($function eq "lcfirst")
                                          ? lcfirst($char)
                                          : ($function eq "fc")
                                            ? fc($char)
                                            : die("Unexpected function \"$function\"");
                    }
                    ok($changed eq $should_be,
                        "$function(\"$char\") in C locale "
                        . (($is_utf8_locale)
                            ? "(use locale ':not_characters'"
                            : "(use locale")
                        . (($encoded_in_utf8)
                            ? "; encoded in utf8)"
                            : "; not encoded in utf8)")
                        . " should be \"$should_be\", got \"$changed\"");

                    # Tainting shouldn't happen for utf8 locales, empty
                    # strings, or those characters above 255.
                    (! $is_utf8_locale && length($char) > 0 && ord($char) < 256)
                    ? check_taint($changed)
                    : check_taint_not($changed);

                    # Use UTF-8 next time through the loop
                    utf8::upgrade($char);
                }
            }
        }
    }
}

print "1..$test_num\n";

# eof
