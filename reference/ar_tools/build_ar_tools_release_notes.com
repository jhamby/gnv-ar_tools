$! File: build_ar_tools_release_notes.com
$!
$! Build the release note file from the three components:
$!    1. The ar_tools_release_note_start.txt
$!    2. The ar_tools_build_steps.txt.
$!
$! Set the name of the release notes from the GNV_PCSI_FILENAME_BASE
$! logical name.
$!
$!
$! 24-Jan-2014  J. Malmberg
$!
$!===========================================================================
$!
$ base_file = f$trnlnm("GNV_PCSI_FILENAME_BASE")
$ if base_file .eqs. ""
$ then
$   write sys$output "@make_pcsi_ar_tools_kit_name.com has not been run."
$   goto all_exit
$ endif
$!
$!
$ type/noheader sys$disk:[]ar_tools_release_note_start.txt,-
        sys$disk:[]ar_tools_build_steps.txt -
        /out='base_file'.release_notes
$!
$ purge 'base_file'.release_notes
$ rename 'base_file.release_notes ;1
$!
$all_exit:
$   exit
