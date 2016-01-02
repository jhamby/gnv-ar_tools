#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define GNV_ERROR_STATUS 0x10000002

#define MAX_READ_BUFF	4096
#define MAX_FILENAME_BUF 4096
#define MAX_CMD_SIZE	4096
#define DEFAULT_VMS_EXE		"a_out.exe"
#define DEFAULT_UNIX_EXE	"a.out"

#define DIM(__table)	(sizeof(__table)/sizeof(__table[0]))

extern int remove_temps;

int test_status_and_exit(int i);
void output_file(FILE *fhandle, char *name, int delete_on_close);
void output_file_to_handle(int fhandle, char *name, int delete_on_close);
char *vms_to_unix (const char *);
char *unix_to_vms(const char *str,int is_dir);
char *unix_to_vms_exp(const char *str,int is_dir);
char *fix_quote(const char *str);

enum suffix_type { 
  suffix_unknown,
  suffix_c, 
  suffix_cxx, 
  suffix_lib, 
  suffix_shlib, 
  suffix_obj 
};

struct suffix {
  const char *str;
  enum suffix_type type;
};

extern struct suffix suffixes[];

char *new_suffix2(char *ret,const char *str, int suf_ptr, 
		  const char *new_suffix);
char *new_suffix(const char *str, int suf_ptr, const char *new_suffix);
char *new_suffix3(const char *str, const char *new_suffix);
const char *get_suffix(const char *str);
int test_suffix(const char *str, unsigned int slen, const char *suffix);
enum suffix_type filename_suffix_type(const char *name, int len);
int enabled (const char *env_name);
void errmsg( const char *msg, ...);

typedef struct list {
  const char *str;
  struct list *next;
} *list_t;

void append_list(list_t **last_ptr, const char *str);
void print_list(list_t lis);

int file_exists(const char *path);


FILE *fopen_tmp(char *fname_buf,
		char *file_pat, 
		int in_tmp_dir, 
		const char *mode);

FILE *fopen_com_out(char *fname_com,
		    char *fname_out,
		    char *file_pat, 
		    int in_tmp_dir, 
		    const char *mode);

int str_spawn(char *cmdproc, char *outfile);
int run_cmdproc(char *cmdproc, char *outfile);

void init_utils(const char *pname);

extern char *lib_action_rtn_name;
unsigned long lib_list (char *name, unsigned long (*artn)() );

#endif
