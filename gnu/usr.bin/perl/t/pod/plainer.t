#!./perl

BEGIN { chdir 't' if -d 't'; @INC = '../lib' }

use Pod::Plainer;
my $parser = Pod::Plainer->new();
my $header = "=pod\n\n";
my $input  = 'plnr_in.pod';
my $output = 'plnr_out.pod';

my $test = 0;
print "1..7\n";
while( <DATA> ) {
    my $expected = $header.<DATA>; 

    open(IN, '>', $input) or die $!;
    print IN $header, $_;
    close IN or die $!;

    open IN, '<', $input or die $!;
    open OUT, '>', $output or die $!;
    $parser->parse_from_filehandle(\*IN,\*OUT);

    open OUT, '<', $output or die $!;
    my $returned; { local $/; $returned = <OUT>; }
    
    unless( $returned eq $expected ) {
       print map { s/^/\#/mg; $_; }
               map {+$_}               # to avoid readonly values
                   "EXPECTED:\n", $expected, "GOT:\n", $returned;
       print "not ";
    }
    printf "ok %d\n", ++$test; 
    close OUT;
    close IN;
}

END { 
    1 while unlink $input;
    1 while unlink $output;
}

__END__
=head <> now reads in records
=head E<lt>E<gt> now reads in records
=item C<-T> and C<-B> not implemented on filehandles
=item C<-T> and C<-B> not implemented on filehandles
e.g. C<< Foo->bar() >> or C<< $obj->bar() >>
e.g. C<Foo-E<gt>bar()> or C<$obj-E<gt>bar()>
The C<< => >> operator is mostly just a more visually distinctive
The C<=E<gt>> operator is mostly just a more visually distinctive
C<uv < 0x80> in which case you can use C<*s = uv>.
C<uv E<lt> 0x80> in which case you can use C<*s = uv>.
C<time ^ ($$ + ($$ << 15))>), but that isn't necessary any more.
C<time ^ ($$ + ($$ E<lt>E<lt> 15))>), but that isn't necessary any more.
The bitwise operation C<<< >> >>>
The bitwise operation C<E<gt>E<gt>>
