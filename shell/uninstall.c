/* 
  Copyright (C) 2001 Pawel A. Gajda (mis@k2.net.pl)
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License published by
  the Free Software Foundation (see file COPYING for details).
*/

/*
  $Id$
*/

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <trurl/nassert.h>
#include <trurl/narray.h>

#include "log.h"
#include "pkg.h"
#include "pkgset.h"
#include "misc.h"
#include "shell.h"


static error_t parse_opt(int key, char *arg, struct argp_state *state);
static int uninstall(struct cmdarg *cmdarg);


#define OPT_UNINST_NODEPS  2
#define OPT_UNINST_FORCE   3

static struct argp_option options[] = {
{"mercy", 'm', 0, OPTION_HIDDEN, "Be tolerant for bugs which RPM tolerates", 1},
{"force", OPT_UNINST_FORCE, 0, 0, "Be unconcerned", 1 },
{"test", 't', 0, 0, "Don't uninstall, but tell if it would work or not", 1 },
{"nodeps", OPT_UNINST_NODEPS, 0, 0,
 "Ignore broken dependencies", 1 },
{0,  'v', "v...", OPTION_ARG_OPTIONAL, "Be more (and more) verbose.", 1 },
{ 0, 0, 0, 0, 0, 0 },
};


struct command command_uninstall = {
    0, 

    "uninstall", "PACKAGE...", "Uninstall packages", 
    
    options, parse_opt,
    
    NULL, uninstall, NULL, NULL, NULL, NULL
};



static
int uninstall_pkgs(tn_array *pkgnevrs, struct inst_s *inst) 
{
    char **argv;
    char *cmd;
    int i, n, nopts = 0, ec;

    for (i=0; i<n_array_size(pkgnevrs); i++) 
        msg(1, "U %s\n", n_array_nth(pkgnevrs, i));
    
    msg(1, "Uninstalling %d package%s\n", n_array_size(pkgnevrs),
        n_array_size(pkgnevrs) > 1 ? "s" : "");
    
    n = 128 + n_array_size(pkgnevrs);
    
    argv = alloca((n + 1) * sizeof(*argv));
    argv[n] = NULL;
    
    n = 0;
    
    if (inst->instflags & PKGINST_TEST) {
        cmd = "/bin/rpm";
        argv[n++] = "rpm";
        
    } else if (inst->flags & INSTS_USESUDO) {
        cmd = "/usr/bin/sudo";
        argv[n++] = "sudo";
        argv[n++] = "/bin/rpm";
        
    } else {
        cmd = "/bin/rpm";
        argv[n++] = "rpm";
    }
    
    argv[n++] = "-e";

    for (i=1; i<verbose; i++)
        argv[n++] = "-v";

    if (inst->instflags & PKGINST_TEST)
        argv[n++] = "--test";
    
    if (inst->instflags & PKGINST_FORCE)
        argv[n++] = "--force";
    
    if (inst->instflags & PKGINST_NODEPS)
        argv[n++] = "--nodeps";

#if 0    
    if (inst->rpmacros) 
        for (i=0; i<n_array_size(inst->rpmacros); i++) {
            argv[n++] = "--define";
            argv[n++] = n_array_nth(inst->rpmacros, i);
        }
#endif
    
    if (inst->rpmopts) 
        for (i=0; i<n_array_size(inst->rpmopts); i++)
            argv[n++] = n_array_nth(inst->rpmopts, i);
    
    nopts = n;
    for (i=0; i<n_array_size(pkgnevrs); i++) 
        argv[n++] = n_array_nth(pkgnevrs, i);
        
    n_assert(n > nopts); 
    argv[n++] = NULL;

    if (verbose > 0) {
        char buf[1024], *p;
        p = buf;
        
        for (i=0; i<nopts; i++) 
            p += snprintf(p, &buf[sizeof(buf) - 1] - p, " %s", argv[i]);
        *p = '\0';
        msg(1, "Running%s...\n", buf);
    }
    

#if ! USE_P_OPEN    
    ec = exec_rpm(cmd, argv);
    
#else 
    p_st_init(&pst);
    if (p_open(&pst, cmd, argv) == NULL) 
        return 0;
    
    process_rpm_output(&pst);
    if ((ec = p_close(&pst) != 0) && (inst->instflags & PKGINST_TEST) == 0)
        log(LOGERR, "%s", pst.errmsg);

    p_st_destroy(&pst);
#endif
    
    return ec == 0;
}


static
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct cmdarg *cmdarg = state->input;

    arg = arg;
    
    switch (key) {
        case OPT_UNINST_NODEPS:
            cmdarg->sh_s->inst->instflags  |= PKGINST_NODEPS;
            break;
            
        case OPT_UNINST_FORCE:
            cmdarg->sh_s->inst->instflags |= PKGINST_FORCE;
            break;
            
        case 't':
            cmdarg->sh_s->inst->instflags |= PKGINST_TEST;
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    
    return 0;
}


static
int shpkg_cmp_rm_uninstalled(struct shpkg *p1, struct shpkg *p2) 
{
    p2 = p2;
    
    if (p1->flags & SHPKG_UNINSTALL)
        return 0;
    
    return -1;
}


static int uninstall(struct cmdarg *cmdarg) 
{
    tn_array *shpkgs = NULL, *pkgnevrs = NULL;
    int i, err = 0;

    if (cmdarg->sh_s->instpkgs == NULL) {
        log(LOGERR, "uninstall: installed packages not loaded, "
            "type \"reload\" to load them\n");
        return 0;
    }
    
    sh_resolve_packages(cmdarg->pkgnames, cmdarg->sh_s->instpkgs, &shpkgs, 1);
    if (shpkgs == NULL || n_array_size(shpkgs) == 0) {
        err++;
        goto l_end;
    }

    if (shpkgs == cmdarg->sh_s->instpkgs) {
        log(LOGERR, "uninstall: better do \"rm -rf /\"\n");
        return 0;
    }
    
    if (err) 
        goto l_end;
    
    pkgnevrs = n_array_new(n_array_size(shpkgs), NULL, (tn_fn_cmp)strcmp);
    
    for (i=0; i<n_array_size(shpkgs); i++) {
        struct shpkg *shpkg = n_array_nth(shpkgs, i);
        
        shpkg->flags |= SHPKG_UNINSTALL;
        n_array_push(pkgnevrs, shpkg->nevr);
    }
    
    if (uninstall_pkgs(pkgnevrs, cmdarg->sh_s->inst))
        n_array_remove_ex(cmdarg->sh_s->instpkgs, NULL,
                          (tn_fn_cmp)shpkg_cmp_rm_uninstalled);
    else
        err = 1;
    
 l_end:
    if (pkgnevrs != NULL)
        n_array_free(pkgnevrs);

    if (shpkgs && shpkgs != cmdarg->sh_s->instpkgs) 
        n_array_free(shpkgs);
    
    return err == 0;
}
