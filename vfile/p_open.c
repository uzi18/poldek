#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRSIGNAL
# define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>



#include "p_open.h"

void p_st_init(struct p_open_st *pst) 
{
    pst->stream = NULL;
    pst->pid = 0;
    pst->cmd = NULL;
    pst->errmsg = NULL;
}

void p_st_destroy(struct p_open_st *pst) 
{
    if (pst->stream) {
        fclose(pst->stream);
        pst->stream = NULL;
    }
    
    if (pst->cmd) {
        free(pst->cmd);
        pst->cmd = NULL;
    }
    
    if (pst->errmsg) {
        free(pst->errmsg);
        pst->errmsg = NULL;
    }
}


FILE *p_open(struct p_open_st *pst, unsigned flags, const char *cmd,
             char *const argv[])
{
    int    pp[2];
    pid_t  pid;
    char   errmsg[1024];
    
    if (access(cmd, X_OK) != 0) {
        snprintf(errmsg, sizeof(errmsg), "%s: no such file", cmd);
        pst->errmsg = strdup(errmsg);
        return NULL;
    }
    
    if (pipe(pp) != 0) {
        snprintf(errmsg, sizeof(errmsg), "%s: no such file", cmd);
        pst->errmsg = strdup(errmsg);
        return NULL;
    }
    
    if ((pid = fork()) == 0) {
        
        if ((flags & P_OPEN_KEEPSTDIN) == 0) {
            int fd = open("/dev/null", O_RDWR);
            dup2(fd, 0);
            close(fd);
        }
        
        close(pp[0]);

	dup2(pp[1], 1);
	dup2(pp[1], 2);
	close(pp[1]);

        execv(cmd, argv);
	exit(EXIT_FAILURE);
        
    } else if (pid < 0) {
        snprintf(errmsg, sizeof(errmsg), "%s: no such file", cmd);
        pst->errmsg = strdup(errmsg);
        return NULL;
        
    } else {
        close(pp[1]);
        pst->fd = pp[0];
        pst->stream = fdopen(pp[0], "r");
        setvbuf(pst->stream, NULL, _IONBF, 0);
        pst->pid = pid;
        pst->cmd = strdup(cmd);
    }
    
    return pst->stream;
}


int p_close(struct p_open_st *pst) 
{
    int st, rc = -1;
    char errmsg[1024];

    pst->errmsg = NULL;
    
    if (pst->pid == 0)
        return 0;
    
    waitpid(pst->pid, &st, 0);
    
    
    if (WIFEXITED(st)) {
        if (WEXITSTATUS(st) != 0) {
            snprintf(errmsg, sizeof(errmsg), "%s exited with %d\n", pst->cmd,
                     WEXITSTATUS(st));
            pst->errmsg = strdup(errmsg);
        }
        rc = WEXITSTATUS(st);
        
    } else if (WIFSIGNALED(st)) {
#ifdef HAVE_STRSIGNAL
        snprintf(errmsg, sizeof(errmsg), "%s terminated by signal %s\n",
                 pst->cmd, strsignal(WTERMSIG(st)));
#else
        snprintf(errmsg, sizeof(errmsg), "%s terminated by signal %d\n",
                 pst->cmd, WTERMSIG(st));
#endif        
        pst->errmsg = strdup(errmsg);
        
    } else {
        snprintf(errmsg, sizeof(errmsg),
                 "have no idea what happen with %s(%d)\n", pst->cmd, pst->pid);
        pst->errmsg = strdup(errmsg);
    }
    
    return rc;
}

#if ENABLE_PTYOPEN
#include <pty.h>
FILE *pty_open(struct p_open_st *pst, const char *cmd, char *const argv[])
{
    int fd;
    pid_t pid;
    char errmsg[1024];
    
    if (access(cmd, X_OK) != 0) {
        snprintf(errmsg, sizeof(errmsg), "%s: no such file", cmd);
        pst->errmsg = strdup(errmsg);
        return NULL;
    }
    
    if ((pid = forkpty(&fd, NULL, NULL, NULL)) == 0) {
        execv(cmd, argv);
	exit(EXIT_FAILURE);
        
    } else if (pid < 0) {
        snprintf(errmsg, sizeof(errmsg), "%s: no such file", cmd);
        pst->errmsg = strdup(errmsg);
        return NULL;
        
    } else {
        pst->fd = fd;
        pst->stream = fdopen(fd, "r");
        setvbuf(pst->stream, NULL, _IONBF, 0);
        pst->pid = pid;
        pst->cmd = strdup(cmd);
    }
    
    return pst->stream;
}
#endif
