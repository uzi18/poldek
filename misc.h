/* $Id$ */
#ifndef POLDEK_MISC_H
#define POLDEK_MISC_H

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <vfile/p_open.h>

int mhexdigest(FILE *stream, unsigned char *mdhex, int *mdhex_size);
int mdigest(FILE *stream, unsigned char *md, int *md_size);


/*
  Returns $TMPDIR or "/tmp" if $TMPDIR isn't set.
  Returned dir always begin with '/'
*/
const char *tmpdir(void);


void die(void);

char *trimslash(char *path);
char *next_token(char **str, char delim, int *toklen);
int is_rwxdir(const char *path);
int is_dir(const char *path);
int mk_dir(const char *path, const char *dn);

extern int mem_info_verbose;
void print_mem_info(const char *prefix);
void mem_info(int level, const char *msg);

void process_cmd_output(struct p_open_st *st, const char *prefix);
int lockfile(const char *lockfile);
pid_t readlockfile(const char *lockfile);

#endif /* POLDEK_MISC_H */
