BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = qw(../lib);
    }
}

use Switch;

print "1..4\n";

my $count = 1;
for my $count (1..3, 'four')
{
	switch ([$count])
	{

=pod

=head1 Test

We also test if Switch is POD-friendly here

=cut

		case qr/\d/ {
				switch ($count) {
					case 1     { print "ok 1\n" }
					case [2,3] { print "ok $count\n" }
				}
			    }
		case 'four' { print "ok 4\n" }
	}
}

__END__

=head1 Another test

Still friendly???

=cut
