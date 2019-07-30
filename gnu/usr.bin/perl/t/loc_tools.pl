# Common tools for test files files to find the locales which exist on the
# system.  Caller should have defined ok() for the unlikely event that setup
# here fails, and should have verified that this isn't miniperl before calling
# the functions.

# Note that it's okay that some languages have their native names
# capitalized here even though that's not "right".  They are lowercased
# anyway later during the scanning process (and besides, some clueless
# vendor might have them capitalized erroneously anyway).

# Functions whose names begin with underscore are internal helper functions
# for this file, and are not to be used by outside callers.

eval { require POSIX; import POSIX 'locale_h'; };
$has_locale_h = ! $@;

sub _trylocale ($$$$) { # For use only by other functions in this file!

    # Adds the locale given by the first parameter to the list given by the
    # 3rd iff the platform supports the locale in each of the categories given
    # by the 2nd parameter, which is either a single category or a reference
    # to a list of categories The 4th parameter is true if to reject locales
    # that aren't apparently fully compatible with Perl.

    my $locale = shift;
    my $categories = shift;
    my $list = shift;
    my $only_plays_well = shift;

    return if ! $locale || grep { $locale eq $_ } @$list;

    $categories = [ $categories ] unless ref $categories;

    my $badutf8 = 0;
    my $plays_well = 1;

    use warnings 'locale';

    local $SIG{__WARN__} = sub {
        $badutf8 = 1 if $_[0] =~ /Malformed UTF-8/;
        $plays_well = 0 if $_[0] =~ /Locale .* may not work well/i
    };

    foreach my $category (@$categories) {
        return unless setlocale($category, $locale);
        return if $only_plays_well && ! $plays_well;
    }

    if ($badutf8) {
        ok(0, "Verify locale name doesn't contain malformed utf8");
        return;
    }
    push @$list, $locale;
}

sub _decode_encodings { # For use only by other functions in this file!
    my @enc;

    foreach (split(/ /, shift)) {
	if (/^(\d+)$/) {
	    push @enc, "ISO8859-$1";
	    push @enc, "iso8859$1";	# HP
	    if ($1 eq '1') {
		 push @enc, "roman8";	# HP
	    }
	    push @enc, $_;
            push @enc, "$_.UTF-8";
            push @enc, "$_.65001"; # Windows UTF-8
            push @enc, "$_.ACP"; # Windows ANSI code page
            push @enc, "$_.OCP"; # Windows OEM code page
            push @enc, "$_.1252"; # Windows
	}
    }
    if ($^O eq 'os390') {
	push @enc, qw(IBM-037 IBM-819 IBM-1047);
    }
    push @enc, "UTF-8";
    push @enc, "65001"; # Windows UTF-8

    return @enc;
}

# LC_ALL can be -1 on some platforms.  And, in fact the implementors could
# legally use any integer to represent any category.  But it makes the most
# sense for them to have used small integers.  Below, we create new locale
# numbers for ones missing from this machine.  We make them very negative,
# hopefully more negative than anything likely to be a valid category on the
# platform, but also below is a check to be sure that our guess is valid.
my $max_bad_category_number = -1000000;

# Initialize this hash so that it looks like e.g.,
#   6 => 'CTYPE',
# where 6 is the value of &POSIX::LC_CTYPE
my %category_name;
unless ($@) {
    my $number_for_missing_category = $max_bad_category_number;
    foreach my $name (qw(ALL COLLATE CTYPE MESSAGES MONETARY NUMERIC TIME)) {
        my $number = eval "&POSIX::LC_$name";

        if ($@) {
            # Use a negative number (smaller than any legitimate category
            # number) if the platform doesn't support this category, so we
            # have an entry for all the ones that might be specified in calls
            # to us.
            $number = $number_for_missing_category-- if $@;
        }
        elsif (   $number !~ / ^ -? \d+ $ /x
               || $number <=  $max_bad_category_number)
        {
            # We think this should be an int.  And it has to be larger than
            # any of our synthetic numbers.
            die "Unexpected locale category number '$number' for LC_$name"
        }

        $category_name{$number} = "$name";
    }
}

sub locales_enabled(;$) {
    # Returns 0 if no locale handling is available on this platform; otherwise
    # 1.
    #
    # The optional parameter is a reference to a list of individual POSIX
    # locale categories.  If present, this function also returns 0 if any of
    # them are individually not available on this platform; otherwise 1.
    # Actually, it is acceptable for the list to be just a simple scalar
    # denoting a single category.
    #
    # If any of the individual categories specified by the optional parameter
    # is all digits (and an optional leading minus), it is taken to be the C
    # enum for the category (e.g., &POSIX::LC_CTYPE).  Otherwise it should be
    # a string name of the category, like 'LC_TIME'.  The initial 'LC_' is
    # optional.  It is a fatal error to call this with something that isn't a
    # known category

    use Config;

    return 0 unless    $Config{d_setlocale}
                        # I (khw) cargo-culted the '?' in the pattern on the
                        # next line.
                    && $Config{ccflags} !~ /\bD?NO_LOCALE\b/
                    && $has_locale_h;

    # Done with the global possibilities.  Now check if any passed in category
    # is disabled.
    my $categories_ref = shift;
    if (defined $categories_ref) {
        $categories_ref = [ $categories_ref ] if ! ref $categories_ref;
        my @local_categories_copy = @$categories_ref;
        for my $category_name_or_number (@local_categories_copy) {
            my $name;
            my $number;
            if ($category_name_or_number =~ / ^ -? \d+ $ /x) {
                $number = $category_name_or_number;
                die "Invalid locale category number '$number'"
                    unless grep { $number == $_ } keys %category_name;
                $name = $category_name{$number};
            }
            else {
                $name = $category_name_or_number;
                $name =~ s/ ^ LC_ //x;
                foreach my $trial (keys %category_name) {
                    if ($category_name{$trial} eq $name) {
                        $number = $trial;
                        last;
                    }
                }
                die "Invalid locale category name '$name'"
                    unless defined $number;
            }

            return 0 if    $number <= $max_bad_category_number
                        || $Config{ccflags} =~ /\bD?NO_LOCALE_$name\b/;

            eval "defined &POSIX::LC_$name";
            return 0 if $@;
        }
    }

    return 1;
}


sub find_locales ($;$) {  # Returns an array of all the locales we found on the
                          # system.  If the optional 2nd parameter is
                          # non-zero, the list is restricted to those locales
                          # that play well with Perl.
                          # The first parameter is either a single locale
                          # category or a reference to a list of categories to
                          # find valid locales for it (or in the case of
                          # multiple) for all of them.
    my $categories = shift;
    my $only_plays_well = shift // 0;

    return unless locales_enabled($categories);

    # Visual C's CRT goes silly on strings of the form "en_US.ISO8859-1"
    # and mingw32 uses said silly CRT
    # This doesn't seem to be an issue any more, at least on Windows XP,
    # so re-enable the tests for Windows XP onwards.
    my $winxp = ($^O eq 'MSWin32' && defined &Win32::GetOSVersion &&
                    join('.', (Win32::GetOSVersion())[1..2]) >= 5.1);
    return if ((($^O eq 'MSWin32' && !$winxp) || $^O eq 'NetWare')
                && $Config{cc} =~ /^(cl|gcc|g\+\+|ici)/i);

    # UWIN seems to loop after taint tests, just skip for now
    return if ($^O =~ /^uwin/);

    _trylocale("C", $categories, \@Locale, $only_plays_well);
    _trylocale("POSIX", $categories, \@Locale, $only_plays_well);
    foreach (0..15) {
        _trylocale("ISO8859-$_", $categories, \@Locale, $only_plays_well);
        _trylocale("iso8859$_", $categories, \@Locale, $only_plays_well);
        _trylocale("iso8859-$_", $categories, \@Locale, $only_plays_well);
        _trylocale("iso_8859_$_", $categories, \@Locale, $only_plays_well);
        _trylocale("isolatin$_", $categories, \@Locale, $only_plays_well);
        _trylocale("isolatin-$_", $categories, \@Locale, $only_plays_well);
        _trylocale("iso_latin_$_", $categories, \@Locale, $only_plays_well);
    }

    # Sanitize the environment so that we can run the external 'locale'
    # program without the taint mode getting grumpy.

    # $ENV{PATH} is special in VMS.
    delete local $ENV{PATH} if $^O ne 'VMS' or $Config{d_setenv};

    # Other subversive stuff.
    delete local @ENV{qw(IFS CDPATH ENV BASH_ENV)};

    if (-x "/usr/bin/locale"
        && open(LOCALES, "/usr/bin/locale -a 2>/dev/null|"))
    {
        while (<LOCALES>) {
            # It seems that /usr/bin/locale steadfastly outputs 8 bit data, which
            # ain't great when we're running this testPERL_UNICODE= so that utf8
            # locales will cause all IO hadles to default to (assume) utf8
            next unless utf8::valid($_);
            chomp;
            _trylocale($_, $categories, \@Locale, $only_plays_well);
        }
        close(LOCALES);
    } elsif ($^O eq 'VMS'
             && defined($ENV{'SYS$I18N_LOCALE'})
             && -d 'SYS$I18N_LOCALE')
    {
    # The SYS$I18N_LOCALE logical name search list was not present on
    # VAX VMS V5.5-12, but was on AXP && VAX VMS V6.2 as well as later versions.
        opendir(LOCALES, "SYS\$I18N_LOCALE:");
        while ($_ = readdir(LOCALES)) {
            chomp;
            _trylocale($_, $categories, \@Locale, $only_plays_well);
        }
        close(LOCALES);
    } elsif (($^O eq 'openbsd' || $^O eq 'bitrig' ) && -e '/usr/share/locale') {

        # OpenBSD doesn't have a locale executable, so reading
        # /usr/share/locale is much easier and faster than the last resort
        # method.

        opendir(LOCALES, '/usr/share/locale');
        while ($_ = readdir(LOCALES)) {
            chomp;
            _trylocale($_, $categories, \@Locale, $only_plays_well);
        }
        close(LOCALES);
    } else { # Final fallback.  Try our list of locales hard-coded here

        # This is going to be slow.
        my @Data;

        # Locales whose name differs if the utf8 bit is on are stored in these two
        # files with appropriate encodings.
        if ($^H & 0x08 || (${^OPEN} || "") =~ /:utf8/) {
            @Data = do "lib/locale/utf8";
        } else {
            @Data = do "lib/locale/latin1";
        }

        # The rest of the locales are in this file.
        push @Data, <DATA>;

        foreach my $line (@Data) {
            my ($locale_name, $language_codes, $country_codes, $encodings) =
                split /:/, $line;
            my @enc = _decode_encodings($encodings);
            foreach my $loc (split(/ /, $locale_name)) {
                _trylocale($loc, $categories, \@Locale, $only_plays_well);
                foreach my $enc (@enc) {
                    _trylocale("$loc.$enc", $categories, \@Locale,
                                                            $only_plays_well);
                }
                $loc = lc $loc;
                foreach my $enc (@enc) {
                    _trylocale("$loc.$enc", $categories, \@Locale,
                                                            $only_plays_well);
                }
            }
            foreach my $lang (split(/ /, $language_codes)) {
                _trylocale($lang, $categories, \@Locale, $only_plays_well);
                foreach my $country (split(/ /, $country_codes)) {
                    my $lc = "${lang}_${country}";
                    _trylocale($lc, $categories, \@Locale, $only_plays_well);
                    foreach my $enc (@enc) {
                        _trylocale("$lc.$enc", $categories, \@Locale,
                                                            $only_plays_well);
                    }
                    my $lC = "${lang}_\U${country}";
                    _trylocale($lC, $categories, \@Locale, $only_plays_well);
                    foreach my $enc (@enc) {
                        _trylocale("$lC.$enc", $categories, \@Locale,
                                                            $only_plays_well);
                    }
                }
            }
        }
    }

    @Locale = sort @Locale;

    return @Locale;
}

sub is_locale_utf8 ($) { # Return a boolean as to if core Perl thinks the input
                         # is a UTF-8 locale

    # On z/OS, even locales marked as UTF-8 aren't.
    return 0 if ord "A" != 65;

    return 0 unless locales_enabled('LC_CTYPE');

    my $locale = shift;

    use locale;
    no warnings 'locale'; # We may be trying out a weird locale

    my $save_locale = setlocale(&POSIX::LC_CTYPE());
    if (! $save_locale) {
        ok(0, "Verify could save previous locale");
        return 0;
    }

    if (! setlocale(&POSIX::LC_CTYPE(), $locale)) {
        ok(0, "Verify could setlocale to $locale");
        return 0;
    }

    my $ret = 0;

    # Use an op that gives different results for UTF-8 than any other locale.
    # If a platform has UTF-8 locales, there should be at least one locale on
    # most platforms with UTF-8 in its name, so if there is a bug in the op
    # giving a false negative, we should get a failure for those locales as we
    # go through testing all the locales on the platform.
    if (CORE::fc(chr utf8::unicode_to_native(0xdf)) ne "ss") {
        if ($locale =~ /UTF-?8/i) {
            ok (0, "Verify $locale with UTF-8 in name is a UTF-8 locale");
        }
    }
    else {
        $ret = 1;
    }

    die "Couldn't restore locale '$save_locale'"
        unless setlocale(&POSIX::LC_CTYPE(), $save_locale);

    return $ret;
}

sub find_utf8_ctype_locale (;$) { # Return the name of a locale that core Perl
                                  # thinks is a UTF-8 LC_CTYPE locale.
                                  # Optional parameter is a reference to a
                                  # list of locales to try; if omitted, this
                                  # tries all locales it can find on the
                                  # platform
    return unless locales_enabled('LC_CTYPE');

    my $locales_ref = shift;

    if (! defined $locales_ref) {

        my @locales = find_locales(&POSIX::LC_CTYPE(),
                                   1 # Reject iffy locales.
                                  );
        $locales_ref = \@locales;
    }

    foreach my $locale (@$locales_ref) {
        return $locale if is_locale_utf8($locale);
    }

    return;
}

1

# Format of data is: locale_name, language_codes, country_codes, encodings
__DATA__
Afrikaans:af:za:1 15
Arabic:ar:dz eg sa:6 arabic8
Brezhoneg Breton:br:fr:1 15
Bulgarski Bulgarian:bg:bg:5
Chinese:zh:cn tw:cn.EUC eucCN eucTW euc.CN euc.TW Big5 GB2312 tw.EUC
Hrvatski Croatian:hr:hr:2
Cymraeg Welsh:cy:cy:1 14 15
Czech:cs:cz:2
Dansk Danish:da:dk:1 15
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
Indonesian:id:id:1 15
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
Norsk Norwegian:no no\@nynorsk nb nn:no:1 15
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
