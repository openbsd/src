$! Brief DCL procedure to parse current Perl version out of
$! patchlevel.h, and update the version token for ARCHLIB
$! config.vms and descrip.mms if necessary.
$ err = "Write Sys$Error"
$
$ If p1.eqs."" Then p1 = "patchlevel.h"
$ If p2.eqs."" Then p2 = F$Parse("config.vms",p1,"[.vms]")
$ If p3.eqs."" Then p3 = F$Parse("descrip.mms",p1,"[.vms]")
$
$ If F$Search(p1).eqs.""
$ Then
$   err "Can't find ''p1' - exiting"
$   Exit 98962  ! RMS$_FNF
$ EndIf
$ plevel = ""
$ sublevel = ""
$ Open/Read patchlevel_h &p1
$
$ pread:
$ Read/End_Of_File=pdone patchlevel_h line
$ If F$Locate("#define PATCHLEVEL",line).ne.F$Length(line)
$ Then
$    plevel = F$Element(2," ",line)
$    If F$Length(plevel).lt.3 Then -
       plevel = F$Extract(0,3 - F$Length(plevel),"000") + plevel
$ EndIf
$ If F$Locate("#define SUBVERSION",line).ne.F$Length(line)
$ Then
$    sublevel = F$Element(2," ",line)
$    If F$Length(sublevel).lt.2 Then -
       sublevel = F$Extract(0,2 - F$Length(sublevel),"00") + sublevel
$ EndIf
$ If .not.(plevel.nes."" .and. sublevel.nes."") Then Goto pread
$
$ pdone:
$ Close patchlevel_h
$!
$ If sublevel.eq.0 Then sublevel = ""
$ perl_version = "5_" + plevel + sublevel
$ If F$GetSyi("HW_MODEL").gt.1024
$ Then
$   arch = "AXP"
$ Else
$   arch = "VAX"
$ EndIf
$ If p2.eqs."#NOFILE#"
$ Then
$   Write Sys$Output "Perl version directory name is ""''perl_version'"""
$   Exit
$ EndIf
$!
$ token = """""""""/perl_root/lib/VMS_''arch'/''perl_version'"""""""""
$ If sublevel.eqs."" Then token = token + "  "
$ token = token + "  /**/"
$ Call update_file "''p2'" "#define ARCHLIB_EXP" "''token'"
$ teststs = $Status
$ If .not.teststs Then Exit teststs
$!
$ If teststs.ne.1 ! current values in config.vms are appropriate
$ Then
$   token = """""""""/perl_root/lib/VMS_''arch'""""""""  /**/"
$   Call update_file "''p2'" "#define OLDARCHLIB_EXP" "''token'"
$   If .not.$Status Then Exit $Status
$!
$   token = """""""""/perl_root/lib/site_perl/VMS_''arch'""""""""  /**/"
$   Call update_file "''p2'" "#define SITEARCH_EXP" "''token'"
$   If .not.$Status Then Exit $Status
$EndIf
$!
$ token = "''perl_version'"
$ If sublevel.eqs."" Then token = token + "  "
$ token = token + "#"
$ Call update_file "''p3'" "PERL_VERSION =" "''token'"
$ If .not.$Status Then Exit $Status
$ If $Status.eq.3
$ Then
$   cmd = "MM[SK]"
$   If F$Locate("MMS",p3).eqs."" Then cmd = "make"
$   err "The PERL_VERSION macro was out of date in the file"
$   err "    ''p3'"
$   err "The file has been corrected, but you must restart the build process"
$   err "by reinvoking ''cmd' to incorporate the new value."
$   Exit 44  ! SS$_ABORT
$ EndIf
$!
$ update_file: Subroutine
$
$ If F$Search(p1).nes.""
$ Then
$   Search/Exact/Output=_NLA0: 'p1' "''p2' ''p3'"
$   If $Status.eq.%X08D78053  ! SEARCH$_NOMATCHES
$   Then
$     Open/Read/Write/Error=done  file &p1
$
$     nextline:
$     Read/End_of_File=done file line
$     If F$Locate(p2,line).ne.F$Length(line)
$     Then
$       Write/Update file "''p2' ''p3'"
$       Goto done
$     EndIf
$     Goto nextline
$
$     done:
$     Close file
$     Exit 3  ! Unused success status
$   EndIf
$   Exit 1  ! SS$_NORMAL
$ Else
$   err "Can't find ''p1'"
$   Exit 98962  ! RMS$_FNF
$ EndIf
$ EndSubroutine
