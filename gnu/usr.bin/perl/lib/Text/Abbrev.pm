package Text::Abbrev;
require 5.000;
require Exporter;

=head1 NAME

abbrev - create an abbreviation table from a list

=head1 SYNOPSIS

    use Abbrev;
    abbrev *HASH, LIST


=head1 DESCRIPTION

Stores all unambiguous truncations of each element of LIST
as keys key in the associative array indicated by C<*hash>.
The values are the original list elements.

=head1 EXAMPLE

    abbrev(*hash,qw("list edit send abort gripe"));

=cut

@ISA = qw(Exporter);
@EXPORT = qw(abbrev);

# Usage:
#	&abbrev(*foo,LIST);
#	...
#	$long = $foo{$short};

sub abbrev {
    local(*domain) = shift;
    @cmp = @_;
    %domain = ();
    foreach $name (@_) {
	@extra = split(//,$name);
	$abbrev = shift(@extra);
	$len = 1;
	foreach $cmp (@cmp) {
	    next if $cmp eq $name;
	    while (substr($cmp,0,$len) eq $abbrev) {
		$abbrev .= shift(@extra);
		++$len;
	    }
	}
	$domain{$abbrev} = $name;
	while (@extra) {
	    $abbrev .= shift(@extra);
	    $domain{$abbrev} = $name;
	}
    }
}

1;

