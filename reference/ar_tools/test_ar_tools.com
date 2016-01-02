$! File: test_ar_tools.com
$!
$! Test use of the ar command
$!--------------------------------------------------
$ if f$search("gnv$gnu:[usr.bin]gnv$ar.exe") .nes. ""
$ then
$   ! Acctual installed location
$   ar_cmd = "$gnv$gnu:[usr.bin]gnv$ar.exe"
$ else
$   ar_cmd = "$sys$disk:[]gnv$ar.exe"
$ endif
$!
$ arch_type = f$getsyi("ARCH_NAME")
$ arch_code = f$extract(0, 1, arch_type)
$!
$! Initialize counts.
$ fail = 0
$ test = 0
$ pid = f$getjpi("", "pid")
$!
$gosub clean_params
$!
$! Start with testing the version
$!--------------------------------
$ gosub version_test
$!
$!
$! Sumarize the run
$!---------------------
$REPORT:
$ gosub test_clean_up
$ write sys$output "''test' tests run."
$ if fail .ne. 0
$ then
$    write sys$output "''fail' tests failed!"
$ else
$    write sys$output "All tests passed!"
$ endif
$!
$!
$exit
$!
$!
$! Basic Version test
$!------------------------
$version_test:
$!
$ write sys$output "Version test"
$ test = test + 1
$ out_file = "sys$scratch:test_ar_version.out"
$ cflags = "--version"
$ expect_cc_out = "GNV ar"
$ gosub version_test_driver
$ return
$!
$
$! Driver for compile commands that do not produce an object
$!----------------------------------------------------------
$version_test_driver:
$ if f$search(out_file) .nes. "" then delete 'out_file';*
$ define/user sys$output 'out_file'
$ cmd --version
$ ar_status = '$status' .and. (.not. %x10000000)
$ if ar_status .ne. expect_ar_status
$ then
$   write sys$output "  ** AR status was ''ar_status'  not ''expect_ar_status'!"
$   lcl_fail = lcl_fail + 1
$ endif
$ if f$search(out_file) .nes. ""
$ then
$   open/read xx 'out_file'
$   read xx line_in
$   close xx
$   if f$locate(expect_ar_out, line_in) .ne. 0
$   then
$       write sys$output "  ** ''expect_ar_out' not found in output"
$       lcl_fail = lcl_fail + 1
$       type 'out_file'
$   endif
$   delete 'out_file';*
$ else
$    write sys$output "  ** Output file not created"
$    lcl_fail = lcl_fail + 1
$ endif
$ if lcl_fail .ne. 0 then fail = fail + 1
$ gosub clean_params
$ return
$!
$!
$!
$clean_params:
$ cmd = ar_cmd
$ more_files = ""
$ expect_cc_out = ""
$ out_file = "test_ar_prog.out"
$ lcl_fail = 0
$ expect_ar_status = 1
$ expect_ar_out = ""
$ return
$!
$test_clean_up:
$ noexe = 1
$ return
$!
