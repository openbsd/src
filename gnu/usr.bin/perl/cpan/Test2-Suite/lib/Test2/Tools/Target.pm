package Test2::Tools::Target;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;

use Test2::Util qw/pkg_to_file/;

sub import {
    my $class = shift;

    my $caller = caller;
    $class->import_into($caller, @_);
}

sub import_into {
    my $class = shift;
    my $into = shift or croak "no destination package provided";

    croak "No targets specified" unless @_;

    my %targets;
    if (@_ == 1) {
        if (ref $_[0] eq 'HASH') {
            %targets = %{ $_[0] };
        }
        else {
            ($targets{CLASS}) = @_;
        }
    }
    else {
        %targets = @_;
    }

    for my $name (keys %targets) {
        my $target = $targets{$name};

        my $file = pkg_to_file($target);
        require $file;

        $name ||= 'CLASS';

        my $const;
        {
            my $const_target = "$target";
            $const = sub() { $const_target };
        }

        no strict 'refs';
        *{"$into\::$name"} = \$target;
        *{"$into\::$name"} = $const;
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Target - Alias the testing target package.

=head1 DESCRIPTION

This lets you alias the package you are testing into a constant and a package
variable.

=head1 SYNOPSIS

    use Test2::Tools::Target 'Some::Package';

    CLASS()->xxx; # Call 'xxx' on Some::Package
    $CLASS->xxx;  # Same

Or you can specify names:

    use Test2::Tools::Target pkg => 'Some::Package';

    pkg()->xxx; # Call 'xxx' on Some::Package
    $pkg->xxx;  # Same

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
