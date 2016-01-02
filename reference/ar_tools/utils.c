/* utils.c

<Copyright notice>

Authors:	Ken Block
		Dave Faulkner	Compaq Computer Corp

Description:
		Utility functions for use with cc.c.


Modification history:
	01	Ken Block
		Original work

	02	Dave Faulkner		May-2001
		Added routine errmsg() to print exception messages,
		and enabled() to test the value of an environment variable
		and return TRUE if defined and not set to a disable value
		or NULL string.

	04	Steve Pitcher		28-Mar-2003
		In filename_suffix_type, it was finding "c", when it was
                supposed to find "cc".  It was finding a shorter match.
		Fix by re-ordering the table of suffixes, to have the longer
                ones first.

	05	Colin Blake		06-Jun-2003
		Move lib_list from cc to here since its now used by ar too.

	06	John Malmberg		03-Nov-2005
		Change unix_to_vms_exp to handle logical name search lists
		correctly.

	07	John Malmberg		05-Jun-2006
		Change unix_to_vms to append SYS$DISK:[] to files with out
		directory specifications.  This is to prevent sticky defaults.

	08	Karl Puder		23-Apr-2007
		Change unix_to_vms to notice when "^UP^path" happens, and do not
		prepend any other SYS$DISK:[] stuff.

	09	John Malmberg
		Do not prepend [] to logname:filename specifications.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unixlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <ssdef.h>
#include <fabdef.h>
#include <namdef.h>
#include <rmsdef.h>
#ifdef __VAX
#define SYS$PARSE sys$parse
#define SYS$SEARCH sys$search
#endif
#include <starlet.h>

/* For spawn */
#include <lib$routines.h>
#include <descrip.h>

#include "utils.h"

#include <lbrdef.h>
#include <lbr$routines.h>
#include <stsdef.h>

const char *program_name;
int remove_temps = TRUE;

/*
 * Warning... This table needs to have the longer suffixes at the beginning, and
 * the shorter ones at the end... Otherwise, it find "c", when looking for "cc".
 */
struct suffix suffixes[] = {
    { "cxx", suffix_cxx },
    { "cpp", suffix_cxx },
    { "hxx", suffix_cxx },
    { "obj", suffix_obj },
    { "lib", suffix_lib },
    { "exe", suffix_shlib },
    { "cc", suffix_cxx },
    { "so", suffix_shlib },

    { "c", suffix_c },
    { "h", suffix_c },

    { "C", suffix_cxx },

    { "o", suffix_obj },

    { "a", suffix_lib }

};

void init_utils(const char *pname) {

    program_name = pname;
}

void errmsg(const char *msg, ...)
{
    va_list ap;
    char buf[256];

    sprintf(buf, "? %s: %s\n", program_name, msg);
    va_start(ap, msg);
    vprintf(buf, ap);
}

/*
 * Returns TRUE if variable defined and not FALSE.
 * Null string is treated as FALSE.
 */

int enabled(const char *var)
{
    static char *disabled[] = { "0", "F", "N", "NO" , "FALSE",
				"DISABLED", NULL };
    char buff[5];
    char *ptr, *qtr;
    int i;
    int default_ret = TRUE;	/* assume TRUE */
    char buf[5];
    char **pval;
    int ret;

    ptr = getenv(var);

    if (!ptr)
	return FALSE;

    /* Ignore leading spaces */
    for (ptr; *ptr && isspace(*ptr); ptr++)
	;

    /* Convert to uppercase up to 1st space*/
    for (qtr = buff, i = 0; *ptr && !isspace(*ptr) && i < 4; ptr++, qtr++, i++)
	*qtr = toupper(*ptr);

    *qtr = '\0';

    /* Null string is same as FALSE */
    if (!*buff)
        return FALSE;

    /*
    ** Compare the string against the first 4 bytes in each of the two tables
    ** for enabled or disabled.
    */
    for (pval = disabled, ret = FALSE; *pval; pval++) {
	for (ptr = buff, qtr = *pval; TRUE; ptr++, qtr++) {
	    if (!*ptr) {				/* End of string */
		if (!*qtr || (ptr - buff) == 4) { /* Exactly equal or
						     matched on 1st 4 chars */
		    return ret;
		}
		break;
	    }
	    if (*ptr != *qtr) {
		break;
	    }
	}
    }

    return default_ret;
}

int test_status_and_exit(int i) {

    switch ( i & 7) {

        case 0:		/* warning */
	    if (getenv("GNV_CC_WARNING_IS_ERROR"))
		exit(GNV_ERROR_STATUS);
	    else
		return 0;
        case 1:		/* normal */
            return 0;

        case 2:		/* error */
	    exit(GNV_ERROR_STATUS);

        case 3:		/* Informational */
            return 0;

        case 4:

        case 5:

        case 6:

        case 7:		/* fatal */
	    exit(GNV_ERROR_STATUS);
    }
}

void output_file(FILE *fhandle, char *name, int delete_on_close) {
    int count = 0;
    int ch;
    FILE *fp = fopen(name, "r");

    if (!fp)
	return ;

    /*
     * fgetc has a yucking error reporting mechanism. On error, it will
     * return EOF. You need to check errno to be sure no error has occured.
     * Also, errno needs to set reset to zero, otherwise we might pickup
     * a previous error.
     */

    errno = 0;

    while ((ch = fgetc(fp)) != EOF) {
        fputc(ch, fhandle);

        if (ch == '\n')
	    fflush(fhandle);
    }

    if (errno) {
        perror(program_name);
        fclose(fhandle);
        exit(1);
    }

    fclose(fp);

    if (delete_on_close)
        remove(name);
}

void output_file_to_handle(int fhandle, char *name, int delete_on_close) {
    char buffer[MAX_READ_BUFF];
    int size;

    int fd = open(name, O_RDONLY);

    if (fd < 0)
	return;

    while ((size = read(fd, buffer, sizeof(buffer))) > 0)
        write(fhandle, buffer, size);

    if (size)
	perror(program_name);

    close(fd);

    if (delete_on_close)
        remove(name);
}

char unix_name[MAX_FILENAME_BUF];
char unsticky_name[MAX_FILENAME_BUF];

int action(char *name, int type) {

    strcpy(unix_name, name);
    return 1;
}

int action1 (char *name)
{
return action (name, 0);
}

char *vms_to_unix (const char *str)
{
    int res = decc$from_vms (str, action1, 0);

    if (!res)
	    return strdup (str);
	else
	    return strdup (unix_name);
}

char *unix_to_vms(const char *str, int is_dir) {
int res;

    /* VMS has sticky file specifications, UNIX does not */
    /* So make sure that there is a directory specfication */

    res = decc$to_vms(str, action, 0, is_dir ? 2 : 0);

    if (!res)
        return strdup(str);

    if (strstr(unix_name,":[") != NULL)
        return strdup(unix_name);

    if (strstr(unix_name,":<") != NULL)
        return strdup(unix_name);

    if (0 == strncmp(unix_name, "\"^UP^", 5))
	return strdup(unix_name);

    /* Need to fix up to prevent sticky defaults */
    unsticky_name[0] == 0;

    /* Must have a device */
    if (strchr(unix_name,':') == NULL)
	strcpy(unsticky_name, "SYS$DISK:");

    /* Must have a device or directory logname:file or [dir]file */
    if (strpbrk(unix_name,":[<") == NULL)
	strcat(unsticky_name, "[]");

    strcat(unsticky_name, unix_name);
    return strdup(unsticky_name);
}

/* You need to use the nam$l_rsa for search lists to work */
char *unix_to_vms_exp(const char *str, int is_dir) {

    if (str[0] == '/')
        return unix_to_vms(str, is_dir);
    else {
        char *p;
        p = unix_to_vms(str, is_dir);
	if (p) {
            struct FAB fab = cc$rms_fab;
            struct NAM nam = cc$rms_nam;
            char esa[NAM$C_MAXRSS+1];
            char rsa[NAM$C_MAXRSS+1];
	    long s;

            fab.fab$l_fna = p;
            fab.fab$b_fns = strlen(p);
            fab.fab$l_nam = &nam;
            fab.fab$l_fop = FAB$M_NAM;

#ifdef NAM$M_NO_SHORT_UPCASE
	    nam.nam$b_nop = NAM$M_NO_SHORT_UPCASE; /* Preserve case */
#endif
            nam.nam$l_esa = esa;
            nam.nam$b_ess = sizeof(esa)-1;
            nam.nam$l_rsa = rsa;
            nam.nam$b_rss = sizeof(rsa)-1;
            s = SYS$PARSE(&fab, 0, 0);
            if (s & STS$K_SUCCESS) {
		esa[nam.nam$b_esl] = '\0';
		nam.nam$l_esa = NULL;
		s = SYS$SEARCH(&fab, NULL, NULL);
		if (s & STS$K_SUCCESS) {
		    free(p);
		    rsa[nam.nam$b_rsl] = '\0';
		    p = strdup(rsa);
		}
		else {
		    free(p);
		    p = strdup(esa);
		}

		/* Clean up after SYS$PARSE */
		nam.nam$b_nop = NAM$M_SYNCHK;
		nam.nam$l_rlf = NULL;
		fab.fab$b_dns = 0;
		s = sys$parse(&fab, NULL, NULL);
            }
	    else
	        printf("Parse returned %d on %s\n",s,p);
	}
        return p;
    }
}

char *fix_quote(const char *str) {
    char output[1024];
    char *optr;
    int i = 0;

    for (optr = output; *str; ) {
        if (*str == '\"') {
            *optr++ = '\"';
            *optr++ = '\"';
	    str++;
	}
	else
	    *optr++ = *str++;
    }

    *optr = '\0';

    return strdup(output);
}

enum suffix_type filename_suffix_type(const char *name, int len) {
    int i;

    for (i = 0; i < sizeof(suffixes) / sizeof(struct suffix); i++) {
        if (test_suffix(name, len, suffixes[i].str))
            return suffixes[i].type;
    }

    return suffix_unknown;
}

const char *get_suffix(const char *str) {
    int len = strlen(str);
    const char *ptr;

    for (ptr = str + len - 1; ptr >= str; --ptr) {
	if (*ptr == '/') {
	    /* Null Suffix */
	    return &str[len];
	}
	if (*ptr == '.')
	    return ptr;
    }
    return &str[len];
}

char *new_suffix2(char *ret, const char *str, int suf_ptr,
                  const char *new_suf) {
    strncpy(ret, str, suf_ptr);
    strcpy(&ret[suf_ptr], new_suf);

    return ret;
}

char *new_suffix(const char *str, int suf_ptr, const char *new_suf) {
    char *ret = malloc(suf_ptr + strlen(new_suf) + 1);

    return new_suffix2(ret, str, suf_ptr, new_suf);
}

char *new_suffix3(const char *str, const char *new_suf) {
    const char *suf = get_suffix(str);

    return new_suffix(str, suf - str, new_suf);
}

void append_list(list_t **last_ptr, const char *str) {

    **last_ptr = (list_t)malloc(sizeof(struct list));
    (**last_ptr)->next = NULL;
    (**last_ptr)->str = str;

    *last_ptr = &(**last_ptr)->next;
}

void print_list(list_t lptr) {
    while (lptr) {
        printf("%s\n", lptr->str);
        lptr = lptr->next;
    }
}

int file_exists(const char *path) {
    int fd;

    fd = open(path, O_RDONLY);

    if (fd >= 0) {
        close(fd);
        return TRUE;
    }

    return FALSE;
}

int test_suffix(const char *str, unsigned int slen, const char *suffix) {

    return !strcmp(&str[slen - strlen(suffix)], suffix);
}

FILE *fopen_tmp(char *fname_buf,
                char *file_pat,
                int in_tmp_dir,
                const char *mode) {
    int fd;
    FILE *fp;

    sprintf(fname_buf, "%sXXXXXX", file_pat);

    fd = mkstemp(fname_buf);

    if (fd < 0) {
        perror("mkstemp failed to create temp file");
	exit(GNV_ERROR_STATUS);
    }

    strcat(fname_buf, ".");

    fp = fdopen(fd, (char*) mode);

    return fp;
}

FILE *fopen_com_out(char *fname_com,
                    char *fname_out,
                    char *file_pat,
                    int in_tmp_dir,
                    const char *mode) {
    int len;
    FILE *fp;

    fp = fopen_tmp(fname_com, file_pat, in_tmp_dir, mode);
    strcpy(fname_out, fname_com);

    strcat(fname_out, "out");

#if 0
    fprintf(fp, "$define sys$output %s\n", fname_out);

    fprintf(fp, "$define sys$error %s\n", fname_out);

#endif

    return fp;
}

int str_spawn(char *cmdproc, char *outfile) {
    struct dsc$descriptor_s cmddsc;
    struct dsc$descriptor_s outdsc;

    struct dsc$descriptor_s *outdsc_ptr = NULL;
    int status;
    int s_status;

    cmddsc.dsc$w_length = strlen(cmdproc);
    cmddsc.dsc$a_pointer = cmdproc;
    cmddsc.dsc$b_dtype = DSC$K_DTYPE_T;
    cmddsc.dsc$b_class = DSC$K_CLASS_S;

    if (outfile) {
        outdsc.dsc$w_length = strlen(outfile);
        outdsc.dsc$a_pointer = outfile;
        outdsc.dsc$b_dtype = DSC$K_DTYPE_T;
        outdsc.dsc$b_class = DSC$K_CLASS_S;

        outdsc_ptr = &outdsc;
    }

    s_status = lib$spawn(&cmddsc, 0, outdsc_ptr, 0, 0, 0, &status);

    if (!(s_status & 1)) {
        errmsg("Problem spawning");
	exit(GNV_ERROR_STATUS);
    }

    return status;
}

int run_cmdproc(char *cmdproc, char *outfile) {
    int ret;
    char buffer[MAX_CMD_SIZE];

    sprintf(buffer, "@%s", cmdproc);

    ret = str_spawn(buffer, outfile);

    return ret;
}

/*
** lib_list opens an object library and obtains a list of all the module
** names. For each module, artn is called.
**
** Since I could not see a way of passing any context in to the action
** routine the name of the object library file is passed in via the
** global lib_action_rtn_name.
*/

char *lib_action_rtn_name;

unsigned long lib_list (char *name, unsigned long (*artn)() )
{
    unsigned long status, lib_index, lib_func, lib_type, lib_index_number;
    struct dsc$descriptor lib_name_d;

    lib_action_rtn_name = name; /* needed by artn */

    lib_func = LBR$C_READ;
#ifdef __ia64
    lib_type = LBR$C_TYP_ELFOBJ;
#else
#ifdef __alpha
    lib_type = LBR$C_TYP_EOBJ;
#else
#ifdef __vax
    lib_type = LBR$C_TYP_OBJ;
#else
#error Unknown architecture
    lib_type =-1;
#endif
#endif
#endif
    status = lbr$ini_control(&lib_index, &lib_func, &lib_type);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    lib_name_d.dsc$w_length = strlen(name);
    lib_name_d.dsc$b_dtype = DSC$K_DTYPE_T;
    lib_name_d.dsc$b_class = DSC$K_CLASS_S;
    lib_name_d.dsc$a_pointer = name;
    status = lbr$open(&lib_index, &lib_name_d, 0, 0, 0, 0, 0);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    lib_index_number = 1;
    status = lbr$get_index(&lib_index, &lib_index_number, artn, 0);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    status = lbr$close(&lib_index);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    return 1;
}
