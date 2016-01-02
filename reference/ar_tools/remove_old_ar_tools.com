$! File: remove_old_ar_tools.com
$!
$! This is a procedure to remove the old ar_tools images that were installed
$! by the GNV kits and replace them with links to the new image.
$!
$! 30-Dec-2015  J. Malmberg	Ar_tools version
$!
$!==========================================================================
$!
$vax = f$getsyi("HW_MODEL") .lt. 1024
$old_parse = ""
$if .not. VAX
$then
$   old_parse = f$getjpi("", "parse_style_perm")
$   set process/parse=extended
$endif
$!
$ old_cutils = "ar"
$!
$!
$ i = 0
$cutils_loop:
$   file = f$element(i, ",", old_cutils)
$   if file .eqs. "" then goto cutils_loop_end
$   if file .eqs. "," then goto cutils_loop_end
$   call update_old_image "''file'" "[bin]"
$   call update_old_image "''file'" "[usr.bin]"
$   call update_old_image "''file'" "[lib]"
$   i = i + 1
$   goto cutils_loop
$cutils_loop_end:
$!
$!
$!
$if .not. VAX
$then
$   set process/parse='old_parse'
$endif
$!
$all_exit:
$  exit
$!
$! Remove old image or update it if needed.
$!-------------------------------------------
$update_old_image: subroutine
$!
$ file = p1
$ path = p2
$ if path .eqs. "" then path = "[bin]"
$! First get the FID of the new gnv$ar.exe image.
$! Don't remove anything that matches it.
$ new_ar_tool = f$search("gnv$gnu:[usr.bin]gnv$ar.exe")
$!
$ new_ar_tool_fid = "No_new_ar_tool_fid"
$ if new_ar_tool .nes. ""
$ then
$   new_ar_tool_fid = f$file_attributes(new_ar_tool, "FID")
$ endif
$!
$!
$!
$! Now get check the "''file'." and "''file'.exe"
$! May be links or copies.
$! Ok to delete and replace.
$!
$!
$ old_ar_tool_fid = "No_old_ar_tool_fid"
$ old_ar_tool = f$search("gnv$gnu:''path'''file'.")
$ old_ar_tool_exe_fid = "No_old_ar_tool_fid"
$ old_ar_tool_exe = f$search("gnv$gnu:''path'''file'.exe")
$ if old_ar_tool_exe .nes. ""
$ then
$   old_ar_tool_exe_fid = f$file_attributes(old_ar_tool_exe, "FID")
$ endif
$!
$ if old_ar_tool .nes. ""
$ then
$   fid = f$file_attributes(old_ar_tool, "FID")
$   if fid .nes. new_ar_tool_fid
$   then
$       if fid .eqs. old_ar_tool_exe_fid
$       then
$           set file/remove 'old_ar_tool'
$       else
$           delete 'old_ar_tool'
$       endif
$       if new_ar_tool .nes. ""
$       then
$           if (file .nes. "gcc") .and. (file .nes. "g^+^+") .and. -
               (path .eqs. "[usr.bin]")
$           then
$               set file/enter='old_ar_tool' 'new_ar_tool'
$           endif
$       endif
$   endif
$ endif
$!
$ if old_ar_tool_exe .nes. ""
$ then
$   if old_ar_tool_fid .nes. new_ar_tool_fid
$   then
$       delete 'old_ar_tool_exe'
$       if new_ar_tool .nes. ""
$       then
$           if (file .nes. "gcc") .and. (file .nes. "g^+^+") .and. -
               (path .eqs. "[usr.bin]")
$           then
$               set file/enter='old_ar_tool_exe' 'new_ar_tool'
$           endif
$       endif
$   endif
$ endif
$!
$ exit
$ENDSUBROUTINE ! Update old image
