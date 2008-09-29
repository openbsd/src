use strict;

if (exists $ENV{'!C:'}) {
  print "You are running this under Cygwin, aren't you? (found '!C' in %ENV)\n";
  print "Are you perhaps using Cygwin Perl? (\$^O is '$^O')\n" if $^O =~ /cygwin/;
  print "I'm sorry but only cmd.exe with e.g. the ActivePerl will work.\n";
  exit(1);
}

unless(# S60 2.x
       $ENV{PATH} =~ m!\\program files\\common files\\symbian\\tools!i
       ||
       # S60 1.2
       $ENV{PATH} =~ m!\\symbian\\6.1\\shared\\epoc32\\tools!i
       ||
       # S80
       $ENV{PATH} =~ m!\\s80_.+?\\epoc32\\!i
       ||
       # UIQ
       $ENV{PATH} =~ m!\\uiq_.+?\\epoc32\\!i
       ) {
    print "I do not think you have installed a Symbian SDK, your PATH is:\n$ENV{PATH}\n";
    exit(1);
}

unless (-f "symbian/symbianish.h") {
  print "You must run this in the top level directory.\n";
  exit(1);
}

if ($] < 5.008) {
  print "You must configure with Perl 5.8 or later.\n";
  exit(1);
}

1;
