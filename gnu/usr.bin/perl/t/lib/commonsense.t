#!./perl

chdir 't' if -d 't';
@INC = '../lib';
require Config; import Config;
if (($Config{'extensions'} !~ /\b(DB|[A-Z]DBM)_File\b/) ){
  print "Bail out! Perl configured without DB_File or [A-Z]DBM_File\n";
  exit 0;
}
if (($Config{'extensions'} !~ /\bFcntl\b/) ){
  print "Bail out! Perl configured without Fcntl module\n";
  exit 0;
}
if (($Config{'extensions'} !~ /\bIO\b/) ){
  print "Bail out! Perl configured without IO module\n";
  exit 0;
}
# hey, DOS users do not need this kind of common sense ;-)
if ($^O ne 'dos' && ($Config{'extensions'} !~ /\bFile\/Glob\b/) ){
  print "Bail out! Perl configured without File::Glob module\n";
  exit 0;
}

print "1..1\nok 1\n";

