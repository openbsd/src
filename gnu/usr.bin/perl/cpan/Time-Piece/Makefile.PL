use ExtUtils::MakeMaker;

require 5.006;

WriteMakefile(
    'NAME'	=> 'Time::Piece',
    'VERSION_FROM' => 'Piece.pm', # finds $VERSION
    'AUTHOR' => 'Matt Sergeant',
    'ABSTRACT_FROM' => 'Piece.pm',
);
