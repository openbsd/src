# -*- Mode: cperl; coding: utf-8; -*-

use strict;
use CPAN;
use vars qw($D $N);

while (<DATA>) {
  next if /^v/ && $]<5.006; # v-string tests are not for pre-5.6.0
  chomp;
  s/\s*#.*//;
  push @$D, [ split ];
}

$N = scalar @$D;
print "1..$N\n";

while (@$D) {
  my($l,$r,$exp) = @{shift @$D};
  my $res = CPAN::Version->vcmp($l,$r);
  if ($res != $exp){
    print "# l[$l]r[$r]exp[$exp]res[$res]\n";
    print "not ";
  }
  print "ok ", $N-@$D, "\n";
}

__END__
0 0 0
1 0 1
0 1 -1
1 1 0
1.1 0.0a 1
1.1a 0.0 1
1.2.3 1.1.1 1
v1.2.3 v1.1.1 1
v1.2.3 v1.2.1 1
v1.2.3 v1.2.11 -1
1.2.3 1.2.11 1 # not what they wanted
1.9 1.10 1
VERSION VERSION 0
0.02 undef 1
1.57_00 1.57 1
1.5700 1.57 1
1.57_01 1.57 1
0.2.10 0.2 1
20000000.00 19990108 1
1.00 0.96 1
0.7.02 0.7 1
1.3a5 1.3 1
undef 1.00 -1
v1.0 undef 1
v0.2.4 0.24 -1
v1.0.22 122 -1
5.00556 v5.5.560 0
5.005056 v5.5.56 0
5.00557 v5.5.560 1
5.00056 v5.0.561 -1
