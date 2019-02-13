#!./perl -w
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    skip_all_if_miniperl();
}

use Config;

my $perlio_log = "perlio$$.txt";

skip_all "DEBUGGING build required"
  unless $::Config{ccflags} =~ /(?<!\S)-DDEBUGGING(?!\S)/
         or $^O eq 'VMS' && $::Config{usedebugging_perl} eq 'Y';

plan tests => 8;

END {
    unlink $perlio_log;
}
{
    unlink $perlio_log;
    local $ENV{PERLIO_DEBUG} = $perlio_log;
    fresh_perl_is("print qq(hello\n)", "hello\n",
                  { stderr => 1 },
                  "No perlio debug file without -Di...");
    ok(!-e $perlio_log, "...no perlio.txt found");
    fresh_perl_like("print qq(hello\n)", qr/\nEXECUTING...\n{1,2}hello\n?/,
                  { stderr => 1, switches => [ "-Di" ] },
                  "Perlio debug file with both -Di and PERLIO_DEBUG...");
    ok(-e $perlio_log, "... perlio debugging file found with -Di and PERLIO_DEBUG");

    unlink $perlio_log;
    fresh_perl_like("print qq(hello\n)", qr/define raw/,
                  { stderr => 1, switches => [ "-TDi" ] },
                  "Perlio debug output to stderr with -TDi (with PERLIO_DEBUG)...");
    ok(!-e $perlio_log, "...no perlio debugging file found");
}

{
    local $ENV{PERLIO_DEBUG};
    fresh_perl_like("print qq(hello)", qr/define raw/,
                    { stderr => 1, switches => [ '-Di' ] },
                   "-Di defaults to stderr");
    fresh_perl_like("print qq(hello)", qr/define raw/,
                    { stderr => 1, switches => [ '-TDi' ] },
                   "Perlio debug output to STDERR with -TDi (no PERLIO_DEBUG)");
}

