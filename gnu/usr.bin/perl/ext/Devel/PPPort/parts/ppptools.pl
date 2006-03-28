################################################################################
#
#  ppptools.pl -- various utility functions
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2006/03/28 19:23:02 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2005, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

sub parse_todo
{
  my $dir = shift || 'parts/todo';
  local *TODO;
  my %todo;
  my $todo;

  for $todo (glob "$dir/*") {
    open TODO, $todo or die "cannot open $todo: $!\n";
    my $perl = <TODO>;
    chomp $perl;
    while (<TODO>) {
      chomp;
      s/#.*//;
      s/^\s+//; s/\s+$//;
      /^\s*$/ and next;
      /^\w+$/ or die "invalid identifier: $_\n";
      exists $todo{$_} and die "duplicate identifier: $_ ($todo{$_} <=> $perl)\n";
      $todo{$_} = $perl;
    }
    close TODO;
  }

  return \%todo;
}

sub expand_version
{
  my($op, $ver) = @_;
  my($r, $v, $s) = parse_version($ver);
  $r == 5 or die "only Perl revision 5 is supported\n";
  $op eq '=='     and return "((PERL_VERSION == $v) && (PERL_SUBVERSION == $s))";
  $op eq '!='     and return "((PERL_VERSION != $v) || (PERL_SUBVERSION != $s))";
  $op =~ /([<>])/ and return "((PERL_VERSION $1 $v) || ((PERL_VERSION == $v) && (PERL_SUBVERSION $op $s)))";
  die "cannot expand version expression ($op $ver)\n";
}

sub parse_partspec
{
  my $file = shift;
  my $section = 'implementation';
  my $vsec = join '|', qw( provides dontwarn implementation
                           xsubs xsinit xsmisc xshead xsboot tests );
  my(%data, %options);
  local *F;

  open F, $file or die "$file: $!\n";
  while (<F>) {
    /^##/ and next;
    if (/^=($vsec)(?:\s+(.*))?/) {
      $section = $1;
      if (defined $2) {
        my $opt = $2;
        $options{$section} = eval "{ $opt }";
        $@ and die "Invalid options ($opt) in section $section of $file: $@\n";
      }
      next;
    }
    push @{$data{$section}}, $_;
  }
  close F;

  for (keys %data) {
    my @v = @{$data{$_}};
    shift @v while @v && $v[0]  =~ /^\s*$/;
    pop   @v while @v && $v[-1] =~ /^\s*$/;
    $data{$_} = join '', @v;
  }

  unless (exists $data{provides}) {
    $data{provides} = ($file =~ /(\w+)$/)[0];
  }
  $data{provides} = [$data{provides} =~ /(\S+)/g];

  if (exists $data{dontwarn}) {
    $data{dontwarn} = [$data{dontwarn} =~ /(\S+)/g];
  }

  my @prov;
  my %proto;

  if (exists $data{tests} && (!exists $data{implementation} || $data{implementation} !~ /\S/)) {
    $data{implementation} = '';
  }
  else {
    $data{implementation} =~ /\S/ or die "Empty implementation in $file\n";

    my $p;

    for $p (@{$data{provides}}) {
      if ($p =~ m#^/.*/\w*$#) {
        my @tmp = eval "\$data{implementation} =~ ${p}gm";
        $@ and die "invalid regex $p in $file\n";
        @tmp or warn "no matches for regex $p in $file\n";
        push @prov, do { my %h; grep !$h{$_}++, @tmp };
      }
      elsif ($p eq '__UNDEFINED__') {
        my @tmp = $data{implementation} =~ /^\s*__UNDEFINED__[^\r\n\S]+(\w+)/gm;
        @tmp or warn "no __UNDEFINED__ macros in $file\n";
        push @prov, @tmp;
      }
      else {
        push @prov, $p;
      }
    }

    for (@prov) {
      if ($data{implementation} !~ /\b\Q$_\E\b/) {
        warn "$file claims to provide $_, but doesn't seem to do so\n";
        next;
      }

      # scan for prototypes
      my($proto) = $data{implementation} =~ /
                   ( ^ (?:[\w*]|[^\S\r\n])+
                       [\r\n]*?
                     ^ \b$_\b \s*
                       \( [^{]* \)
                   )
                       \s* \{
                   /xm or next;

      $proto =~ s/^\s+//;
      $proto =~ s/\s+$//;
      $proto =~ s/\s+/ /g;

      exists $proto{$_} and warn "$file: duplicate prototype for $_\n";
      $proto{$_} = $proto;
    }
  }

  for $section (qw( implementation xsubs xsinit xsmisc xshead xsboot )) {
    if (exists $data{$section}) {
      $data{$section} =~ s/\{\s*version\s*(<|>|==|!=|>=|<=)\s*([\d._]+)\s*\}/expand_version($1, $2)/gei;
    }
  }

  $data{provides}   = \@prov;
  $data{prototypes} = \%proto;
  $data{OPTIONS}    = \%options;

  my %prov     = map { ($_ => 1) } @prov;
  my %dontwarn = exists $data{dontwarn} ? map { ($_ => 1) } @{$data{dontwarn}} : ();
  my @maybeprov = do { my %h;
                       grep {
                         my($nop) = /^Perl_(.*)/;
                         not exists $prov{$_}                         ||
                             exists $dontwarn{$_}                     ||
                             (defined $nop && exists $prov{$nop}    ) ||
                             (defined $nop && exists $dontwarn{$nop}) ||
                             $h{$_}++;
                       }
                       $data{implementation} =~ /^\s*#\s*define\s+(\w+)/g };

  if (@maybeprov) {
    warn "$file seems to provide these macros, but doesn't list them:\n  "
         . join("\n  ", @maybeprov) . "\n";
  }

  return \%data;
}

sub compare_prototypes
{
  my($p1, $p2) = @_;
  for ($p1, $p2) {
    s/^\s+//;
    s/\s+$//;
    s/\s+/ /g;
    s/(\w)\s(\W)/$1$2/g;
    s/(\W)\s(\w)/$1$2/g;
  }
  return $p1 cmp $p2;
}

sub ppcond
{
  my $s = shift;
  my @c;
  my $p;

  for $p (@$s) {
    push @c, map "!($_)", @{$p->{pre}};
    defined $p->{cur} and push @c, "($p->{cur})";
  }

  join " && ", @c;
}

sub trim_arg
{
  my $in = shift;
  my $remove = join '|', qw( NN NULLOK );

  $in eq '...' and return ($in);

  local $_ = $in;
  my $id;

  s/[*()]/ /g;
  s/\[[^\]]*\]/ /g;
  s/\b(?:auto|const|extern|inline|register|static|volatile|restrict)\b//g;
  s/\b(?:$remove)\b//;
  s/^\s*//; s/\s*$//;

  if( /^\b(?:struct|union|enum)\s+\w+(?:\s+(\w+))?$/ ) {
    defined $1 and $id = $1;
  }
  else {
    if( s/\b(?:char|double|float|int|long|short|signed|unsigned|void)\b//g ) {
      /^\s*(\w+)\s*$/ and $id = $1;
    }
    else {
      /^\s*\w+\s+(\w+)\s*$/ and $id = $1;
    }
  }

  $_ = $in;

  defined $id and s/\b$id\b//;

  # these don't matter at all
  s/\b(?:auto|extern|inline|register|static|volatile|restrict)\b//g;
  s/\b(?:$remove)\b//;

  s/(?=<\*)\s+(?=\*)//g;
  s/\s*(\*+)\s*/ $1 /g;
  s/^\s*//; s/\s*$//;
  s/\s+/ /g;

  return ($_, $id);
}

sub parse_embed
{
  my @files = @_;
  my @func;
  my @pps;
  my $file;
  local *FILE;

  for $file (@files) {
    open FILE, $file or die "$file: $!\n";
    my($line, $l);

    while (defined($line = <FILE>)) {
      while ($line =~ /\\$/ && defined($l = <FILE>)) {
        $line =~ s/\\\s*//;
        $line .= $l;
      }
      next if $line =~ /^\s*:/;
      $line =~ s/^\s+|\s+$//gs;
      my($dir, $args) = ($line =~ /^\s*#\s*(\w+)(?:\s*(.*?)\s*)?$/);
      if (defined $dir and defined $args) {
        for ($dir) {
          /^ifdef$/   and do { push @pps, { pre => [], cur => "defined($args)"  }         ; last };
          /^ifndef$/  and do { push @pps, { pre => [], cur => "!defined($args)" }         ; last };
          /^if$/      and do { push @pps, { pre => [], cur => $args             }         ; last };
          /^elif$/    and do { push @{$pps[-1]{pre}}, $pps[-1]{cur}; $pps[-1]{cur} = $args; last };
          /^else$/    and do { push @{$pps[-1]{pre}}, $pps[-1]{cur}; $pps[-1]{cur} = undef; last };
          /^endif$/   and do { pop @pps                                                   ; last };
          /^include$/ and last;
          /^define$/  and last;
          /^undef$/   and last;
          warn "unhandled preprocessor directive: $dir\n";
        }
      }
      else {
        my @e = split /\s*\|\s*/, $line;
        if( @e >= 3 ) {
          my($flags, $ret, $name, @args) = @e;
          for (@args) {
            $_ = [trim_arg($_)];
          }
          ($ret) = trim_arg($ret);
          push @func, {
            name  => $name,
            flags => { map { $_, 1 } $flags =~ /./g },
            ret   => $ret,
            args  => \@args,
            cond  => ppcond(\@pps),
          };
        }
      }
    }

    close FILE;
  }

  return @func;
}

sub make_prototype
{
  my $f = shift;
  my @args = map { "@$_" } @{$f->{args}};
  my $proto;
  my $pTHX_ = exists $f->{flags}{n} ? "" : "pTHX_ ";
  $proto = "$f->{ret} $f->{name}" . "($pTHX_" . join(', ', @args) . ')';
  return $proto;
}

sub format_version
{
  my $ver = shift;

  $ver =~ s/$/000000/;
  my($r,$v,$s) = $ver =~ /(\d+)\.(\d{3})(\d{3})/;

  $v = int $v;
  $s = int $s;

  if ($r < 5 || ($r == 5 && $v < 6)) {
    if ($s % 10) {
      die "invalid version '$ver'\n";
    }
    $s /= 10;

    $ver = sprintf "%d.%03d", $r, $v;
    $s > 0 and $ver .= sprintf "_%02d", $s;

    return $ver;
  }

  return sprintf "%d.%d.%d", $r, $v, $s;
}

sub parse_version
{
  my $ver = shift;

  if ($ver =~ /^(\d+)\.(\d+)\.(\d+)$/) {
    return ($1, $2, $3);
  }
  elsif ($ver !~ /^\d+\.[\d_]+$/) {
    die "cannot parse version '$ver'\n";
  }

  $ver =~ s/_//g;
  $ver =~ s/$/000000/;

  my($r,$v,$s) = $ver =~ /(\d+)\.(\d{3})(\d{3})/;

  $v = int $v;
  $s = int $s;

  if ($r < 5 || ($r == 5 && $v < 6)) {
    if ($s % 10) {
      die "cannot parse version '$ver'\n";
    }
    $s /= 10;
  }

  return ($r, $v, $s);
}

1;
