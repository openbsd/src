
BEGIN { unshift @INC, '[-.lib]'; }

my $Invoke_Perl = qq(MCR $^X "-I[-.lib]");

print "1..17\n";

#========== vmsish status ==========
`$Invoke_Perl -e 1`;  # Avoid system() from a pipe from harness.  Mutter.
if ($?) { print "not ok 1 # POSIX status is $?\n"; }
else    { print "ok 1\n"; }
{
  use vmsish qw(status);
  if (not ($? & 1)) { print "not ok 2 # vmsish status is $?\n"; }
  else              { print "ok 2\n"; }
  {
    no vmsish '$?'; # check unimport function
    if ($?) { print "not ok 3 # POSIX status is $?\n"; }
    else    { print "ok 3\n"; }
  }
  # and lexical scoping
  if (not ($? & 1)) { print "not ok 4 # vmsish status is $?\n"; }
  else              { print "ok 4\n"; }
}
if ($?) { print "not ok 5 # POSIX status is $?\n"; }
else    { print "ok 5\n";                          }
{
  use vmsish qw(exit);  # check import function
  if ($?) { print "not ok 6 # POSIX status is $?\n"; }
  else    { print "ok 6\n"; }
}

#========== vmsish exit, messages ==========
{
  use vmsish qw(status);

  $msg = do_a_perl('-e "exit 1"');
  if ($msg !~ /ABORT/) {
    $msg =~ s/\n/\\n/g; # keep output on one line
    print "not ok 7 # subprocess output: |$msg|\n";
  }
  else { print "ok 7\n"; }
  if ($? & 1) { print "not ok 8 # subprocess VMS status: $?\n"; }
  else        { print "ok 8\n"; }

  $msg = do_a_perl('-e "use vmsish qw(exit); exit 1"');
  if (length $msg) {
    $msg =~ s/\n/\\n/g; # keep output on one line
    print "not ok 9 # subprocess output: |$msg|\n";
  }
  else { print "ok 9\n"; }
  if (not ($? & 1)) { print "not ok 10 # subprocess VMS status: $?\n"; }
  else              { print "ok 10\n"; }

  $msg = do_a_perl('-e "use vmsish qw(exit); exit 44"');
  if ($msg !~ /ABORT/) {
    $msg =~ s/\n/\\n/g; # keep output on one line
    print "not ok 11 # subprocess output: |$msg|\n";
  }
  else { print "ok 11\n"; }
  if ($? & 1) { print "not ok 12 # subprocess VMS status: $?\n"; }
  else        { print "ok 12\n"; }

  $msg = do_a_perl('-e "use vmsish qw(exit hushed); exit 44"');
  if ($msg =~ /ABORT/) {
    $msg =~ s/\n/\\n/g; # keep output on one line
    print "not ok 13 # subprocess output: |$msg|\n";
  }
  else { print "ok 13\n"; }

}


#========== vmsish time ==========
{
  my($utctime, @utclocal, @utcgmtime, $utcmtime,
     $vmstime, @vmslocal, @vmsgmtime, $vmsmtime,
     $utcval,  $vmaval, $offset);
  # Make sure apparent local time isn't GMT
  if (not $ENV{'SYS$TIMEZONE_DIFFERENTIAL'}) {
    $oldtz = $ENV{'SYS$TIMEZONE_DIFFERENTIAL'};
    $ENV{'SYS$TIMEZONE_DIFFERENTIAL'} = 3600;
    eval "END { \$ENV{'SYS\$TIMEZONE_DIFFERENTIAL'} = $oldtz; }";
    gmtime(0); # Force reset of tz offset
  }
  {
     use vmsish qw(time);
     $vmstime   = time;
     @vmslocal  = localtime($vmstime);
     @vmsgmtime = gmtime($vmstime);
     $vmsmtime  = (stat $0)[9];
  }
  $utctime   = time;
  @utclocal  = localtime($vmstime);
  @utcgmtime = gmtime($vmstime);
  $utcmtime  = (stat $0)[9];
  
  $offset = $ENV{'SYS$TIMEZONE_DIFFERENTIAL'};

  # We allow lots of leeway (10 sec) difference for these tests,
  # since it's unlikely local time will differ from UTC by so small
  # an amount, and it renders the test resistant to delays from
  # things like stat() on a file mounted over a slow network link.
  if ($utctime - $vmstime + $offset > 10) {
    print "not ok 14  # (time) UTC: $utctime  VMS: $vmstime\n";
  }
  else { print "ok 14\n"; }

  $utcval = $utclocal[5] * 31536000 + $utclocal[7] * 86400 +
            $utclocal[2] * 3600     + $utclocal[1] * 60 + $utclocal[0];
  $vmsval = $vmslocal[5] * 31536000 + $vmslocal[7] * 86400 +
            $vmslocal[2] * 3600     + $vmslocal[1] * 60 + $vmslocal[0];
  if ($vmsval - $utcval + $offset > 10) {
    print "not ok 15  # (localtime)\n# UTC: @utclocal\n# VMS: @vmslocal\n";
  }
  else { print "ok 15\n"; }

  $utcval = $utcgmtime[5] * 31536000 + $utcgmtime[7] * 86400 +
            $utcgmtime[2] * 3600     + $utcgmtime[1] * 60 + $utcgmtime[0];
  $vmsval = $vmsgmtime[5] * 31536000 + $vmsgmtime[7] * 86400 +
            $vmsgmtime[2] * 3600     + $vmsgmtime[1] * 60 + $vmsgmtime[0];
  if ($vmsval - $utcval + $offset > 10) {
    print "not ok 16  # (gmtime)\n# UTC: @utcgmtime\n# VMS: @vmsgmtime\n";
  }
  else { print "ok 16\n"; }

  if ($vmsmtime - $utcmtime + $offset > 10) {
    print "not ok 17  # (stat) UTC: $utcmtime  VMS: $vmsmtime\n";
  }
  else { print "ok 17\n"; }
}

#====== need this to make sure error messages come out, even if
#       they were turned off in invoking procedure
sub do_a_perl {
    local *P;
    open(P,'>vmsish_test.com') || die('not ok ?? : unable to open "vmsish_test.com" for writing');
    print P "\$ set message/facil/sever/ident/text\n";
    print P "\$ define/nolog/user sys\$error _nla0:\n";
    print P "\$ $Invoke_Perl @_\n";
    close P;
    my $x = `\@vmsish_test.com`;
    unlink 'vmsish_test.com';
    return $x;
}

