/* 
  Copyright (C) 2000, 2001 Pawel A. Gajda (mis@k2.net.pl)
 
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

#include <trurl/trurl.h>
#include "sigint/sigint.h"

#include "i18n.h"
#include "log.h"
#include "pkg.h"
#include "pkgset.h"
#include "misc.h"
#include "cli.h"
#include "op.h"

static error_t parse_opt(int key, char *arg, struct argp_state *state);
static error_t cmdl_parse_opt(int key, char *arg, struct argp_state *state);
static int install(struct cmdctx *cmdctx);

#define OPT_GID             1500
#define OPT_INST_NODEPS     (OPT_GID + 1)
#define OPT_INST_FORCE      (OPT_GID + 2)
#define OPT_INST_REINSTALL  (OPT_GID + 3)
#define OPT_INST_DOWNGRADE  (OPT_GID + 4)
#define OPT_INST_INSTDIST   (OPT_GID + 5)
#define OPT_INST_UPGRDIST   (OPT_GID + 6)
#define OPT_INST_REINSTDIST (OPT_GID + 7)

#define OPT_INST_FETCH      (OPT_GID + 16)

#define OPT_INST_JUSTDB           (OPT_GID + 17)
#define OPT_INST_TEST             't'
#define OPT_INST_RPMDEF           (OPT_GID + 19)
#define OPT_INST_DUMP             (OPT_GID + 20)
#define OPT_INST_DUMPN            (OPT_GID + 21)
#define OPT_INST_NOFOLLOW         'N'
#define OPT_INST_FRESHEN          'F'
#define OPT_INST_HOLD             (OPT_GID + 24)
#define OPT_INST_NOHOLD           (OPT_GID + 25)
#define OPT_INST_IGNORE           (OPT_GID + 26)
#define OPT_INST_NOIGNORE         (OPT_GID + 27)
#define OPT_INST_GREEDY           'G'
#define OPT_INST_UNIQNAMES        'Q'
#define OPT_INST_UNIQNAMES_ALIAS  (OPT_GID + 28)
#define OPT_INST_ROOTDIR          'r' 
#define OPT_MERCY                  (OPT_GID + 29)
#define OPT_PROMOTEEPOCH           (OPT_GID + 30)
#define OPT_PMONLY_NODEPS         (OPT_GID + 31)
#define OPT_PMONLY_FORCE          (OPT_GID + 32)
#define OPT_PM                    (OPT_GID + 33)
#define OPT_INST_NOFETCH          (OPT_GID + 34)

static struct argp_option options[] = {
{0, 'I', 0, 0, N_("Install, not upgrade packages"), OPT_GID },
{"reinstall", OPT_INST_REINSTALL, 0, 0, N_("Reinstall"), OPT_GID }, 
{"downgrade", OPT_INST_DOWNGRADE, 0, 0, N_("Downgrade"), OPT_GID },
{"force", OPT_INST_FORCE, 0, 0, N_("Be unconcerned"), OPT_GID },
{"test", 't', 0, 0, N_("Don't install, but tell if it would work or not"),
     OPT_GID },
{"fresh", 'F', 0, 0, N_("Upgrade packages, but only if an earlier version "
                        "currently exists"), OPT_GID },
{"nofollow", 'N', 0, 0, N_("Don't install packages required by "
                           "selected ones"), OPT_GID },
{"greedy", 'G', 0, 0, N_("Automatically upgrade packages which dependencies "
                         "are broken by unistalled ones"), OPT_GID }, 
{"fetch", OPT_INST_FETCH, "DIR", OPTION_ARG_OPTIONAL,
     N_("Do not install, only download packages"), OPT_GID },

{"root", OPT_INST_ROOTDIR, "DIR", 0, N_("Set top directory to DIR"), OPT_GID },    

{"nodeps", OPT_INST_NODEPS, 0, 0,
 N_("Install packages with broken dependencies"), OPT_GID },

{"mercy", OPT_MERCY, 0, 0, N_("Treat requirements with EVR as satisfied by "
                              "capabilities without it (old RPM behaviour)"), OPT_GID},

{"promoteepoch", OPT_PROMOTEEPOCH, 0, 0,
     N_("Promote non-existent requirement's epoch to "
        "package's one (rpm < 4.2.1 behaviour)"), OPT_GID }, 
                                           

{"dump", OPT_INST_DUMP, "FILE", OPTION_ARG_OPTIONAL,
N_("Just dump install marked package filenames to FILE (default stdout)"), OPT_GID },

{"dumpn", OPT_INST_DUMPN, "FILE", OPTION_ARG_OPTIONAL,
N_("Just dump install marked package names to FILE (default stdout)"), OPT_GID },

{"justdb", OPT_INST_JUSTDB, 0, 0, N_("Modify only the database"), OPT_GID },
                                           
{"pm-nodeps", OPT_PMONLY_NODEPS, 0, 0, 
N_("Install packages with broken dependencies (applied to PM only)"), OPT_GID },

{"rpm-nodeps", 0, 0, OPTION_ALIAS | OPTION_HIDDEN, 0, OPT_GID },

{"pm-force", OPT_PMONLY_FORCE, 0, 0,
N_("Be unconcerned (applied to PM only)"), OPT_GID },

{"rpm-force", 0, 0, OPTION_ALIAS | OPTION_HIDDEN, 0, OPT_GID },
    
{"pmopt", OPT_PM, "OPTION", 0, 
 N_("pass option OPTION to PM binary"), OPT_GID },
{"rpm", 0, 0, OPTION_ALIAS | OPTION_HIDDEN, 0, OPT_GID },

{"nofetch", OPT_INST_NOFETCH, 0, OPTION_HIDDEN,
     N_("Do not download packages"), OPT_GID },    

{0,  'v', 0, 0, N_("Be verbose."), OPT_GID },
{NULL, 'h', 0, OPTION_HIDDEN, "", OPT_GID }, /* alias for -? */
{ 0, 0, 0, 0, 0, 0 },
};


static struct argp_option cmdl_options[] = {
    {0,0,0,0, N_("Package installation:"), OPT_GID - 100 },
    {"install", 'i', 0, 0, N_("Install given packages"), OPT_GID - 100 },
    {"reinstall", OPT_INST_REINSTALL, 0, 0, N_("Reinstall given packages"),
         OPT_GID - 100},
    {"downgrade", OPT_INST_DOWNGRADE, 0, 0, N_("Downgrade"), OPT_GID - 100 },     
    {"upgrade", 'u', 0, 0, N_("Upgrade given packages"), OPT_GID - 100 },
    {NULL, 'h', 0, OPTION_HIDDEN, "", OPT_GID - 100 }, /* for compat with -Uvh */
    
    {0,0,0,0, N_("Distribution installation/upgrade:"), OPT_GID - 90 },
    {"install-dist", OPT_INST_INSTDIST, "DIR", 0,
    N_("Install package set under DIR as root directory"), OPT_GID - 90 },

    {"upgrade-dist", OPT_INST_UPGRDIST, "DIR", OPTION_ARG_OPTIONAL,
     N_("Upgrade all packages needs upgrade"), OPT_GID - 90 },

    {"reinstall-dist", OPT_INST_REINSTDIST, "DIR", OPTION_ARG_OPTIONAL,
     N_("Reinstall all packages under DIR as root directory"), OPT_GID - 90 },

    {0,0,0,0, N_("Installation switches:"), OPT_GID },
{"hold", OPT_INST_HOLD, "PACKAGE[,PACKAGE]...", 0,
 N_("Prevent packages listed from being upgraded if they are already installed."),
     OPT_GID },
{"nohold", OPT_INST_NOHOLD, 0, 0,
 N_("Do not hold any packages"), OPT_GID },

{"ignore", OPT_INST_IGNORE, "PACKAGE[,PACKAGE]...", 0,
 N_("Make packages listed invisible."), OPT_GID },
    
{"noignore", OPT_INST_NOIGNORE, NULL, 0,
 N_("Make invisibled packages visible."), OPT_GID },

{"uniq", OPT_INST_UNIQNAMES, 0, 0, 
N_("Do sort | uniq on available package list"), OPT_GID },

{"unique-pkg-names", OPT_INST_UNIQNAMES_ALIAS, 0, OPTION_ALIAS | OPTION_HIDDEN,
     0, OPT_GID },     

    { 0, 0, 0, 0, 0, 0 },
};


struct poclidek_cmd command_install = {
    COMMAND_HASVERBOSE | COMMAND_MODIFIESDB |
    COMMAND_PIPEABLE_LEFT | COMMAND_PIPE_XARGS | COMMAND_PIPE_PACKAGES, 
    "install", N_("PACKAGE..."), N_("Install packages"), 
    options, parse_opt,
    NULL, install, NULL, NULL, NULL, NULL, 0
};

static struct argp cmd_argp = {
    options, parse_opt, 0, 0, 0, 0, 0
};

static struct argp_child cmd_argp_child[2] = {
    { &cmd_argp, 0, NULL, 0 },
    { NULL, 0, NULL, 0 },
};

static struct argp poclidek_argp = {
    cmdl_options, cmdl_parse_opt, 0, 0, cmd_argp_child, 0, 0
};

static 
struct argp_child poclidek_argp_child = {
    &poclidek_argp, 0, NULL, OPT_GID,
};


static int cmdl_run(struct poclidek_opgroup_rt *rt);

struct poclidek_opgroup poclidek_opgroup_install = {
    "Package installation", 
    &poclidek_argp, 
    &poclidek_argp_child,
    cmdl_run
};

struct cmdl_arg_s {
    struct cmdctx cmdctx;
};

static
error_t cmdl_parse_opt(int key, char *arg, struct argp_state *state)
{
    struct poclidek_opgroup_rt  *rt;
    struct poldek_ts            *ts;
    struct cmdl_arg_s           *arg_s;


    rt = state->input;
    ts = rt->ts;
    arg = arg;
    
    if (rt->_opdata != NULL) {
        arg_s = rt->_opdata;
        
    } else {
        arg_s = n_malloc(sizeof(*arg_s));
        memset(arg_s, 0, sizeof(*arg_s));
        arg_s->cmdctx.ts = rt->ts;
        rt->_opdata = arg_s;
    }
    
    switch (key) {
        case ARGP_KEY_INIT:
            state->child_inputs[0] = &arg_s->cmdctx;
            state->child_inputs[1] = NULL;
            break;

        case 'i':
            poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "install");
            poldek_ts_setf(ts, POLDEK_TS_INSTALL);
            break;
            
        case OPT_INST_DOWNGRADE:
        case OPT_INST_REINSTALL:
        case 'U':
        case 'u':
            poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "install");
            poldek_ts_setf(ts, POLDEK_TS_UPGRADE);
            
            if (key == OPT_INST_DOWNGRADE)
                poldek_ts_setf(ts, POLDEK_TS_DOWNGRADE);
            
            else if (key == OPT_INST_REINSTALL)
                poldek_ts_setf(ts, POLDEK_TS_REINSTALL);
            break;

        case OPT_INST_INSTDIST:
            poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "install-dist");
            poldek_ts_setf(ts, POLDEK_TS_DIST);
            if (arg)
                poldek_ts_configure(ts, POLDEK_CONF_ROOTDIR, arg);
            break;

        case OPT_INST_REINSTDIST:
            poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "reinstall-dist");
            poldek_ts_setf(ts, POLDEK_TS_DIST);
            poldek_ts_setf(ts, POLDEK_TS_UPGRADE);
            poldek_ts_setf(ts, POLDEK_TS_REINSTALL);
            if (arg)
                poldek_ts_configure(ts, POLDEK_CONF_ROOTDIR, arg);
            break;

        case OPT_INST_UPGRDIST:
            poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "upgrade-dist");
            poldek_ts_setf(ts, POLDEK_TS_DIST);
            poldek_ts_setf(ts, POLDEK_TS_UPGRADE);
            if (arg)
                poldek_ts_configure(ts, POLDEK_CONF_ROOTDIR, arg);
            break;

        case OPT_INST_HOLD:
            poldek_configure(ts->ctx, POLDEK_CONF_OPT, POLDEK_OP_HOLD, 1);
            poldek_configure(ts->ctx, POLDEK_CONF_HOLD, arg);
            break;
            
        case OPT_INST_NOHOLD:
            ts->setop(ts, POLDEK_OP_HOLD, 0);
            break;
            
        case OPT_INST_IGNORE:
            poldek_configure(ts->ctx, POLDEK_CONF_OPT, POLDEK_OP_IGNORE, 1);
            poldek_configure(ts->ctx, POLDEK_CONF_IGNORE, arg);
            break;

        case OPT_INST_NOIGNORE:
            ts->setop(ts, POLDEK_OP_IGNORE, 0);
            break;

        case OPT_INST_UNIQNAMES:
        case OPT_INST_UNIQNAMES_ALIAS:
            poldek_configure(ts->ctx, POLDEK_CONF_OPT, POLDEK_OP_UNIQN, 1);
            break;
    

        default:
            return ARGP_ERR_UNKNOWN;
            
    }
    
    return 0;
}

static
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct cmdctx         *cmdctx = state->input;
    struct poldek_ts      *ts;


    ts = cmdctx->ts;
    
    switch (key) {
        case ARGP_KEY_INIT:
            break;
            
        case OPT_MERCY:
            ts->setop(ts, POLDEK_OP_VRFYMERCY, 1);
            break;

        case OPT_PROMOTEEPOCH:
            ts->setop(ts, POLDEK_OP_PROMOTEPOCH, 1);
            break;
            
        case OPT_INST_NODEPS:
            ts->setop(ts, POLDEK_OP_NODEPS, 1);
            break;
            
        case OPT_INST_FORCE:
            ts->setop(ts, POLDEK_OP_FORCE, 1);
            break;
            
        case 't':
            if (ts->getop(ts, POLDEK_OP_TEST))
                ts->setop(ts, POLDEK_OP_RPMTEST, 1);
            else
                ts->setop(ts, POLDEK_OP_TEST, 1);
            break;
            
        case 'F':
            ts->setop(ts, POLDEK_OP_FRESHEN, 1);
            break;

        case OPT_INST_NOFOLLOW:
            ts->setop(ts, POLDEK_OP_FOLLOW, 0);
            break;

        case 'G':
            ts->setop(ts, POLDEK_OP_GREEDY, 1);
            break;
            
        case 'I':
            poldek_ts_setf(ts, POLDEK_TS_INSTALL);
            poldek_ts_clrf(ts, POLDEK_TS_UPGRADE);
            
            break;

        case OPT_INST_REINSTALL:
            poldek_ts_setf(ts, POLDEK_TS_REINSTALL);
            break;

        case OPT_INST_DOWNGRADE:
            poldek_ts_setf(ts, POLDEK_TS_DOWNGRADE);
            break;

        case OPT_INST_ROOTDIR:
            poldek_ts_configure(ts, POLDEK_CONF_ROOTDIR, arg);
            break;

        case OPT_INST_JUSTDB:
            ts->setop(ts, POLDEK_OP_JUSTDB, 1);
            break;

        case OPT_INST_DUMP:
            if (arg)
                poldek_ts_configure(ts, POLDEK_CONF_DUMPFILE, arg);
            ts->setop(ts, POLDEK_OP_JUSTPRINT, 1);
            break;

        case OPT_INST_DUMPN:
            if (arg)
                poldek_ts_configure(ts, POLDEK_CONF_DUMPFILE, arg);
            ts->setop(ts, POLDEK_OP_JUSTPRINT_N, 1);
            break;

        case OPT_INST_NOFETCH:
            ts->setop(ts, POLDEK_OP_NOFETCH, 1);
            break;
 
        case OPT_INST_FETCH:
            if (arg) {
                if (!is_rwxdir(arg)) {
                    logn(LOGERR, _("%s: no such directory"), arg);
                    return EINVAL;
                }
                
                poldek_ts_configure(ts, POLDEK_CONF_FETCHDIR, arg);
            }

            ts->setop(ts, POLDEK_OP_JUSTFETCH, 1);
            break;

        case OPT_PMONLY_FORCE:
            poldek_ts_configure(ts, POLDEK_CONF_RPMOPTS, "--force");
            break;
            
        case OPT_PMONLY_NODEPS:
            poldek_ts_configure(ts, POLDEK_CONF_RPMOPTS, "--nodeps");
            break;

        case OPT_PM: {
            char opt[256];
            n_snprintf(opt, sizeof(opt), "--%s", arg);
            poldek_ts_configure(ts, POLDEK_CONF_RPMOPTS, opt);
        }
            break;
            
            
        default:
            return ARGP_ERR_UNKNOWN;
    }
    
    return 0;
}


static int install(struct cmdctx *cmdctx)
{
    struct poldek_iinf   iinf;
    struct poclidek_ctx  *cctx;
    struct poldek_ts      *ts;
    int rc = 1, is_test;
    
    cctx = cmdctx->cctx;
    ts = cmdctx->ts;
    
    poldek_ts_set_type(ts, POLDEK_TS_INSTALL, "install-cmd");
    if (!poldek_ts_issetf(ts, POLDEK_TS_INSTALL))
        poldek_ts_setf(ts, POLDEK_TS_UPGRADE); /* the default */
    is_test = ts->getop_v(ts, POLDEK_OP_TEST, POLDEK_OP_RPMTEST, 0);

    rc = poldek_ts_run(ts, is_test ? NULL : &iinf);
    
    if (rc == 0 && !sigint_reached())
        msgn(1, _("There were errors"));
    
    if (!is_test && cmdctx->cctx->pkgs_installed)
        poclidek_apply_iinf(cmdctx->cctx, &iinf);
    
    if (!is_test)
        poldek_iinf_destroy(&iinf);
    
    return rc;
}

static int cmdl_run(struct poclidek_opgroup_rt *rt)
{
    int rc;

    dbgf_("%p->%p, %p->%p\n", rt->ts, rt->ts->hold_patterns,
          rt->ts->ctx->ts, rt->ts->ctx->ts->hold_patterns);
    
    if (poldek_ts_type(rt->ts) != POLDEK_TS_INSTALL)
        return OPGROUP_RC_NIL;

    if (!poldek_ts_issetf_all(rt->ts, POLDEK_TS_UPGRADEDIST)) {
        if (poldek_ts_get_arg_count(rt->ts) == 0) {
            logn(LOGERR, _("no packages specified"));
            return OPGROUP_RC_ERROR | OPGROUP_RC_FINI;
        }
    }
    

    rc = poldek_ts_run(rt->ts, NULL);
    return rc ? OPGROUP_RC_FINI : OPGROUP_RC_ERROR | OPGROUP_RC_FINI;
}
