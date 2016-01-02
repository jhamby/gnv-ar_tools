$! File: build_ar_tools.com
$!
$!
$ if p1 .eqs. "CLEAN" then goto clean
$ if p1 .eqs. "REALCLEAN" then goto realclean
$!
$ if "''cc'" .eqs. ""
$ then
$   cc = "cc"
$ else
$   cc = cc
$ endif
$
$ cc = cc + "/DEFINE=(_POSIX_EXIT,_VMS_WAIT)"
$ cc = cc + "/nested_include_directory=primary"
$ on warning then goto no_main_qual
$ msgsetting = f$environment("message")
$ ! hush, we are just testing.
$ set message /nofacility/noseverity/noidentification/notext
$ cc /OBJECT=NL: as.c /MAIN=POSIX_EXIT
$ cc = cc + "/MAIN=POSIX_EXIT"
$ cc = cc + "/names=(as_is,shortened)/repository=sys$disk:[]"
$ no_main_qual:
$ on error then exit	! restore default
$ set message 'msgsetting'	! restore message
$
$ if p1 .eqs. "DEBUG"
$ then
$   write sys$output "Adding /noopt/debug to CC command"
$   cc = cc + "/NOOPT/DEBUG"
$   write sys$output "Adding /debug to LINK command"
$   link = "LINK/DEBUG"
$ endif
$ on error then goto done
$ set verify
$ cc ar.c
$ cc utils.c
$ if f$search ("vms_crtl_init.obj") .eqs. ""
$ then
$   cc vms_crtl_init.c/obj=vms_crtl_init.obj
$ endif
$ link/exe=gnv$ar.exe ar,utils,vms_crtl_init
$ link/debug/exe=gnv$debug-ar.exe ar,utils,vms_crtl_init
$done:
$ set noverify
$!
$exit
$!
$realclean:
$clean:
$  file="gnv$ar.exe"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$ar.map"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$debug-ar.exe"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$debug-ar.map"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="ar.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="ar.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="utils.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="utils.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="vms_crtl_init.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="vms_crtl_init.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$exit
