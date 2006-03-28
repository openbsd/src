#!/usr/bin/perl -w
# I'm assuming that you're running this on some kind of ASCII system, but
# it will generate EDCDIC too. (TODO)
use strict;
use Encode;

my @lines = grep {!/^#/} <DATA>;

sub addline {
  my ($arrays, $chrmap, $letter, $arrayname, $spare, $nocsum, $size,
      $condition) = @_;
  my $line = "/* $letter */ $size";
  $line .= " | PACK_SIZE_SPARE" if $spare;
  $line .= " | PACK_SIZE_CANNOT_CSUM" if $nocsum;
  $line .= ",";
  # And then the hack
  $line = [$condition, $line] if $condition;
  $arrays->{$arrayname}->[ord $chrmap->{$letter}] = $line;
  # print ord $chrmap->{$letter}, " $line\n";
}

sub output_tables {
  my %arrays;

  my $chrmap = shift;
  foreach (@_) {
    my ($letter, $shriek, $spare, $nocsum, $size, $condition)
      = /^([A-Za-z])(!?)\t(\S*)\t(\S*)\t([^\t\n]+)(?:\t+(.*))?$/;
    die "Can't parse '$_'" unless $size;

    if (defined $condition) {
	$condition = join " && ", map {"defined($_)"} split ' ', $condition;
    }
    unless ($size =~ s/^=//) {
      $size = "sizeof($size)";
    }

    addline (\%arrays, $chrmap, $letter, $shriek ? 'shrieking' : 'normal',
	     $spare, $nocsum, $size, $condition);
  }

  my %earliest;
  foreach my $arrayname (sort keys %arrays) {
    my $array = $arrays{$arrayname};
    die "No defined entries in $arrayname" unless $array->[$#$array];
    # Find the first used entry
    my $earliest = 0;
    $earliest++ while (!$array->[$earliest]);
    # Remove all the empty elements.
    splice @$array, 0, $earliest;
    print "unsigned char size_${arrayname}[", scalar @$array, "] = {\n";
    my @lines;
    foreach (@$array) {
	# Remove the assumption here that the last entry isn't conditonal
	if (ref $_) {
	    push @lines,
	      ["#if $_->[0]", "  $_->[1]", "#else", "  0,", "#endif"];
	} else {
	    push @lines, $_ ? "  $_" : "  0,";
	}
    }
    # remove the last, annoying, comma
    my $last = $lines[$#lines];
    my $got;
    foreach (ref $last ? @$last : $last) {
      $got += s/,$//;
    }
    die "Last entry had no commas" unless $got;
    print map {"$_\n"} ref $_ ? @$_ : $_ foreach @lines;
    print "};\n";
    $earliest{$arrayname} = $earliest;
  }

  print "struct packsize_t packsize[2] = {\n";

  my @lines;
  foreach (qw(normal shrieking)) {
    my $array = $arrays{$_};
    push @lines, "  {size_$_, $earliest{$_}, " . (scalar @$array) . "},";
  }
  # remove the last, annoying, comma
  chop $lines[$#lines];
  print "$_\n" foreach @lines;
  print "};\n";
}

my %asciimap = (map {chr $_, chr $_} 0..255);
my %ebcdicmap = (map {chr $_, Encode::encode ("posix-bc", chr $_)} 0..255);

print <<'EOC';
#if 'J'-'I' == 1
/* ASCII */
EOC
output_tables (\%asciimap, @lines);
print <<'EOC';
#else
/* EBCDIC (or bust) */
EOC
output_tables (\%ebcdicmap, @lines);
print "#endif\n";

__DATA__
#Symbol	spare	nocsum	size
c			char
C			unsigned char
U			char
s!			short
s			=SIZE16
S!			unsigned short
v			=SIZE16
n			=SIZE16
S			=SIZE16
v!			=SIZE16	PERL_PACK_CAN_SHRIEKSIGN
n!			=SIZE16	PERL_PACK_CAN_SHRIEKSIGN
i			int
i!			int
I			unsigned int
I!			unsigned int
j			=IVSIZE
J			=UVSIZE
l!			long
l			=SIZE32
L!			unsigned long
V			=SIZE32
N			=SIZE32
V!			=SIZE32	PERL_PACK_CAN_SHRIEKSIGN
N!			=SIZE32	PERL_PACK_CAN_SHRIEKSIGN
L			=SIZE32
p		*	char *
w		*	char
q			Quad_t	HAS_QUAD
Q			Uquad_t	HAS_QUAD
f			float
d			double
F			=NVSIZE
D			=LONG_DOUBLESIZE	HAS_LONG_DOUBLE USE_LONG_DOUBLE
