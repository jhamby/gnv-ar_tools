/* ar.c

Modification history:
    00	Unknown

    01	J. Malmberg	13-Jan-2006
	Fix library module name to be in exact case.
	Indicate that unimplemented or unrecognized switches are
	ignored.

    02	Karl Puder	2007.05.04
	Spell out command name and qualifier fully for "LIBRARY /REPLACE".

    03  J. Malmberg	30-Dec-2015
	* Fixes for missing object modules supplied by Eric Robertson of
	  IQware.
	* Add help command.

    04  J. Malmberg
	* Pull in current utils.c from ld_tools
	* Fix verify parameter to do something.
	* Fix list, print, and extract to do something.

    05  J. Malmberg
        * Do not try to process a null library file.
*/

/* TODO #1: Use lbr$ calls instead of DCL shell */
/* TODO #2: See if binutils understands VMS librarian formats */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unixlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <descrip.h>
#include <stsdef.h>

#ifndef __VAX
#include <ppropdef.h>
#endif

#include "utils.h"
#include "version.h"
#include "vms_eco_level"

int gnv_ar_debug=0;
FILE *cmd_proc;
char *obj_suffix;
char  current_file_path[4096];
char  current_vms_file_path[4096];

void usage(const char *str,...)
{
  va_list va;
  va_start(va,str);

  printf("Error: ");
  vprintf(str,va);

  va_end(va);

  exit(2);
}

char *libname = NULL;
char *relpos = NULL;
char *count_str = NULL;

list_t objfiles_list=0;
list_t *objfiles_list_last = &objfiles_list;

int ar_op = 0;

#define AR_OP_DELETE	'd'
#define AR_OP_INSERT	'r'
#define AR_OP_LIST	't'
#define AR_OP_PRINT	'p'
#define AR_OP_EXTRACT	'x'
#define AR_OP_MOVE	'm'

int ar_mod = 0;

#define AR_MOD_VERSION	0x0001
#define AR_MOD_VERBOSE	0x0002
#define AR_MOD_CREATE	0x0004
#define AR_MOD_ODATE	0x0008
#define AR_MOD_UPDATE	0x0010
#define AR_MOD_OBJINDX	0x0020
#define AR_MOD_HELP	0x0040

#define AR_MOD_AFTER	0x0100
#define AR_MOD_BEFORE	0x0200
#define AR_MOD_NOSYM	0x0400
#define AR_MOD_TRUNC	0x0800
#define AR_MOD_COUNT	0x1000
#define AR_MOD_FULLPATH	0x2000

/*
 *  Not sure what the real max is, we say 1000 to give us some
 *  fudge room, in case we make a mistake counting the length.
 *  J. Malmberg - 1024/4097 are the real max depending on version.
 */
#if __CRTL_VER >= 70302000 && !defined(__VAX)
#define MAX_CMD_PROC_LINE 4000
#else
#define MAX_CMD_PROC_LINE 1000
#endif

/*
** These are the action routines for the list and extract support.
*/

unsigned long lib_action_rtn_list(struct dsc$descriptor *d, long rfa[2])
{
    printf("%1.*s%s\n", d->dsc$w_length, d->dsc$a_pointer, obj_suffix);
    return 1;
}

unsigned long lib_action_rtn_extract(struct dsc$descriptor *d, long rfa[2])
{
    fprintf(cmd_proc,
      "$ library /object /extract=\"%1.*s\" /output=%1.*s%s %s\n",
        d->dsc$w_length, d->dsc$a_pointer,
        d->dsc$w_length, d->dsc$a_pointer, obj_suffix,
        lib_action_rtn_name);
    return 1;
}

unsigned long lib_action_rtn_print(struct dsc$descriptor *d, long rfa[2])
{
    fprintf(cmd_proc,
      "$ library /object /extract=\"%1.*s\" /output=SYS$OUTPUT: %s\n",
        d->dsc$w_length, d->dsc$a_pointer,
        lib_action_rtn_name);
    return 1;
}

void do_ar_list(void)
{
int status;
char *vms_libname;

    vms_libname = unix_to_vms(libname,FALSE);


    status = lib_list (vms_libname, lib_action_rtn_list);
    if (!$VMS_STATUS_SUCCESS(status)) {
      fprintf(stderr,"Unable to list library file: %s\n", libname);
      exit(2);
    }
    exit(0);
}

void do_ar_delete(void)
{
    puts("Delete member command ignored on OpenVMS.");
    exit(0);
}

void do_ar_move(void)
{
    puts("Move member command ignored on OpenVMS.");
    exit(0);
}

void do_ar_extract(void)
{
  int status;
  char *vms_libname;
  char cmd_proc_name[2040];
  char cmd_out_name[2040];

  vms_libname = unix_to_vms(libname,FALSE);

  cmd_proc=fopen_com_out(cmd_proc_name,
			 cmd_out_name,
			 "ar",
			 TRUE,
			 "w");

    if (ar_mod & AR_MOD_VERBOSE) {
	fprintf(cmd_proc, "$set verify\n");
    }
#ifndef __VAX
#ifdef PPROP$C_TOKEN
    fprintf(cmd_proc, "$set process/token=extended\n");
#endif
#ifdef PPROP$C_PARSE_STYLE_TEMP
    fprintf(cmd_proc, "$set process/parse_style=extended\n");
#endif
#endif
  if (ar_op == AR_OP_PRINT)
    status = lib_list (vms_libname, lib_action_rtn_print);
  else
    status = lib_list (vms_libname, lib_action_rtn_extract);

  if (!$VMS_STATUS_SUCCESS(status)) {
    fprintf(stderr,"Unable to extract from library file: %s\n", libname);
    fprintf(cmd_proc,"$ exit 2\n");
  }
  if ((status&3)==2)
     exit(2);
  exit(0);
}

void do_ar_work(void)
{
  int status;
  list_t ptr;
  char cmd_proc_name[2040];
  char cmd_out_name[2040];
  char *vms_libname;
  char *file_start_ptr;
  char *vms_file_path_ptr;
  int cmd_len = 0;
  char * lastdot;

  vms_libname = unix_to_vms(libname,FALSE);

  /* Hack, to do this right needs to much of a rewrite for now */
  lastdot = strrchr(vms_libname, '.');
  if (lastdot != NULL) {
      switch (lastdot[1]) {
      case 't':
      case 'T':
          obj_suffix = ".txt";
          break;
      case 'm':
      case 'M':
          obj_suffix = ".mar";
          break;
      case 'h':
      case 'H':
          obj_suffix = ".hlp";
          break;
      }
  }

    /* Exit success if no object files? */
  switch(ar_op) {
  case AR_OP_MOVE:
  case AR_OP_DELETE:
  case AR_OP_INSERT:

    if (objfiles_list == NULL)
       exit(0);
  }

  switch(ar_op) {

  case AR_OP_LIST:
      do_ar_list();
      break;

  case AR_OP_MOVE:
      do_ar_move();
      break;

  case AR_OP_DELETE:
      do_ar_delete();
      break;

  case AR_OP_PRINT:
  case AR_OP_EXTRACT:
      do_ar_extract();
      break;

  case AR_OP_INSERT:
    cmd_proc=fopen_com_out(cmd_proc_name,
			 cmd_out_name,
			 "ar",
			 TRUE,
			 "w");

    if (ar_mod & AR_MOD_VERBOSE) {
	fprintf(cmd_proc, "$set verify\n");
    }
#ifndef __VAX
#ifdef PPROP$C_TOKEN
    fprintf(cmd_proc, "$set process/token=extended\n");
#endif
#ifdef PPROP$C_PARSE_STYLE_TEMP
    fprintf(cmd_proc, "$set process/parse_style=extended\n");
#endif
#endif
    cmd_len = fprintf(cmd_proc,"$library /replace -\n");
    if (!file_exists(libname)) {
	cmd_len += fprintf(cmd_proc,"/create -\n");
    }
    cmd_len += fprintf(cmd_proc,"%s ",vms_libname);

    if (objfiles_list != NULL) {
	cmd_len += fprintf(cmd_proc, " -\n");

	ptr=objfiles_list;
	while (ptr != NULL) {
	    char *vms_file;
	    int vms_len;

	    vms_file = unix_to_vms(ptr->str, FALSE);
	    vms_len = strlen(vms_file);
	    if (file_exists(ptr->str)) {
		/*
		 *  If command line is going to be too long, create a
		 *  new command line.
		 */
		if ((cmd_len + vms_len) > MAX_CMD_PROC_LINE) {
		    fprintf(cmd_proc, "\n");
		    cmd_len = fprintf(cmd_proc,
				      "$library /replace %s -\n", vms_libname);
		} else {
		    /*
		     * If this is not the first time through, add a comma.
		     */
		    if (ptr != objfiles_list) {
			cmd_len += fprintf(cmd_proc, ",");
		    }
		    cmd_len += fprintf(cmd_proc, " -\n");
		}

		cmd_len += fprintf(cmd_proc, "%s", vms_file);
	    } else {
		fprintf(stderr, "Warning: %s is not accessible.\r\n", ptr->str);
		status = ENOENT;
	    }
	ptr=ptr->next;
	} /* end while (ptr!= NULL) */
    } /* end if (objfiles_list!=NULL) */

    fprintf(cmd_proc,"\n");
    fclose(cmd_proc);

    status = run_cmdproc(cmd_proc_name, cmd_out_name);

    output_file(stderr,cmd_out_name,remove_temps);

    /*
     *  Must remove cmd_proc last since it is the name
     *  which other temps are based on.
     */
    if (remove_temps)
      remove(cmd_proc_name);

    if ((status&3)==2)
      exit(2);
    exit(0);
    break;
  }

  fprintf(stderr,"Unknown command %c\n", ar_op);
  exit (0);
}


/* There can be only one operation specified */
void set_ar_op(int op)
{
    if (ar_op == 0) {
	ar_op = op;
	return;
    }

    puts("AR: multiple operations specified - Only one allowed.");
    exit(1);
}

void handle_switch(char *x)
{
  switch(x[0]) {

  case 'd':	/* Delete */
  case 'm':	/* Move */
  case 'p':	/* Print */
  case 't':	/* List */
  case 'x':	/* Extract */
    set_ar_op(x[0]);
    break;

  case 'q':	/* Quick Append - Same as 'r' on OpenVMS */
  case 'r':	/* Insert or replace */
    set_ar_op(AR_OP_INSERT);
    break;

  case 'a':	/* Add after - Not usually available on OpenVMS */
    printf("%c unimplemented switch - ignored\n", x[0]);
    ar_mod |= AR_MOD_AFTER;
    break;

  case 'i':	/* Insert Before - Same as below */
  case 'b':	/* Add before - Not usually available on OpenVMS */
    printf("%c unimplemented switch - ignored\n", x[0]);
    ar_mod |= AR_MOD_BEFORE;
    break;

  case 'c':	/* Create Archive */
    ar_mod |= AR_MOD_CREATE;
    break;

  case 'f':	/* Truncate names to fit library restrictions */
    ar_mod |= AR_MOD_TRUNC;
    break;

  case 'h':	/* Help */
    ar_mod |= AR_MOD_HELP;
    break;

  case 'l':	/* This modifier is accepted but not used */
    break;

  case 'N':	/* Multiple entries in the archive - Not in VMS librarian */
    printf("%c unimplemented switch - ignored\n", x[0]);
    ar_mod |= AR_MOD_COUNT;
    break;

  case 'o':	/* Preserve original dates */
    printf("%c unimplemented switch - ignored\n", x[0]);
    ar_mod |= AR_MOD_ODATE;
    break;

  case 'P':	/* Use full path name when matching names in archive */
    printf("%c unimplemented switch - ignored\n", x[0]);
    ar_mod |= AR_MOD_FULLPATH;
    break;

  case 's':	/* Write object file index into the archive */
    ar_mod |= AR_MOD_OBJINDX;
    break;

  case 'S':	/* Do not generate an archive symbol table */
    ar_mod |= AR_MOD_NOSYM;
    printf("%c unimplemented switch - ignored\n", x[0]);
    break;

  case 'u':	/* Update older only */
    /* u is to replace only older modules, requires getting the date from the
       existing module and comparing it with the new module.
     */
    ar_mod |= AR_MOD_UPDATE;
    printf("%c unimplemented switch - ignored\n", x[0]);
    break;

  case 'v':	/* Verbose */
    ar_mod |= AR_MOD_VERBOSE;
    break;

  case 'V':	/* Version */
    ar_mod |= AR_MOD_VERSION;
    break;

  case 'X':	/* Ignore -X options */
    break;

  case '-':  /* --version */
    if (strcmp(&x[1], "version") == 0) {
        ar_mod |= AR_MOD_VERSION;
        break;
    }
    if (strcmp(&x[1], "help") != 0) {
        printf("unrecognized switch %s\n",x);
    }
    ar_mod |= AR_MOD_HELP;
    break;

  default:
    printf("Warning: %c unrecognized switch - ignored\n",x[0]);
  };
}

void output_help(void) {
    puts("Usage: ar [-]{dmpqrstx}[abcfilNoPsSuvV] archive-file file...");
    puts("Simulates the ar command by using the VMS librarian commands.");
    puts(" commands\n\
  d            - delete file(s) from the archive\n\
  m[ab]        - move file(s) in the archive\n\
  p            - print file(s) found in the archive\n\
  q[f]         - quick append file(s) to the archive same as 'r' on OpenVMS\n\
  r[ab][f][u]  - replace existing or insert new file(s) into the archive\n\
  s            - act as ranlib - write object file index into archive\n\
  t            - display contents of archive\n\
  x[o]         - extract file(s) from the archive\n\
 command specific modifiers:\n\
  [a]          - put file(s) after [member-name].  Ignored on OpenVMS\n\
  [b]          - put file(s) before [member-name] (same as [i])\n\
                 Ignored on OpenVMS\n\
  [N]          - use instance [count] of name.  Ignored on OpenVMS\n\
  [f]          - truncate inserted file names\n\
  [P]          - use full path names when matching.  Ignored on OpenVMS\n\
  [o]          - preserve original dates.  Ignored on OpenVMS\n\
  [u]          - only replace files that are newer than current archive\
 contents\n\
                 Ignored on OpenVMS\n\
 generic modifiers:\n\
  [c]          - do not warn if the library had to be created\n\
  [s]          - create an archive index (cf. ranlib)\n\
  [S]          - do not build a symbol table.  Ignored on OpenVMS.\n\
  [v]          - be verbose\n\
  [V]          - display the version number");

puts("\n  environment variables:\n\
  GNV_SUFFIX_MODE        (vms, unix)\n\
      When set to \"vms\", object files have type of .obj instead of .o\n\
  GNV_AR_DEBUG           (1, 0)\n\
      When enabled, temporary comand files are not deleted.\n");

}

int main(int argc, char *argv[])
{
  int i = 1;
  int first=1;
  char *gnv_suffix_mode;

  gnv_ar_debug=(NULL!=getenv("GNV_AR_DEBUG"));

  gnv_suffix_mode     = getenv("GNV_SUFFIX_MODE");
  if (gnv_suffix_mode && !strcasecmp(gnv_suffix_mode, "vms"))
    obj_suffix = ".obj";
  else
    obj_suffix = ".o";

  /* Special case. If the first argument is not a switch,
     each letter in that string is treated as a switch */

  if ((argv[1]!= NULL) && (argv[1][0] != '-')) {
    for (i = 0; i<strlen(argv[1]); i++)
      handle_switch(&argv[1][i]);

    first = 2;
  }

  for (i=first; i < argc; i++) {
    if (argv[i][0]=='-') {
      handle_switch(&argv[i][1]);
    }
    else {
      if ((relpos == NULL) && (ar_mod & (AR_MOD_AFTER | AR_MOD_BEFORE))) {
	relpos = argv[i];
      }
      else if ((count_str == NULL) && (ar_mod & AR_MOD_COUNT)) {
	count_str = argv[i];
      }
      else if (libname == NULL) {
	libname = argv[i];
      }
      else
	append_list(&objfiles_list_last,argv[i]);
    }
  }

  if (ar_mod & AR_MOD_VERSION)
    printf("GNV %s %s-%s %s %s\n", "ar",
                   PACKAGE_VERSION, VMS_ECO_LEVEL, __DATE__, __TIME__);

  if (ar_mod & AR_MOD_HELP) {
      printf("GNV %s %s\n", __DATE__, __TIME__);
      output_help();
  }

  /*
   *  Check things controlable through environment variables after
   *  processing command line in case it has been overridden.
   */

  if (gnv_ar_debug)
    remove_temps = FALSE;

  if (libname != NULL) {
    do_ar_work();
  }
  exit(0);
}
