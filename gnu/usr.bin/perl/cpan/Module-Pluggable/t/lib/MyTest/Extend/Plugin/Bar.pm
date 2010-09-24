package MyTest::Extend::Plugin::Bar;
use strict;

sub new {
	my $class = shift;
	my %self = @_;

	return bless \%self, $class;
}


sub nork {
	return $_[0]->{'nork'};
}
1;


