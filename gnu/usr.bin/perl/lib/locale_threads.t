use strict;
use warnings;

# This file tests interactions with locale and threads

BEGIN {
    $| = 1;

    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');

    skip_all_without_config('useithreads');
    skip_all("Fails on threaded builds on OpenBSD")
        if ($^O =~ m/^(openbsd)$/);

    require './loc_tools.pl';

    eval { require POSIX; POSIX->import(qw(errno_h locale_h unistd_h )) };
    if ($@) {
        skip_all("could not load the POSIX module"); # running minitest?
    }
}

use Time::HiRes qw(time usleep);

use Devel::Peek;
$Devel::Peek::pv_limit = 0; $Devel::Peek::pv_limit = 0;
use Data::Dumper;
$Data::Dumper::Sortkeys=1;
$Data::Dumper::Useqq = 1;
$Data::Dumper::Deepcopy = 1;

my $debug = 0;

my %map_category_name_to_number;
my %map_category_number_to_name;
my @valid_categories = valid_locale_categories();
foreach my $category (@valid_categories) {
    my $cat_num = eval "&POSIX::$category";
    die "Can't determine ${category}'s number: $@" if $@;

    $map_category_name_to_number{$category} = $cat_num;
    $map_category_number_to_name{$cat_num} = $category;
}

my $LC_ALL;
my $LC_ALL_string;
if (defined $map_category_name_to_number{LC_ALL}) {
    $LC_ALL_string = 'LC_ALL';
    $LC_ALL = $map_category_name_to_number{LC_ALL};
}
elsif (defined $map_category_name_to_number{LC_CTYPE}) {
    $LC_ALL_string = 'LC_CTYPE';
    $LC_ALL = $map_category_name_to_number{LC_CTYPE};
}
else {
    skip_all("No LC_ALL nor LC_CTYPE");
}

# reset the locale environment
delete local @ENV{'LANGUAGE', 'LANG', keys %map_category_name_to_number};

my @locales = find_locales($LC_ALL);
skip_all("Couldn't find any locales") if @locales == 0;

plan(2);

my ($utf8_locales_ref, $non_utf8_locales_ref)
                                    = classify_locales_wrt_utf8ness(\@locales);

my $official_ascii_name = 'ansi_x341968';

my %lang_code_to_script = (     # ISO 639.2, but without the many codes that
                                # are for latin (but the few western European
                                # ones that are latin1 are included)
                            am          => 'amharic',
                            amh         => 'amharic',
                            amharic     => 'amharic',
                            ar          => 'arabic',
                            be          => 'cyrillic',
                            bel         => 'cyrillic',
                            ben         => 'bengali',
                            bn          => 'bengali',
                            bg          => 'cyrillic',
                            bul         => 'cyrillic',
                            bulgarski   => 'cyrillic',
                            bulgarian   => 'cyrillic',
                            c           => $official_ascii_name,
                            cnr         => 'cyrillic',
                            de          => 'latin_1',
                            deu         => 'latin_1',
                            deutsch     => 'latin_1',
                            german      => 'latin_1',
                            div         => 'thaana',
                            dv          => 'thaana',
                            dzo         => 'tibetan',
                            dz          => 'tibetan',
                            el          => 'greek',
                            ell         => 'greek',
                            ellada      => 'greek',
                            en          => $official_ascii_name,
                            eng         => $official_ascii_name,
                            american    => $official_ascii_name,
                            british     => $official_ascii_name,
                            es          => 'latin_1',
                            fa          => 'arabic',
                            fas         => 'arabic',
                            flamish     => 'latin_1',
                            fra         => 'latin_1',
                            fr          => 'latin_1',
                            heb         => 'hebrew',
                            he          => 'hebrew',
                            hi          => 'hindi',
                            hin         => 'hindi',
                            hy          => 'armenian',
                            hye         => 'armenian',
                            ita         => 'latin_1',
                            it          => 'latin_1',
                            ja          => 'katakana',
                            jpn         => 'katakana',
                            nihongo     => 'katakana',
                            japanese    => 'katakana',
                            ka          => 'georgian',
                            kat         => 'georgian',
                            kaz         => 'cyrillic',
                            khm         => 'khmer',
                            kir         => 'cyrillic',
                            kk          => 'cyrillic',
                            km          => 'khmer',
                            ko          => 'hangul',
                            kor         => 'hangul',
                            korean      => 'hangul',
                            ku          => 'arabic',
                            kur         => 'arabic',
                            ky          => 'cyrillic',
                            latin1      => 'latin_1',
                            lao         => 'lao',
                            lo          => 'lao',
                            mk          => 'cyrillic',
                            mkd         => 'cyrillic',
                            macedonian  => 'cyrillic',
                            mn          => 'cyrillic',
                            mon         => 'cyrillic',
                            mya         => 'myanmar',
                            my          => 'myanmar',
                            ne          => 'devanagari',
                            nep         => 'devanagari',
                            nld         => 'latin_1',
                            nl          => 'latin_1',
                            nederlands  => 'latin_1',
                            dutch       => 'latin_1',
                            por         => 'latin_1',
                            posix       => $official_ascii_name,
                            ps          => 'arabic',
                            pt          => 'latin_1',
                            pus         => 'arabic',
                            ru          => 'cyrillic',
                            russki      => 'cyrillic',
                            russian     => 'cyrillic',
                            rus         => 'cyrillic',
                            sin         => 'sinhala',
                            si          => 'sinhala',
                            so          => 'arabic',
                            som         => 'arabic',
                            spa         => 'latin_1',
                            sr          => 'cyrillic',
                            srp         => 'cyrillic',
                            tam         => 'tamil',
                            ta          => 'tamil',
                            tg          => 'cyrillic',
                            tgk         => 'cyrillic',
                            tha         => 'thai',
                            th          => 'thai',
                            thai        => 'thai',
                            ti          => 'ethiopian',
                            tir         => 'ethiopian',
                            uk          => 'cyrillic',
                            ukr         => 'cyrillic',
                            ur          => 'arabic',
                            urd         => 'arabic',
                            zgh         => 'arabic',
                            zh          => 'chinese',
                            zho         => 'chinese',
                        );
my %codeset_to_script = (
                            88591  => 'latin_1',
                            88592  => 'latin_2',
                            88593  => 'latin_3',
                            88594  => 'latin_4',
                            88595  => 'cyrillic',
                            88596  => 'arabic',
                            88597  => 'greek',
                            88598  => 'hebrew',
                            88599  => 'latin_5',
                            885910 => 'latin_6',
                            885911 => 'thai',
                            885912 => 'devanagari',
                            885913 => 'latin_7',
                            885914 => 'latin_8',
                            885915 => 'latin_9',
                            885916 => 'latin_10',
                            cp1251 => 'cyrillic',
                            cp1255 => 'hebrew',
                      );

my %script_priorities = (       # In trying to make the results as distinct as
                                # possible, make the ones closest to Unicode,
                                # and ASCII lowest priority
                            $official_ascii_name => 15,
                            latin_1 => 14,
                            latin_9 => 13,
                            latin_2 => 12,
                            latin_4 => 12,
                            latin_5 => 12,
                            latin_6 => 12,
                            latin_7 => 12,
                            latin_8 => 12,
                            latin_10 => 12,
                            latin   => 11,  # Unknown latin version
                        );

my %script_instances;   # Keys are scripts, values are how many locales use
                        # this script.

sub analyze_locale_name($) {

    # Takes the input name of a locale and creates (and returns) a hash
    # containing information about that locale

    my %ret;
    my $input_locale_name = shift;

    my $old_locale = setlocale(LC_CTYPE);

    # Often a locale has multiple aliases, and the base one is returned
    # by setlocale() when called with an alias.  The base is more likely to
    # meet the XPG standards than the alias.
    my $new_locale = setlocale(LC_CTYPE, $input_locale_name);
    if (! $new_locale) {
        diag "Unexpectedly can't setlocale(LC_CTYPE, $new_locale);"
           . " \$!=$!, \$^E=$^E";
        return;
    }

    $ret{locale_name} = $new_locale;

    # XPG standard for locale names:
    #   language[_territory[.codeset]][@modifier]
    # But, there are instances which violate this, where there is a codeset
    # without a territory, so instead match:
    #   language[_territory][.codeset][@modifier]
    $ret{locale_name} =~ / ^
                                      ( .+? )          # language
                              (?:  _  ( .+? ) )?       # territory
                              (?: \.  ( .+? ) )?       # codeset
                              (?: \@  ( .+  ) )?       # modifier
                            $
                         /x;

    $ret{language}  = $1 // "";
    $ret{territory} = $2 // "";
    $ret{codeset}   = $3 // "";
    $ret{modifier}  = $4 // "";

    # Normalize all but 'territory' to lowercase
    foreach my $key (qw(language codeset modifier)) {
        $ret{$key} = lc $ret{$key};
    }

    # Often, the codeset is omitted from the locale name, but it is still
    # discoverable (via langinfo() ) for the current locale on many platforms.
    # We already have switched locales
    use I18N::Langinfo qw(langinfo CODESET);
    my $langinfo_codeset = lc langinfo(CODESET);

    # Now can switch back to the locale current on entry to this sub
    if (! setlocale(LC_CTYPE, $old_locale)) {
        die "Unexpectedly can't restore locale to $old_locale from"
          . " $new_locale; \$!=$!, \$^E=$^E";
    }

    # Normalize the codesets
    foreach my $codeset_ref (\$langinfo_codeset, \$ret{codeset}) {
        $$codeset_ref =~ s/\W//g;
        $$codeset_ref =~ s/iso8859/8859/g;
        $$codeset_ref =~ s/\b65001\b/utf8/;     # Windows synonym
        $$codeset_ref =~ s/\b646\b/$official_ascii_name/;
        $$codeset_ref =~ s/\busascii\b/$official_ascii_name/;
    }

    # The langinfo codeset, if found, is considered more reliable than the one
    # in the name.  (This is because libc looks into the actual data
    # definition.)  So use it unconditionally when found.  But note any
    # discrepancy as an aid for improving this test.
    if ($langinfo_codeset) {
        if ($ret{codeset} && $ret{codeset} ne $langinfo_codeset) {
            diag "In $ret{locale_name}, codeset from langinfo"
               . " ($langinfo_codeset) doesn't match codeset in"
               . " locale_name ($ret{codeset})";
        }
        $ret{codeset} = $langinfo_codeset;
    }

    $ret{is_utf8} = 0 + ($ret{codeset} eq 'utf8');

    # If the '@' modifier is a known script, use it as the script.
    if (    $ret{modifier}
        and grep { $_ eq $ret{modifier} } values %lang_code_to_script)
    {
        $ret{script} = $ret{nominal_script} = $ret{modifier};
        $ret{modifier} = "";
    }
    elsif ($ret{codeset} && ! $ret{is_utf8}) {

        # The codeset determines the script being used, except if we don't
        # have the codeset, or it is UTF-8 (which covers a multitude of
        # scripts).
        #
        # We have hard-coded the scripts corresponding to a few of these
        # non-UTF-8 codesets.  See if this is one of them.
        $ret{script} = $codeset_to_script{$ret{codeset}};
        if ($ret{script}) {

            # For these, the script is likely a combination of ASCII (from
            # 0-127), and the script from (128-255).  Reflect that in the name
            # used (for distinguishing below)
            $ret{script} .= '_' . $official_ascii_name;
        }
        elsif ($ret{codeset} =~ /^koi/) {   # Another common set.
            $ret{script} = "cyrillic_${official_ascii_name}";
        }
        else {  # Here the codeset name is unknown to us.  Just assume it
                # means a whole new script.  Add the language at the end of
                # the name to further make it distinct
            $ret{script} = $ret{codeset};
            $ret{script} .= "_$ret{language}"
                                    if $ret{codeset} !~ /$official_ascii_name/;
        }
    }
    else {  # Here, the codeset is unknown or is UTF-8.

        # In these cases look up the script based on the language.  The table
        # is meant to be pretty complete, but omits the many scripts that are
        # ASCII or Latin1.  And it omits the fullnames of languages whose
        # scripts are themselves.  The grep below catches those.  Defaulting
        # to Latin means that a non-standard language name is considered to be
        # latin -- maybe not the best outcome but what else is better?
        $ret{script} = $lang_code_to_script{$ret{language}};
        if (! $ret{script}) {
            $ret{script} = (grep { $ret{language} eq $_ }
                                                    values %lang_code_to_script)
                            ? $ret{language}
                            : 'latin';
        }
    }

    # If we have @euro, and the script is ASCII or latin or latin1, change it
    # into latin9, which is closer to what is going on.  latin9 has a few
    # other differences from latin1, but it's not worth creating a whole new
    # script type that differs only in the currency symbol.
    if (  ($ret{modifier} && $ret{modifier} eq 'euro')
        && $ret{script} =~ / ^ ($official_ascii_name | latin (_1)? ) $ /x)
    {
        $ret{script} = 'latin_9';
    }

    #  Look up the priority of this script.  All the non-listed ones have
    #  highest (0 or 1) priority.  We arbitrarily make the ones higher
    #  priority (0) that aren't known to be half-ascii, simply because they
    #  might be entirely different than most locales.
    $ret{priority} = $script_priorities{$ret{script}};
    if (! $ret{priority}) {
        $ret{priority} = (   $ret{script} ne $official_ascii_name
                          && $ret{script} =~ $official_ascii_name)
                         ? 0
                         : 1;
    }

    # Script names have been set up so that anything after an underscore is a
    # modifier of the main script.  We keep a counter of which occurence of
    # this script this is.  This is used along with the priority to order the
    # locales so that the characters are as varied as possible.
    my $script_root = ($ret{script} =~ s/_.*//r) . "_$ret{is_utf8}";
    $ret{script_instance} = $script_instances{$script_root}++;

    return \%ret;
}

# Prioritize locales that are most unlike the standard C/Latin1-ish ones.
# This is to minimize getting passes for tests on a category merely because
# they share many of the same characteristics as the locale of another
# category simultaneously in effect.
sub sort_locales ()
{
    my $cmp =  $a->{script_instance} <=> $b->{script_instance};
    return $cmp if $cmp;

    $cmp =  $a->{priority} <=> $b->{priority};
    return $cmp if $cmp;

    $cmp =  $a->{script} cmp $b->{script};
    return $cmp if $cmp;

    $cmp =  $a->{modifier} cmp $b->{modifier};
    return $cmp if $cmp;

    $cmp =  $a->{codeset} cmp $b->{codeset};
    return $cmp if $cmp;

    $cmp =  $a->{territory} cmp $b->{territory};
    return $cmp if $cmp;

    return lc $a cmp lc $b;
}

# Find out extra info about each locale
my @cleaned_up_locales;
for my $locale (@locales) {
    my $locale_struct = analyze_locale_name($locale);

    next unless $locale_struct;

    my $name = $locale_struct->{locale_name};
    next if grep { $name eq $_->{locale_name} } @cleaned_up_locales;

    push @cleaned_up_locales, $locale_struct;
}

@locales = @cleaned_up_locales;

# Without a proper codeset, we can't really know how to test.  This should
# only happen on platforms that lack the ability to determine the codeset.
@locales = grep { $_->{codeset} ne "" } @locales;

# Sort into priority order.
@locales = sort sort_locales @locales;

# First test
SKIP: { # perl #127708
    my $locale = $locales[0];
    skip("No valid locale to test with", 1) if $locale->{codeset} eq
                                                          $official_ascii_name;
    local $ENV{LC_MESSAGES} = $locale->{locale_name};

    # We're going to try with all possible error numbers on this platform
    my $error_count = keys(%!) + 1;

    print fresh_perl("
        use threads;
        use strict;
        use warnings;
        use Time::HiRes qw(usleep);

        my \$errnum = 1;

        my \@threads = map +threads->create(sub {
            usleep 0.1;
            'threads'->yield();

            for (1..5_000) {
                \$errnum = (\$errnum + 1) % $error_count;
                \$! = \$errnum;

                # no-op to trigger stringification
                next if \"\$!\" eq \"\";
            }
        }), (0..1);
        \$_->join for splice \@threads;",
    {}
    );

    pass("Didn't segfault");
}

# Second test setup
my %locale_name_to_object;
for my $locale (@locales) {
    $locale_name_to_object{$locale->{locale_name}} = $locale;
}

sub sort_by_hashed_locale {
    local $a = $locale_name_to_object{$a};
    local $b = $locale_name_to_object{$b};

    return sort_locales;
}

sub min {
    my ($a, $b) = @_;
    return $a if $a <= $b;
    return $b;
}

# Smokes have shown this to be about the maximum numbers some platforms can
# handle.  khw has tried 500 threads/1000 iterations on Linux
my $thread_count = 15;
my $iterations = 100;

my $alarm_clock = (1 * 10 * 60);    # A long time, just to prevent hanging

# Chunk the iterations, so that every so often the test comes up for air.
my $iterations_per_test_set = min(30, int($iterations / 5));
$iterations_per_test_set = 1 if $iterations_per_test_set == 0;

# Sometimes the test calls setlocale() for each individual locale category.
# But every this many threads, it will be called just once, using LC_ALL to
# specify the categories.  This way both setting individual categories and
# LC_ALL get tested.  But skip this nicety on platforms where we are restricted from
# using all the available categories, as it would make the code more complex
# for not that much gain.
my @platform_categories = platform_locale_categories();
my $lc_all_frequency =  scalar @platform_categories == scalar @valid_categories
                        ? 3
                        : -1;

# To avoid things getting too big; skip tests whose results are larger than
# this many characters.
my $max_result_length = 10000;

# Estimate as to how long in seconds to allow a thread to be ready to roll
# after creation, so as to try to get all the threads to start as
# simultaneously as possible
my $per_thread_startup = .18;

# For use in experimentally tuning the above value
my $die_on_negative_sleep = 1;

# We don't need to test every possible errno, but you could change this to do
# so by setting it to negative
my $max_message_catalog_entries = 10;

# December 18, 1987
my $strftime_args = "'%c', 0, 0, , 12, 18, 11, 87";

my %distincts;  # The distinct 'operation => result' cases
my %op_counts;  # So we can bail early if more test cases than threads
my $separator = '____';     # The operation and result are often melded into a
                            # string separated by this.

sub pack_op_result($$) {
    my ($op, $result) = @_;
    return $op . $separator
         . (0 + utf8::is_utf8($op)) . $separator
         . $result . $separator
         . (0 + utf8::is_utf8($result));
}

sub fixup_utf8ness($$) {
    my ($operand, $utf8ness) = @_;

    # Make sure $operand is encoded properly

    if ($utf8ness + 0 != 0 + utf8::is_utf8($$operand)) {
        if ($utf8ness) {
            utf8::upgrade($$operand);
        }
        else {
            utf8::downgrade($$operand);
        }
    }
}

sub unpack_op_result($) {
    my $op_result = shift;

    my ($op, $op_utf8ness, $result, $result_utf8ness) =
                                            split $separator, $op_result;
    fixup_utf8ness(\$op, $op_utf8ness);
    fixup_utf8ness(\$result, $result_utf8ness);

    return ($op, $result);
}

sub add_trials($$;$)
{
    # Add a test case for category $1.
    # $2 is the test case operation to perform
    # $3 is a constraint, optional.

    my $category_name = shift;
    my $input_op = shift;                   # The eval string to perform
    my $locale_constraint = shift // "";    # If defined, the test will be
                                            # created only for locales that
                                            # match this
  LOCALE:
    foreach my $locale (@locales) {
        my $locale_name = $locale->{locale_name};
        my $op = $input_op;

        # All categories should be set to the same locale to make sure
        # this test gets the valid results.
        next unless setlocale($LC_ALL, $locale_name);

        # As of NetBSD 10, it doesn't implement LC_COLLATE, and setting that
        # category to anything but C or POSIX fails.  But setting LC_ALL to
        # other locales (as we just did) returns success, while leaving
        # LC_COLLATE untouched.  Therefore, also set the category individually
        # to catch such things.  This problem may not be confined to NetBSD.
        # This also works if the platform lacks LC_ALL.  We at least set
        # LC_CTYPE (via '$LC_ALL' above) besides the category.
        next unless setlocale($map_category_name_to_number{$category_name},
                              $locale_name);

        # Use a placeholder if this test requires a particular constraint,
        # which isn't met in this case.
        if ($locale_constraint) {
            if ($locale_constraint eq 'utf8_only') {
                next if ! $locale->{is_utf8};
            }
            elsif ($locale_constraint eq 'a<b') {
                my $result = eval "use locale; 'a' lt 'B'";
                die "$category_name: '$op (a lt B)': $@" if $@;
                next unless $result;
            }
            else {
                die "Only accepted locale constraints are 'utf8_only' and 'a<b'"
            }
        }

        # Calculate what the expected value of the test should be.  We're
        # doing this here in the main thread and with all the locales set to
        # be the same thing.  The test will be that we should get this value
        # under stress, with each thread using different locales for each
        # category, and multiple threads simultaneously executing with
        # disparate locales
        my $eval_string = ($op) ? "use locale; $op;" : "";
        my $result = eval $eval_string;
        die "$category_name: '$op': $@" if $@;
        if (! defined $result) {
            if ($debug) {
                print STDERR __FILE__, ": ", __LINE__,
                             ": Undefined result for $locale_name",
                             " $category_name: '$op'\n";
            }
            next;
        }
        elsif ($debug > 1) {
            print STDERR "\n", __FILE__, ": ", __LINE__, ": $category_name:",
                         " $locale_name: Op = ", Dumper($op), "; Returned ";
            Dump $result;
        }
        if (length $result > $max_result_length) {
            diag("For $locale_name, '$op', result is too long; skipped");
            next;
        }

        # It seems best to not include tests with mojibake results, which here
        # is checked for by two question marks in a row.  (strxfrm is excluded
        # from this restriction, as the result is really binary, so '??' could
        # and does come up, not meaning mojibake.)  A concrete example of this
        # is in Mingw the locale Yi_China.1252.  CP 1252 is for a Latin
        # script; just about anything from an East Asian script is bound to
        # fail.  It makes no sense to have this locale, but it exists.
        if ($eval_string !~ /xfrm/ && $result =~ /\?\?/) {
            if ($debug)  {
                print STDERR __FILE__, ": ", __LINE__,
                  " For $locale_name, op=$op, result has mojibake: $result\n";
            }

            next;
        }

        # Some systems are buggy in that setlocale() gives non-deterministic
        # results for some locales.   Here we try to exclude those from our
        # test by trying the setlocale this many times to see if it varies:
        my $deterministic_trial_count = 5;

        # To do this, we set the locale to an 'alternate' locale between
        # trials.  This defeats any attempt by the implementation to skip the
        # setlocale if it is already in said locale.
        my $alternate;
        my @alternate;

        # If possible, the alternate is chosen to be of the opposite UTF8ness,
        # so as to reset internal states about that.
        if (! $utf8_locales_ref || ! $utf8_locales_ref->@*) {

            # If no UTF-8 locales, must choose one that is non-UTF-8.
            @alternate = grep { $_ ne $locale_name } $non_utf8_locales_ref->@*;
        }
        elsif (! $non_utf8_locales_ref || ! $non_utf8_locales_ref->@*) {

            # If no non-UTF-8 locales, must choose one that is UTF-8.
            @alternate = grep { $_ ne $locale_name } $utf8_locales_ref->@*;
        }
        elsif (grep { $_ eq $locale_name } $utf8_locales_ref->@*) {
            @alternate = $non_utf8_locales_ref->@*;
        }
        else {
            @alternate = $utf8_locales_ref->@*;
        }

        # Now do the trials.  For each, we choose the next alternate on the
        # list, rotating the list so the following iteration will choose a
        # different alternate.
        for my $i (1 .. $deterministic_trial_count - 1) {
            my $other = shift @alternate;
            push @alternate, $other;

            # Run the test on the alternate locale
            if (! setlocale($LC_ALL, $other)) {
                if (   $LC_ALL_string eq 'LC_ALL'
                    || ! setlocale($map_category_name_to_number{$category_name},
                                   $other))
                {
                    die "Unexpectedly can't set locale to $other:"
                      . " \$!=$!, \$^E=$^E";
                }
            }

            eval $eval_string;

            # Then run it on the one we are hoping to test
            if (! setlocale($LC_ALL, $locale_name)) {
                if (   $LC_ALL_string eq 'LC_ALL'
                    || ! setlocale($map_category_name_to_number{$category_name},
                                   $locale_name))
                {
                    die "Unexpectedly can't set locale to $locale_name from "
                      . setlocale($LC_ALL)
                      . "; \$!=$!, \$^E=$^E";
                }
            }

            my $got = eval $eval_string;
            next if $got eq $result
                 && utf8::is_utf8($got) == utf8::is_utf8($result);

            # If the result varied from the expected value, this is a
            # non-deterministic locale, so, don't test it.
            diag("For '$eval_string',\nresults in iteration $i differed from"
               . " the original\ngot");
            Dump($got);
            diag("expected");
            Dump($result);
            next LOCALE;
        }

        # Here, the setlocale for this locale appears deterministic.  Use it.
        my $op_result = pack_op_result($op, $result);
        push $distincts{$category_name}{$op_result}{locales}->@*, $locale_name;
        # No point in looking beyond this if we already have all the tests we
        # need.  Note this assumes that the same op isn't used in two
        # categories.
        if (defined $op_counts{$op} && $op_counts{$op} >= $thread_count)
        {
            last;
        }
    }
}

use Config;

# Figure out from config how to represent disparate LC_ALL
my @valid_category_numbers = sort { $a <=> $b }
                    map { $map_category_name_to_number{$_} } @valid_categories;

my $use_name_value_pairs = defined $Config{d_perl_lc_all_uses_name_value_pairs};
my $lc_all_separator = ($use_name_value_pairs)
                       ? ";"
                       : $Config{perl_lc_all_separator} =~ s/"//gr;
my @position_to_category_number;
if (! $use_name_value_pairs) {
    my $positions = $Config{perl_lc_all_category_positions_init} =~ s/[{}]//gr;
    $positions =~ s/,//g;
    $positions =~ s/^ +//;
    $positions =~ s/ +$//;
    @position_to_category_number = split / \s+ /x, $positions
}

sub get_next_category() {
    use feature 'state';
    state $index;

    # Called to rotate all the legal locale categories

    my $which = ($use_name_value_pairs)
                ? \@valid_category_numbers
                : \@position_to_category_number;

    $index = -1 unless defined $index;
    $index++;

    if (! defined $which->[$index]) {
        undef $index;
        return;
    }

    my $category_number = $which->[$index];
    return $category_number if $category_number != $LC_ALL;

    # If this was LC_ALL, the next one won't be
    return &get_next_category();
}

SKIP: {
    skip("Unsafe locale threads", 1) unless ${^SAFE_LOCALES};

    # The second test is several threads nearly simulataneously executing
    # locale-sensitive operations with the categories set to disparate
    # locales.  This catches cases where the results of a given category is
    # related to what the locale is of another category.  (As an example, this
    # test showed that some platforms require LC_CTYPE to be the same as
    # LC_COLLATION, and/or LC_MESSAGES for proper results, so that Perl had to
    # change to bring these into congruence under the hood).  And it also
    # catches where there is interference between multiple threads.
    #
    # This test tries to exercise every underlying locale-dependent operation
    # available in Perl.  It doesn't test every use of the operation, but
    # includes some Perl construct that uses each.  For example, it tests lc
    # but not lcfirst.  That would be redundant for this test; it wants to
    # know if lowercasing works under threads and locales.  But if the
    # implementations were disjoint at the time this test was written, it
    # would try each implementation.  So, various things in the POSIX module
    # have separate tests from the ones in core.
    #
    # For each such underlying locale-dependent operation, a Perl-visible
    # construct is chosen that uses it.  And a typical input or set of inputs
    # is passed to that and the results are noted for every available locale
    # on the platform.  Many locales will have identical results, so the
    # duplicates are stored separately.
    #
    # There will be N simultaneous threads.  Each thread is configured to set
    # a locale for each category, to run operations whose results depend on
    # that locale, then check that the result matches the expected value, and
    # to immediately repeat some largish number of iterations.  The goal is to
    # see if the locales on each thread are truly independent of those on the
    # other threads.
    #
    # To that end, the locales are chosen so that the results differ from
    # every other locale.  Otherwise, the thread results wouldn't be truly
    # independent.  But if there are more threads than there are distinct
    # results, duplicates are used to fill up what would otherwise be empty
    # slots.  That is the best we can do on those platforms.
    #
    # Having lots of locales to continually switch between stresses things so
    # as to find potential segfaults where locale changing isn't really thread
    # safe.

    # There is a bug in older Windows runtimes in which locales in CP1252 and
    # similar code pages whose names aren't entirely ASCII aren't recognized
    # by later setlocales.  Some names that are all ASCII are synonyms for
    # such names.  Weed those out by doing a setlocale of the original name,
    # and then a setlocale of the resulting one.  Discard locales which have
    # any unacceptable name
    if (${^O} eq "MSWin32" && $Config{'libc'} !~ /ucrt/) {
        @locales = grep {
            my $locale_name = $_->{locale_name};
            my $underlying_name = setlocale(&LC_CTYPE, $locale_name);

            # Defeat any attempt to skip the setlocale if the same as current,
            # by switching to a locale very unlikey to be the current one.
            setlocale($LC_ALL, "Albanian");

            defined($underlying_name) && setlocale(&LC_CTYPE, $underlying_name)
        } @locales;
    }

    # Create a hash of the errnos:
    #          "1" => "Operation\\ not\\ permitted",
    #          "2" => "No\\ such\\ file\\ or\\ directory",
    #          etc.
    my %msg_catalog;
    foreach my $error (sort keys %!) {
        my $number = eval "Errno::$error";
        $! = $number;
        my $description = "$!";
        next unless "$description";
        $msg_catalog{$number} = quotemeta "$description";
    }

    # Then just the errnos.
    my @msg_catalog = sort { $a <=> $b } keys %msg_catalog;

    # Remove the excess ones.
    splice @msg_catalog, $max_message_catalog_entries
                                          if $max_message_catalog_entries >= 0;
    my $msg_catalog = join ',', @msg_catalog;

    eval  { my $discard = POSIX::localeconv()->{currency_symbol}; };
    my $has_localeconv = $@ eq "";

    # Now go through and create tests for each locale category on the system.
    # These tests were determined by grepping through the code base for
    # locale-sensitive operations, and then figuring out something to exercise
    # them.
    foreach my $category (@valid_categories) {
        no warnings 'uninitialized';

        next if $category eq 'LC_ALL';  # Tested below as a combination of the
                                        # individual categories
        if ($category eq 'LC_COLLATE') {
            add_trials('LC_COLLATE',
                       # 'reverse' causes it to be definitely out of order for
                       # the 'sort' to correct
                       'quotemeta join "", sort reverse map { chr } (1..255)');

            # We pass an re to exclude testing locales that don't necessarily
            # have a lt b.
            add_trials('LC_COLLATE', '"a" lt "B"', 'a<b');
            add_trials('LC_COLLATE', 'my $a = "a"; my $b = "B";'
                                   . ' POSIX::strcoll($a, $b) < 0;',
                        'a<b');

            # Doesn't include NUL because our memcollxfrm implementation of it
            # isn't perfect
            add_trials('LC_COLLATE', 'my $string = quotemeta join "",'
                                   . ' map { chr } (1..255);'
                                   . ' POSIX::strxfrm($string)');
            next;
        }

        if ($category eq 'LC_CTYPE') {
            add_trials('LC_CTYPE', 'no warnings "locale"; quotemeta lc'
                                 . ' join "" , map { chr } (0..255)');
            add_trials('LC_CTYPE', 'no warnings "locale"; quotemeta uc'
                                 . ' join "", map { chr } (0..255)');
            add_trials('LC_CTYPE', 'no warnings "locale"; quotemeta CORE::fc'
                                 . ' join "", map { chr } (0..255)');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                                 . ' my $string = join "", map { chr } 0..255;'
                                 . ' $string =~ s|(.)|$1=~/\d/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                                 . ' my $string = join "", map { chr } 0..255;'
                                 . ' $string =~ s|(.)|$1=~/\s/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                                 . ' my $string = join "", map { chr } 0..255;'
                                 . ' $string =~ s|(.)|$1=~/\w/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:alpha:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:alnum:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:ascii:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:blank:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:cntrl:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:graph:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:lower:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:print:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:punct:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                              . ' my $string = join "", map { chr } 0..255;'
                              . ' $string =~ s|(.)|$1=~/[[:upper:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'no warnings "locale";'
                             . ' my $string = join "", map { chr } 0..255;'
                             . ' $string =~ s|(.)|$1=~/[[:xdigit:]]/?1:0|gers');
            add_trials('LC_CTYPE', 'use I18N::Langinfo qw(langinfo CODESET);'
                                 . ' no warnings "uninitialized";'
                                 . ' langinfo(CODESET);');

            # In the multibyte functions, the non-reentrant ones can't be made
            # thread safe
            if ($Config{'d_mbrlen'} eq 'define') {
                add_trials('LC_CTYPE', 'my $string = chr 0x100;'
                                     . ' utf8::encode($string);'
                                     . ' no warnings "uninitialized";'
                                     . ' POSIX::mblen(undef);'
                                     . ' POSIX::mblen($string)',
                           'utf8_only');
            }
            if ($Config{'d_mbrtowc'} eq 'define') {
                add_trials('LC_CTYPE', 'my $value; my $str = "\x{100}";'
                                     . ' utf8::encode($str);'
                                     . ' no warnings "uninitialized";'
                                     . ' POSIX::mbtowc(undef, undef);'
                                     . ' POSIX::mbtowc($value, $str); $value;',
                           'utf8_only');
            }
            if ($Config{'d_wcrtomb'} eq 'define') {
                add_trials('LC_CTYPE', 'my $value;'
                                     . ' no warnings "uninitialized";'
                                     . ' POSIX::wctomb(undef, undef);'
                                     . ' POSIX::wctomb($value, 0xFF);'
                                     . ' $value;',
                           'utf8_only');
            }

            add_trials('LC_CTYPE',
                       'no warnings "locale";'
                     . ' my $uc = CORE::uc join "", map { chr } (0..255);'
                     . ' my $fc = quotemeta CORE::fc $uc;'
                     . ' $uc =~ / \A $fc \z /xi;');
            next;
        }

        if ($category eq 'LC_MESSAGES') {
            add_trials('LC_MESSAGES',
                     "join \"\n\", map { \$! = \$_; \"\$!\" } ($msg_catalog)");
            add_trials('LC_MESSAGES',
                  'use I18N::Langinfo qw(langinfo YESSTR NOSTR YESEXPR NOEXPR);'
                . ' no warnings "uninitialized";'
                . ' join ",",'
                . '     map { langinfo($_) } YESSTR, NOSTR, YESEXPR, NOEXPR;');
            next;
        }

        if ($category eq 'LC_MONETARY') {
            if ($has_localeconv) {
                add_trials('LC_MONETARY', "localeconv()->{currency_symbol}");
            }
            add_trials('LC_MONETARY',
                       'use I18N::Langinfo qw(langinfo CRNCYSTR);'
                    . ' no warnings "uninitialized";'
                    . ' join "|",  map { langinfo($_) } CRNCYSTR;');
            next;
        }

        if ($category eq 'LC_NUMERIC') {
            if ($has_localeconv) {
                add_trials('LC_NUMERIC', "no warnings; 'uninitialised';"
                                       . " join '|',"
                                       . " localeconv()->{decimal_point},"
                                       . " localeconv()->{thousands_sep}");
            }
            add_trials('LC_NUMERIC',
                       'use I18N::Langinfo qw(langinfo RADIXCHAR THOUSEP);'
                     . ' no warnings "uninitialized";'
                     . ' join "|",  map { langinfo($_) } RADIXCHAR, THOUSEP;');

            # Use a variable to avoid runtime bugs being hidden by constant
            # folding
            add_trials('LC_NUMERIC', 'my $in = 4.2; sprintf("%g", $in)');
            next;
        }

        if ($category eq 'LC_TIME') {
            add_trials('LC_TIME', "POSIX::strftime($strftime_args)");
            add_trials('LC_TIME', <<~'END_OF_CODE');
                use I18N::Langinfo qw(langinfo
                    ABDAY_1 ABDAY_2 ABDAY_3 ABDAY_4 ABDAY_5 ABDAY_6 ABDAY_7
                    ABMON_1 ABMON_2 ABMON_3 ABMON_4 ABMON_5 ABMON_6
                    ABMON_7 ABMON_8 ABMON_9 ABMON_10 ABMON_11 ABMON_12
                    DAY_1 DAY_2 DAY_3 DAY_4 DAY_5 DAY_6 DAY_7
                    MON_1 MON_2 MON_3 MON_4 MON_5 MON_6
                    MON_7 MON_8 MON_9 MON_10 MON_11 MON_12
                    D_FMT D_T_FMT T_FMT);
                no warnings "uninitialized";
                join "|",
                    map { langinfo($_) }
                        ABDAY_1,ABDAY_2,ABDAY_3,ABDAY_4,ABDAY_5,
                        ABDAY_6,ABDAY_7,
                        ABMON_1,ABMON_2,ABMON_3,ABMON_4,ABMON_5,
                        ABMON_6, ABMON_7,ABMON_8,ABMON_9,ABMON_10,
                        ABMON_11,ABMON_12,
                        DAY_1,DAY_2,DAY_3,DAY_4,DAY_5,DAY_6,DAY_7,
                        MON_1,MON_2,MON_3,MON_4,MON_5,MON_6, MON_7,
                        MON_8,MON_9,MON_10,MON_11,MON_12,
                        D_FMT,D_T_FMT,T_FMT;
                END_OF_CODE
            next;
        }
    } # End of creating test cases.


    # Now analyze the test cases
    my %all_tests;
    foreach my $category (keys %distincts) {
        my %results;
        my %distinct_results_count;

        # Find just the distinct test operations; sort for repeatibility
        my %distinct_ops;
        for my $op_result (sort keys $distincts{$category}->%*) {
            my ($op, $result) = unpack_op_result($op_result);

            $distinct_ops{$op}++;
            push $results{$op}->@*, $result;
            $distinct_results_count{$result} +=
                        scalar $distincts{$category}{$op_result}{locales}->@*;
        }

        # And get a sorted list of all the test operations
        my @ops = sort keys %distinct_ops;

        sub gen_combinations {

            # Generate all the non-empty combinations of operations and
            # results (for the current category) possible on this platform.
            # That is, if a category has N operations, it will generate a list
            # of entries.  Each entry will itself have N elements, one for
            # each operation, and when all the entries are considered
            # together, every possible outcome is represented.

            my $op_ref = shift;         # Reference to list of operations
            my $results_ref = shift;    # Reference to hash; key is operation;
                                        # value is an array of all possible
                                        # outcomes of this operation.
            my $distincts_ref = shift;  # Reference to %distincts of this
                                        # category

            # Get the first operation on the list
            my $op = shift $op_ref->@*;

            # The return starts out as a list of hashes of all possible
            # outcomes for executing 'op'.  Each hash has two keys:
            #   'op_results' is an array of one element: 'op => result',
            #                packed into a string.
            #   'locales'    is an array of all the locales which have the
            #                same result for 'op'
            my @return;
            foreach my $result ($results_ref->{$op}->@*) {
                my $op_result = pack_op_result($op, $result);
                push @return, {
                            op_results => [ $op_result ],
                            locales    => $distincts_ref->{$op_result}{locales},
                          };
            }

            # If this is the final element of the list, we are done.
            return (\@return) unless $op_ref->@*;

            # Otherwise recurse to generate the combinations for the remainder
            # of the list.
            my $recurse_return = &gen_combinations($op_ref,
                                                   $results_ref,
                                                   $distincts_ref);
            # Now we have to generate the combinations of the current item
            # with the ones returned by the recursion.  Each element of the
            # current item is combined with each element of the recursed.
            my @combined;
            foreach my $this (@return) {
                my @this_locales = $this->{locales}->@*;
                foreach my $recursed ($recurse_return->@*) {
                    my @recursed_locales = $recursed->{locales}->@*;

                    # @this_locales is a list of locales this op => result is
                    # valid for.  @recursed_locales is similarly a list of the
                    # valid ones for the recursed return.  Their intersection
                    # is a list of the locales valid for this combination.
                    my %seen;
                    $seen{$_}++ foreach @this_locales, @recursed_locales;
                    my @intersection = grep $seen{$_} == 2, keys %seen;

                    # An alternative intersection algorithm:
                    # my (%set1, %set2);
                    # @set1{@list1} = ();
                    # @set2{@list2} = ();
                    # my @intersection = grep exists $set1{$_}, keys %set2;

                    # If the intersection is empty, this combination can't
                    # actually happen on this platform.
                    next unless @intersection;

                    # Append the recursed list to the current list to form the
                    # combined list.
                    my @combined_result = $this->{op_results}->@*;
                    push @combined_result, $recursed->{op_results}->@*;
                    # And create the hash for the combined result, including
                    # the locales it is valid for
                    push @combined, {
                                      op_results => \@combined_result,
                                      locales    => \@intersection,
                                    };
                }
            }

            return \@combined;
        } # End of gen_combinations() definition

        # The result of calling gen_combinations() will be an array of hashes.
        #
        # The main value in each hash is an array (whose key is 'op_results')
        # containing all the tests for this category for a thread.  If there
        # were N calls to 'add_trial' for this category, there will be 'N'
        # elements in the array.  Each element is a string packed with the
        # operation to eval in a thread and the operation's expected result.
        #
        # The other data structure in each hash is an array with the key
        # 'locales'.  That array is a list of every locale which yields the
        # identical results in 'op_results'.
        #
        # Effectively, each hash gives all the tests for this category for a
        # thread.  The total array of hashes gives the complete list of
        # distinct tests possible on this system.  So later, a thread will
        # pluck the next available one from the array..
        my $combinations_ref = gen_combinations(\@ops, \%results,
                                                $distincts{$category});

        # Fix up the entries ...
        foreach my $test ($combinations_ref->@*) {

            # Sort the locale names; this makes it work for later comparisons
            # to look at just the first element of each list.
            $test->{locales}->@* =
                                sort sort_by_hashed_locale $test->{locales}->@*;

            # And for each test, calculate and store how many locales have the
            # same result (saves recomputation later in a sort).  This adds
            # another data structure to each hash in the main array.
            my @individual_tests = $test->{op_results}->@*;
            my @in_common_locale_counts;
            foreach my $this_test (@individual_tests) {

                # Each test came from %distincts, and there we have stored the
                # list of all locales that yield the same result
                push @in_common_locale_counts,
                        scalar $distincts{$category}{$this_test}{locales}->@*;
            }
            push $test->{in_common_locale_counts}->@*, @in_common_locale_counts;
        }

        # Make a copy
        my @cat_tests = $combinations_ref->@*;

        # This sorts the test cases so that the ones with the least overlap
        # with other cases are first.
        sub sort_test_order {
            my $a_tests_count = scalar $a->{in_common_locale_counts}->@*;
            my $b_tests_count = scalar $b->{in_common_locale_counts}->@*;
            my $tests_count = min($a_tests_count, $b_tests_count);

            # Choose the one that is most distinctive (least overlap); that is
            # the one that has the most tests whose results are not shared by
            # any other locale.
            my $a_nondistincts = 0;
            my $b_nondistincts = 0;
            for my $i (0 .. $tests_count - 1) {
                $a_nondistincts += ($a->{in_common_locale_counts}[$i] != 1);
                $b_nondistincts += ($b->{in_common_locale_counts}[$i] != 1);
            }

            my $cmp = $a_nondistincts <=> $b_nondistincts;
            return $cmp if $cmp;

            # If they have the same number of those, choose the one with the
            # fewest total number of locales that have the same result
            my $a_count = 0;
            my $b_count = 0;
            for my $i (0 .. $tests_count - 1) {
                $a_count += $a->{in_common_locale_counts}[$i];
                $b_count += $b->{in_common_locale_counts}[$i];
            }

            $cmp = $a_count <=> $b_count;
            return $cmp if $cmp;

            # If that still doesn't yield a winner, use the general sort order.
            local $a = $a->{locales}[0];
            local $b = $b->{locales}[0];
            return sort_by_hashed_locale;
        }

        # Actually perform the sort.
        @cat_tests = sort sort_test_order @cat_tests;

        # This category will now have all the distinct tests possible for it
        # on this platform, with the first test being the one with the least
        # overlap with other test cases
        push $all_tests{$category}->@*, @cat_tests;
    }     # End of loop through the categories creating and sorting the test
          # cases

    my %thread_already_used_locales;

    # Now generate the tests for each thread.
    my @tests_by_thread;
    for my $i (0 .. $thread_count - 1) {
        foreach my $category (sort keys %all_tests) {
            my $skipped = 0;    # Used below to not loop infinitely

            # Get the next test case
          NEXT_CANDIDATE:
            my $candidate = shift $all_tests{$category}->@*;

            my $locale_name = $candidate->{locales}[0];

            # Avoid, if possible, using the same locale name twice (for
            # different categories) in the same thread.
            if (defined $thread_already_used_locales{$locale_name =~ s/\W.*//r})
            {
                # Look through the synonyms of this locale for an
                # as-yet-unused one
                for my $j (1 .. $candidate->{locales}->@* - 1) {
                    my $synonym = $candidate->{locales}[$j];
                    next if defined $thread_already_used_locales{$synonym =~
                                                                    s/\W.*//r};
                    $locale_name = $synonym;
                    goto found_synonym;
                }

                # Here, no synonym was found.  If we haven't cycled through
                # all the possible tests, try another (putting this one at the
                # end as a last resort in the future).
                $skipped++;
                if ($skipped < scalar $all_tests{$category}->@*) {
                    push $all_tests{$category}->@*, $candidate;
                    goto NEXT_CANDIDATE;
                }

                # Here no synonym was found, this test has already been used,
                # but there are no unused ones, so have to re-use it.

              found_synonym:
            }

            # Here, we have found a test case.  The thread needs to know what
            # locale to use,
            $tests_by_thread[$i]->{$category}{locale_name} = $locale_name;

            # And it needs to know each test to run, and the expected result.
            my @cases;
            for my $j (0 .. $candidate->{op_results}->@* - 1) {
                my ($op, $result) =
                             unpack_op_result($candidate->{op_results}[$j]);
                push @cases, { op => $op, expected => $result };
            }
            push $tests_by_thread[$i]->{$category}{locale_tests}->@*, @cases;

            # Done with this category in this thread.  Setup for subsequent
            # categories in this thread, and subsequent threads.
            #
            # It's best to not have two categories in a thread use the same
            # locale.  Save this locale name so that later iterations handling
            # other categories can avoid using it, if possible.
            $thread_already_used_locales{$locale_name =~ s/\W.*//r} = 1;

            # In pursuit of using as many different locales as possible, the
            # first shall be last in line next time, and eventually the last
            # shall be first
            push $candidate->{locales}->@*, shift $candidate->{locales}->@*;

            # Similarly, this test case is added back at the end of the list,
            # so will be used only as a last resort in the next thread, and as
            # the penultimate resort in the thread following that, etc. as the
            # test cases are cycled through.
            push $all_tests{$category}->@*, $candidate;
        } # End of looping through the categories for this thread
    } # End of generating all threads

    # Now reformat the tests to a form convenient for the actual test file
    # script to use; minimizing the amount of ancillary work it needs to do.
    my @cooked_tests;
    for my $i (0 .. $#tests_by_thread) {

        my $this_tests = $tests_by_thread[$i];
        my @this_cooked_tests;
        my (@this_categories, @this_locales);    # Parallel arrays

        # Every so often we use LC_ALL instead of individual locales, provided
        # it is available on the platform
        if (   ($i % $lc_all_frequency == $lc_all_frequency - 1)
            && $LC_ALL_string eq 'LC_ALL')
        {
            my $lc_all= "";
            my $category_number;

            # Compute the LC_ALL string for the syntax accepted by this
            # platform from the locale each category is to be set to.
            while (defined($category_number = get_next_category())) {
                my $category_name =
                                $map_category_number_to_name{$category_number};
                my $locale = $this_tests->{$category_name}{locale_name};
                $locale = "C" unless defined $locale;
                $category_name =~ s/\@/\\@/g;

                $lc_all .= $lc_all_separator if $lc_all ne "";

                if ($use_name_value_pairs) {
                    $lc_all .= $category_name . "=";
                }

                $lc_all .= $locale;
            }

            $this_categories[0] = $LC_ALL;
            $this_locales[0] = $lc_all;
        }
        else {  # The other times, just set each category to its locale
                # individually
            foreach my $category_name (sort keys $this_tests->%*) {
                push @this_categories,
                                $map_category_name_to_number{$category_name};
                push @this_locales,
                            $this_tests->{$category_name}{locale_name};
            }
        }

        while (keys $this_tests->%*) {
            foreach my $category_name (sort keys $this_tests->%*) {
                my $this_category_tests = $this_tests->{$category_name};
                my $test = shift
                                $this_category_tests->{locale_tests}->@*;
                print STDERR __FILE__, ': ', __LINE__, ': ', Dumper $test
                                                                    if $debug;
                if (! $test) {
                    delete $this_tests->{$category_name};
                    next;
                }

                $test->{category_name} = $category_name;
                my $locale_name = $this_category_tests->{locale_name};
                $test->{locale_name} = $locale_name;
                $test->{codeset} =
                                $locale_name_to_object{$locale_name}{codeset};

                push @this_cooked_tests, $test;
            }
        }

        push @cooked_tests, {
                              thread => $i,
                              categories => \@this_categories,
                              locales => \@this_locales,
                              tests => \@this_cooked_tests,
                            };
    }

    my $all_tests_ref = \@cooked_tests;
    my $all_tests_file = tempfile();

    # Store the tests into a file, retrievable by the subprocess
    use Storable;
    if (! defined store($all_tests_ref, $all_tests_file)) {
        die "Could not save the built-up data structure";
    }

    my $category_number_to_name = Data::Dumper->Dump(
                                            [ \%map_category_number_to_name ],
                                            [  'map_category_number_to_name']);

    my $switches = "";
    $switches = "switches => [ -DLv ]" if $debug > 2;

    # Build up the program to run.  This stresses locale thread safety.  We
    # start a bunch of threads.  Each sets the locale of each category being
    # tested to the value determined in the code above.  Then each sleeps to a
    # common start time, at which point they awaken and iterate their
    # respective loops.  Each iteration runs a set of tests and checks that
    # the results are as expected.  This should catch any instances of other
    # threads interfering.  Every so often, each thread shifts to instead use
    # the locales and tests of another thread.  This catches bugs dealing with
    # changing the locale on the fly.
    #
    # The code above has set up things so that each thread has as disparate
    # results from the other threads as possible, so to more likely catch any
    # bleed-through.
    my $program = <<EOT;

    BEGIN { \$| = 1; }
    my \$debug = $debug;
    my \$thread_count = $thread_count;
    my \$iterations_per_test_set = $iterations_per_test_set;
    my \$iterations = $iterations;
    my \$die_on_negative_sleep = $die_on_negative_sleep;
    my \$per_thread_startup = $per_thread_startup;
    my \$all_tests_file = $all_tests_file;
    my \$alarm_clock = $alarm_clock;
EOT

    $program .= <<'EOT';
    use threads;
    use strict;
    use warnings;
    use POSIX qw(locale_h);
    use utf8;
    use Time::HiRes qw(time usleep);
    $|=1;

    use Data::Dumper;
    $Data::Dumper::Sortkeys=1;
    $Data::Dumper::Useqq = 1;
    $Data::Dumper::Deepcopy = 1;

    # Get the tests stored for us by the setup process
    use Storable;
    my $all_tests_ref = retrieve($all_tests_file);
    if (! defined $all_tests_ref) {
        die "Could not restore the built-up data structure";
    }

    my %corrects;

    sub output_test_failure_prefix {
        my ($iteration, $category_name, $test) = @_;
        my $tid = threads->tid();
        print STDERR "\nthread ", $tid,
                     " failed in iteration $iteration",
                     " for locale $test->{locale_name}",
                     " codeset='$test->{codeset}'",
                     " $category_name",
                     "\nop='$test->{op}'",
                     "\nafter getting ", ($corrects{$category_name}
                                          {$test->{locale_name}}
                                          {all} // 0),
                     " previous correct results for this category and",
                     " locale,\nincluding ", ($corrects{$category_name}
                                              {$test->{locale_name}}
                                              {$tid} // 0),
                     " in this thread\n";
    }

    sub output_test_result($$$) {
        my ($type, $result, $utf8_matches) = @_;

        no locale;

        print STDERR "$type";

        my $copy = $result;
        if (! $utf8_matches) {
            if (utf8::is_utf8($copy)) {
                print STDERR " (result already was in UTF-8)";
            }
            else {
                utf8::upgrade($copy);
                print STDERR " (result wasn't in UTF-8; converted for easier",
                             " comparison)";
            }
        }
        print STDERR ":\n";

        use Devel::Peek;
        Dump $copy;
    }

    sub iterate {       # Run some chunk of iterations of the tests
        my ($tid,                  # Which thread
            $initial_iteration,    # The number of the first iteration
            $count,                # How many
            $tests_ref)            # The tests
            = @_;

        my $iteration = $initial_iteration;
        $count += $initial_iteration;

        # Repeatedly ...
        while ($iteration < $count) {
            my $errors = 0;

            use locale;

            # ... execute the tests
            foreach my $test ($tests_ref->@*) {

                # We know what we are expecting
                my $expected = $test->{expected};

                my $category_name = $test->{category_name};

                # And do the test.
                my $got = eval $test->{op};

                if (! defined $got) {
                    output_test_failure_prefix($iteration,
                                               $category_name,
                                               $test);
                    output_test_result("expected", $expected,
                                        1 # utf8ness matches, since only one
                                      );
                    $errors++;
                    next;
                }

                my $utf8ness_matches = (   utf8::is_utf8($got)
                                        == utf8::is_utf8($expected));

                my $matched = ($got eq $expected);
                if ($matched) {
                    if ($utf8ness_matches) {
                        no warnings 'uninitialized';
                        $corrects{$category_name}{$test->{locale_name}}{all}++;
                        $corrects{$category_name}{$test->{locale_name}}{$tid}++;
                        next;   # Complete success!
                    }
                }

                $errors++;
                output_test_failure_prefix($iteration, $category_name, $test);

                if ($matched) {
                    print STDERR "Only difference is UTF8ness of results\n";
                }
                output_test_result("expected", $expected, $utf8ness_matches);
                output_test_result("got", $got, $utf8ness_matches);

            } # Loop to do the remaining tests for this iteration

            return 0 if $errors;

            $iteration++;

            # A way to set a gdb break point pp_study
            #study if $iteration % 10 == 0;

            threads->yield();
        }

        return 1;
    } # End of iterate() definition

EOT

    $program .= "my $category_number_to_name\n";

    $program .= <<'EOT';
    sub setlocales {
        # Set each category to the appropriate locale for this test set
        my ($categories, $locales) = @_;
        for my $i (0 .. $categories->@* - 1) {
            if (! setlocale($categories->[$i], $locales->[$i])) {
                my $category_name =
                            $map_category_number_to_name->{$categories->[$i]};
                print STDERR "\nthread ", threads->tid(),
                             " setlocale($category_name ($categories->[$i]),",
                             " $locales->[$i]) failed\n";
                return 0;
            }
        }

        return 1;
    }

    my $startup_insurance = 1;
    my $future = $startup_insurance + $thread_count * $per_thread_startup;
    my $starting_time = time() + $future;

    sub wait_until_time {

        # Sleep until the time when all the threads are due to wake up, so
        # they run as simultaneously as we can make it.
        my $sleep_time = ($starting_time - time());
        #printf STDERR "thread %d started, sleeping %g sec\n",
        #              threads->tid, $sleep_time;
        if ($sleep_time < 0 && $die_on_negative_sleep) {
            # What the start time should have been
            my $a_better_future = $future - $sleep_time;

            my $better_per_thread =
                        ($a_better_future - $startup_insurance) / $thread_count;
            printf STDERR "$per_thread_startup would need to be %g",
                          " for thread %d to have started\nin sync with",
                          " the other threads\n",
                          $better_per_thread, threads->tid;
            die "Thread started too late";
        }
        else {
            usleep($sleep_time * 1_000_000) if $sleep_time > 0;
        }
    }

    # Create all the subthreads: 1..n
    my @threads = map +threads->create(sub {
        $SIG{'KILL'} = sub { threads->exit(); };

        my $thread = shift;

        # Start out with the set of tests whose number is the same as the
        # thread number
        my $test_set = $thread;

        wait_until_time();

        # Loop through all the iterations for this thread
        my $this_iteration_start = 1;
        do {
             # Set up each category with its locale;
            my $this_ref = $all_tests_ref->[$test_set];
            return 0 unless setlocales($this_ref->{categories},
                                       $this_ref->{locales});
            # Then run one batch of iterations
            my $result = iterate($thread,
                                 $this_iteration_start,
                                 $iterations_per_test_set,
                                 $this_ref->{tests});
            return 0 if $result == 0;   # Quit if failed

            # Next iteration will shift to use a different set of locales for
            # each category
            $test_set++;
            $test_set = 0 if $test_set >= $thread_count;
            $this_iteration_start += $iterations_per_test_set;
        } while ($this_iteration_start <= $iterations);

        return 1;   # Success

    }, $_), (1..$thread_count - 1);     # For each non-0 thread

    # Here is thread 0.  We do a smaller chunk of iterations in it; then
    # join whatever threads have finished so far, then do another chunk.
    # This tests for bugs that arise as a result of joining.

    my %thread0_corrects = ();
    my $this_iteration_start = 1;
    my $result = 1;    # So far, everything is ok
    my $test_set = -1;  # Start with 0th test set

    wait_until_time();
    alarm($alarm_clock);    # Guard against hangs

    do {
        # Next time, we'll use the next test set
        $test_set++;
        $test_set = 0 if $test_set >= $thread_count;

        my $this_ref = $all_tests_ref->[$test_set];

        # set the locales for this test set.  Do this even if we
        # are going to bail, so that it will be set correctly for the final
        # batch after the loop.
        $result &= setlocales($this_ref->{categories}, $this_ref->{locales});

        if ($debug > 1) {
            my @joinable = threads->list(threads::joinable);
            if (@joinable) {
                print STDERR "In thread 0, before iteration ",
                             $this_iteration_start,
                             " these threads are done: ",
                             join (", ", map { $_->tid() } @joinable),
                             "\n";
            }
        }

        # Join anything already finished.
        for my $thread (threads->list(threads::joinable)) {
            my $thread_result = $thread->join;
            if ($debug > 1) {
                print STDERR "In thread 0, before iteration ",
                             $this_iteration_start,
                             " joining thread ", $thread->tid(),
                             "; result=", ((defined $thread_result)
                                           ? $thread_result
                                           : "undef"),
                             "\n";
            }

            # If the thread failed badly, stop testing anything else.
            if (! defined $thread_result) {
                $_->kill('KILL')->detach() for threads->list();
                print 0;
                exit;
            }

            # Update the status
            $result &= $thread_result;
        }

        # Do a chunk of iterations on this thread 0.
        $result &= iterate(0,
                           $this_iteration_start,
                           $iterations_per_test_set,
                           $this_ref->{tests},
                           \%thread0_corrects);
        $this_iteration_start += $iterations_per_test_set;

        # And repeat as long as there are other tests
    } while (threads->list(threads::all));

    print $result;
EOT

    # Finally ready to run the test.
    fresh_perl_is($program,
        1,
        { eval $switches },
        "Verify there were no failures with simultaneous running threads"
    );
}
