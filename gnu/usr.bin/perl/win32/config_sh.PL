my %opt;
while (@ARGV && $ARGV[0] =~ /^([\w_]+)=(.*)$/)
 {
  $opt{$1}=$2;
  shift(@ARGV);
 }

if ($] =~ /\.(\d\d\d)?(\d\d)?$/) { # should always be true
  $opt{PATCHLEVEL} = int($1 || 0);
  $opt{SUBVERSION} = $2 || '00';
}

$opt{'cf_by'} = $ENV{USERNAME} unless $opt{'cf_by'};
$opt{'cf_email'} = $opt{'cf_by'} . '@' . (gethostbyname('localhost'))[0]
	unless $opt{'cf_email'};

while (<>)
 {
  s/~([\w_]+)~/$opt{$1}/g;
  $_ = "$1='$opt{$1}'\n" if (/^([\w_]+)=/ && exists($opt{$1}));
  print;
 }

