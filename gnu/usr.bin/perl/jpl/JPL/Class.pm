package JPL::Class;
use JPL::AutoLoader ();

sub DESTROY {}

sub import {
    my $class = shift;
    foreach $class (@_) {
	*{$class . "::AUTOLOAD"} = *JPL::AutoLoader::AUTOLOAD;
	*{$class . "::DESTROY"} = \&DESTROY;
    }
}
1;
