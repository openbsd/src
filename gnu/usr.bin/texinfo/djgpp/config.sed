/for ac_dir in \$PATH; do/,/IFS="\$ac_save_[Ii][Ff][Ss]"/ {
  s|test -f \$ac_dir/|test -x $ac_dir/|
}

/IFS="\${IFS=/,/IFS="\$ac_save_ifs"/ {
    s|test -f \$ac_dir/|test -x $ac_dir/|
}

s|po2tbl\.sed\.in|po2tblsed.in|g

/ac_given_INSTALL=/,/^CEOF/ {
  /^s%@l@%/a\
  /TEXINPUTS=/s,:,\\\\\\\\\\\\\\;,g\
  /^terminal\\.o:/s,: ,: pcterm.c ,\
  s,po2tbl\\.sed\\.in,po2tblsed.in,g\
  s,Makefile\\.in\\.in,Makefile.in-in,g
}

/^CONFIG_FILES=/,/^EOF/ {
  s|po/Makefile.in|po/Makefile.in:po/Makefile.in-in|
}

/\*) srcdir=/s,/\*,/*|[A-z]:/*,
/\$]\*) INSTALL=/s,\[/\$\]\*,&|[A-z]:/*,
/\$]\*) ac_rel_source=/s,\[/\$\]\*,&|[A-z]:/*,
/ac_file_inputs=/s,\( -e "s%\^%\$ac_given_srcdir/%"\)\( -e "s%:% $ac_given_srcdir/%g"\),\2\1,
