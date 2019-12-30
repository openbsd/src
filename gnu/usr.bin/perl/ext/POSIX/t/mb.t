#!./perl

# These tests are in a separate file, because they use fresh_perl_is()
# from test.pl.

# The mb* functions use the "underlying locale" that is not affected by
# the Perl one.  So we run the tests in a separate "fresh_perl" process
# with the correct LC_CTYPE set in the environment.

BEGIN {
    require Config; import Config;
    if ($^O ne 'VMS' and $Config{'extensions'} !~ /\bPOSIX\b/) {
	print "1..0\n";
	exit 0;
    }
    unshift @INC, "../../t";
    require 'loc_tools.pl';
    require 'charset_tools.pl';
    require 'test.pl';
}

plan tests => 4;

use POSIX qw();

SKIP: {
    skip("mblen() not present", 4) unless $Config{d_mblen};

    is(&POSIX::mblen("a", &POSIX::MB_CUR_MAX), 1, 'mblen() basically works');

    skip("LC_CTYPE locale support not available", 3)
      unless locales_enabled('LC_CTYPE');

    my $utf8_locale = find_utf8_ctype_locale();
    skip("no utf8 locale available", 3) unless $utf8_locale;

    local $ENV{LC_CTYPE} = $utf8_locale;
    local $ENV{LC_ALL};
    delete $ENV{LC_ALL};

    fresh_perl_like(
        'use POSIX; print &POSIX::MB_CUR_MAX',
      qr/[4-6]/, {}, 'MB_CUR_MAX is at least 4 in a UTF-8 locale');

  SKIP: {
    my ($major, $minor, $rest) = $Config{osvers} =~ / (\d+) \. (\d+) .* /x;
    skip("mblen() broken (at least for c.utf8) on early HP-UX", 2)
        if   $Config{osname} eq 'hpux'
          && $major < 11 || ($major == 11 && $minor < 31);
    fresh_perl_is(
        'use POSIX; print &POSIX::mblen("'
      . I8_to_native("\x{c3}\x{28}")
      . '", 2)',
      -1, {}, 'mblen() recognizes invalid multibyte characters');

    fresh_perl_is(
     'use POSIX; print &POSIX::mblen("\N{GREEK SMALL LETTER SIGMA}", 2)',
     2, {}, 'mblen() works on UTF-8 characters');
  }
}
