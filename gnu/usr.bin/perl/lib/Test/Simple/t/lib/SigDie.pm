package SigDie;

use vars qw($DIE);
$SIG{__DIE__} = sub { $DIE = $@ };

1;
