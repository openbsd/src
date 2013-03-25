#!./perl

BEGIN {
    require Config;
    if (($Config::Config{extensions} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

use strict;
use Test::More;
use feature (sprintf(":%vd", $^V)); # to avoid relying on the feature
                                    # logic to add CORE::

# Many functions appear in multiple lists, so that shift() and shift(foo)
# are both tested.
# For lists, we test 0 to 2 arguments.
my @nary = (
 # nullary functions
     [qw( abs alarm break chr cos chop close chdir chomp chmod chown
          chroot caller continue die dump exp exit exec endgrent
          endpwent endnetent endhostent endservent
          endprotoent evalbytes fc fork glob
          getppid getpwent getprotoent gethostent getnetent getservent
          getgrent getlogin getc gmtime hex int lc log lstat length
          lcfirst localtime mkdir ord oct pop quotemeta ref rand
          rmdir reset reverse readlink select setpwent setgrent
          shift sin sleep sqrt srand stat __SUB__ system tell time times
          uc utime umask unlink ucfirst wantarray warn wait write    )],
 # unary
     [qw( abs alarm bless binmode chr cos chop close chdir chomp
          chmod chown chroot closedir die do dump exp exit exec
          each evalbytes fc fileno getpgrp getpwnam getpwuid getpeername
          getprotobyname getprotobynumber gethostbyname
          getnetbyname getsockname getgrnam getgrgid
          getc glob gmtime hex int join keys kill lc
          log lock lstat length lcfirst localtime
          mkdir ord oct open pop push pack quotemeta
          ref rand rmdir reset reverse readdir readlink
          rewinddir select setnetent sethostent setservent
          setprotoent shift sin sleep sprintf splice sqrt
          srand stat system tell tied telldir uc utime umask
          unpack unlink unshift untie ucfirst values warn write )],
 # binary, but not infix
     [qw( atan2 accept bind binmode chop chomp chmod chown crypt
          connect die exec flock formline getpriority gethostbyaddr
          getnetbyaddr getservbyname getservbyport index join kill
          link listen mkdir msgget open opendir push pack pipe
          rename rindex reverse seekdir semop setpgrp shutdown
          sprintf splice substr system symlink syscall syswrite
          tie truncate utime unpack unlink warn waitpid           )],
 # ternary
     [qw( fcntl getsockopt index ioctl join  kill  msgctl
          msgsnd open push pack  read  rindex  seek  send
          semget setpriority shmctl shmget sprintf splice
          substr sysopen sysread sysseek syswrite tie vec )],
 # quaternary
     [qw( open read  recv  send  select  semctl  setsockopt  shmread
          shmwrite socket splice substr sysopen sysread syswrite tie )],
 # quinary
     [qw( msgrcv open socketpair splice )]
);

use B::Deparse;
my $deparse = new B::Deparse;

sub CORE_test {
  my($keyword,$expr,$name) = @_;
  package test;
  use subs ();
  import subs $keyword;
  ::like
      $deparse->coderef2text(
         eval "no strict 'vars'; sub { () = $expr }" or die "$@in $expr"
      ),
      qr/\sCORE::$keyword.*;/,
      $name||$keyword  
}

for my $argc(0..$#nary) {
 for(@{$nary[$argc]}) {
  CORE_test
     $_,
    "CORE::$_(" . join(',',map "\$$_", (undef,"a".."z")[1..$argc]) . ")",
    "$_, $argc argument" . "s"x($argc != 1);
 }
}

# Special cases
CORE_test dbmopen => 'CORE::dbmopen %foo, $bar, $baz';
CORE_test dbmclose => 'CORE::dbmclose %foo';
CORE_test eof => 'CORE::eof $foo', 'eof $arg';
CORE_test eof => 'CORE::eof', 'eof';
CORE_test eof => 'CORE::eof()', 'eof()';
CORE_test exec => 'CORE::exec $foo $bar', 'exec PROGRAM LIST';
CORE_test each => 'CORE::each %bar', 'each %hash';
CORE_test keys => 'CORE::keys %bar', 'keys %hash';
CORE_test reverse => 'CORE::reverse sort @foo', 'reverse sort';
CORE_test system => 'CORE::system $foo $bar', 'system PROGRAM LIST';
CORE_test values => 'CORE::values %bar', 'values %hash';
CORE_test not => '3 unless CORE::not $a && $b', 'not';
CORE_test readline => 'CORE::readline $a.$b', 'readline';
CORE_test readpipe => 'CORE::readpipe $a+$b', 'readpipe';

# Tests for prefixing feature.pm-enabled keywords with CORE:: when
# feature.pm is not enabled are in deparse.t, as they fit that for-
# mat better.

done_testing();
