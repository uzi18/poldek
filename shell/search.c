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

#include <pcre.h>
#include <trurl/nassert.h>
#include <trurl/narray.h>

#include "log.h"
#include "pkg.h"
#include "pkgset.h"
#include "pkgset.h"
#include "misc.h"
#include "search.h"
#include "shell.h"

static const unsigned char   *pcre_chartable = NULL;
static int                    pcre_established = 0;

struct pattern {
    char             *regexp;
    unsigned         flags;
    pcre             *pcre;
    pcre             *pcre_extra;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state);
static int search(struct cmdarg *cmdarg);

#define OPT_SEARCH_CAP     (1 << 0)
#define OPT_SEARCH_REQ     (1 << 1)
#define OPT_SEARCH_CNFL    (1 << 2)
#define OPT_SEARCH_OBSL    (1 << 3)
#define OPT_SEARCH_SUMM    (1 << 4)
#define OPT_SEARCH_DESC    (1 << 5)
#define OPT_SEARCH_FL      (1 << 6)

#define OPT_SEARCH_ALL     (OPT_SEARCH_CAP  | OPT_SEARCH_REQ | OPT_SEARCH_CNFL |  \
                            OPT_SEARCH_OBSL | OPT_SEARCH_SUMM | OPT_SEARCH_DESC | \
                            OPT_SEARCH_FL)

#define OPT_SEARCH_DEFAULT (OPT_SEARCH_SUMM | OPT_SEARCH_DESC)

/* options which requires Packages processing */
#define OPT_SEARCH_HDD     (OPT_SEARCH_SUMM | OPT_SEARCH_DESC | OPT_SEARCH_FL)  

static struct argp_option options[] = {
    { "provides",  'p', 0, 0, "Search package capablities", 1},
    { "requires",  'r', 0, 0, "Search package requirements", 1},
    { "conflicts", 'c', 0, 0, "Search package conflicts", 1},
    { "obsoletes", 'o', 0, 0, "Search package obsolences", 1},
    { "summary",   's', 0, 0, "Search summaries, urls and license", 1},
    { "description",   'd', 0, 0, "Search packages descriptions", 1},
    { "files",     'l', 0,  0, "Search package file list", 1},
    { "all",       'a', 0, 0,
      "Search all described fields, the defaults are: -sd", 1
    }, 
    { 0, 0, 0, 0, 0, 0 },
};

struct command command_search;

static
struct command_alias cmd_aliases[] = {
    {
        "what-requires", "search -r", &command_search,
    },

    {
        "what-provides", "search -p", &command_search,
    },

    {
        NULL, NULL, NULL
    },
};


struct command command_search = {
    0, 
    "search", "PATTERN [PACKAGE...]", "Search packages", 
    options, parse_opt,
    NULL, search,
    NULL, NULL,
    (struct command_alias*)&cmd_aliases, 
    "PATTERN := /perl-regexp/[imsx], see perlre(1) for help\n"
};

static
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct cmdarg *cmdarg = state->input;

    switch (key) {
        case 'a':
            cmdarg->flags |= OPT_SEARCH_ALL;
            break;
            
        case 'c':
            cmdarg->flags |= OPT_SEARCH_CNFL;
            break;

        case 'l':
            cmdarg->flags |= OPT_SEARCH_FL;
            break;
            
        case 'o':
            cmdarg->flags |= OPT_SEARCH_OBSL;
            break;
            
        case 'p':
            cmdarg->flags |= OPT_SEARCH_CAP;
            break;

        case 'r':
            cmdarg->flags |= OPT_SEARCH_REQ;
            break;
            
        case 's':
            cmdarg->flags |= OPT_SEARCH_SUMM;
            break;

        case 'd':
            cmdarg->flags |= OPT_SEARCH_DESC;
            break;
        
        case ARGP_KEY_ARG:
            if (n_array_size(cmdarg->pkgnames) == 0 && cmdarg->d == NULL) {
                struct pattern   *pt;
                char             *p, delim, *lastp, *regexp;
                int              len;
                unsigned         flags = 0;
                
                
                p = arg;
                delim = *arg;
                if (delim != '/' && delim != '|') {
                    argp_usage(state);
                    return EINVAL;
                }
                
                len = strlen(p) - 1;
                lastp = p + len;
                
                if (strchr("imsx", *lastp) == NULL && *lastp != delim) {
                    argp_usage(state);
                    return EINVAL;
                    
                }
                
                regexp = p + 1;
                
                if ((p = strrchr(regexp, delim)) == NULL) {
                    argp_usage(state);
                    return EINVAL;
                }
                
                *p = '\0';
                p++;
                
                
                while (*p) {
                    switch (*p) {
                        case 'i':
                            flags |= PCRE_CASELESS;
                            break;

                        case 'm':
                            flags |= PCRE_MULTILINE;
                            break;

                        case 's':
                            flags |= PCRE_DOTALL;
                            break;

                        case 'x':
                            flags |= PCRE_EXTENDED;
                            break;
                            
                        default:
                            log(LOGERR, "search: unknown regexp option -- %c\n", *p);
                            argp_usage(state);
                            return EINVAL;
                    }
                    p++;
                }
                

                pt = malloc(sizeof(*pt));
                pt->regexp = strdup(regexp);
                pt->flags = flags;
                pt->pcre = NULL;
                pt->pcre_extra = NULL;
                cmdarg->d = pt;
                
                break;
            }
            /* no break, let default handler collect arguments */
            
        default:
            return ARGP_ERR_UNKNOWN;
    }
    
    return 0;
}

static
void init_pcre(void) 
{
    if (pcre_established == 0) {
        pcre_malloc = malloc;
        pcre_free = free;
        pcre_chartable = pcre_maketables();
#if 0        
        if (pcre_chartable != NULL)
            printf("pcre_chartable ON: %s\n", pcre_chartable);
#endif        
        pcre_established = 1;
    }
}

static
int pattern_compile(struct pattern *pt, int ntimes) 
{
    const char       *pcre_err = NULL;
    int              pcre_err_off = 0;

    
    n_assert(pt->pcre == NULL);
    n_assert(pt->pcre_extra == NULL);
    
    pt->pcre = pcre_compile(pt->regexp, pt->flags, &pcre_err,
                            &pcre_err_off, pcre_chartable);
    
    if (pt->pcre == NULL) {
        log(LOGERR, "search: pattern: %s:%d: %s\n", pt->regexp,
            pcre_err_off, pcre_err);
        return 0;
    }

    if (ntimes > 10) {
        pt->pcre_extra = pcre_study(pt->pcre, PCRE_CASELESS, &pcre_err);
        if (pt->pcre_extra == NULL) {
            log(LOGERR, "search: pattern: %s: %s\n", pt->regexp, pcre_err);
            return 0;
        }
    }
    return 1;
}

static
int pattern_match(struct pattern *pt, const char *s, int len) 
{
    int match;
    
    match = pcre_exec(pt->pcre, pt->pcre_extra, s, len, 0, 0, NULL, 0);
    return match == 0;
}

static
void pattern_free(struct pattern *pt) 
{

    if (pt->regexp) {
        free(pt->regexp);
        pt->regexp = NULL;
    }
    
    if (pt->pcre) {
        free(pt->pcre);
        pt->pcre = NULL;
    }
    
 
    if (pt->pcre_extra) {
        free(pt->pcre_extra);
        pt->pcre_extra = NULL;
    }

    free(pt);
}


static int fl_match(tn_array *fl, struct pattern *pt) 
{
    int i, j, match = 0;
    
    for (i=0; i<n_array_size(fl); i++) {
        struct pkgfl_ent    *flent;
        char                path[PATH_MAX];

        flent = n_array_nth(fl, i);
        for (j=0; j<flent->items; j++) {
            struct flfile *f = flent->files[j];
            int n;
            
            n = snprintf(path, sizeof(path), "%s/%s", flent->dirname, f->basename);
            if ((match = pattern_match(pt, path, n)))
                goto l_end;

            
                
            if (S_ISLNK(f->mode)) {
                char *name = f->basename + strlen(f->basename) + 1;
                if ((match = pattern_match(pt, name, strlen(name))))
                    goto l_end;
            }
        }
    }
    
 l_end:
    return match;
}

static int search_pkg_files(struct pkg *pkg, struct pattern *pt) 
{
    tn_array  *fl;
    void      *flmark;
    int       match = 0;

    

    if (pkg->fl && fl_match(pkg->fl, pt))
        return 1;

    flmark = pkgflmodule_allocator_push_mark();

    if ((fl = pkg_other_fl(pkg)) != NULL) {
        match = fl_match(fl, pt);
        n_array_free(fl);
    }
    
    pkgflmodule_allocator_pop_mark(flmark);
    
    return match;
}



static int pkg_match(struct pkg *pkg, struct pattern *pt, unsigned flags) 
{
    int i, match = 0;
    struct capreq *cr;
    char *p;

    
    if ((flags & OPT_SEARCH_CAP) && pkg->caps)
        for (i=0; i<n_array_size(pkg->caps); i++) {
            cr = n_array_nth(pkg->caps, i);
            p = capreq_name(cr);
            if ((match = pattern_match(pt, p, strlen(p))))
                goto l_end;
        }
    
    if ((flags & OPT_SEARCH_REQ) && pkg->reqs)
        for (i=0; i<n_array_size(pkg->reqs); i++) {
            cr = n_array_nth(pkg->reqs, i);
            p = capreq_name(cr);
            if ((match = pattern_match(pt, p, strlen(p))))
                goto l_end;
        }
    
    if ((flags & (OPT_SEARCH_CNFL | OPT_SEARCH_OBSL)) && pkg->cnfls)
        for (i=0; i<n_array_size(pkg->cnfls); i++) {
            cr = n_array_nth(pkg->cnfls, i);
            p = capreq_name(cr);
            
            if ((flags & OPT_SEARCH_CNFL) == 0 || cnfl_is_obsl(cr))
                if ((match = pattern_match(pt, p, strlen(p))))
                    goto l_end;
        }
    

    if (flags & (OPT_SEARCH_SUMM | OPT_SEARCH_DESC)) {
        struct pkguinf *pkgu;
        
        if ((pkgu = pkg_info(pkg)) == NULL) {
            log(LOGERR, "%s: load package info failed\n", pkg_snprintf_s(pkg));
            
        } else {
            if (flags & OPT_SEARCH_SUMM) {
                match = pattern_match(pt, pkgu->summary, strlen(pkgu->summary));
                if (!match) 
                    match = pattern_match(pt, pkgu->license, strlen(pkgu->license));
                
                if (!match && pkgu->url)
                    match = pattern_match(pt, pkgu->url, strlen(pkgu->url));
            }
            
            if (!match && ((flags & OPT_SEARCH_DESC) && pkgu->description)) 
                match = pattern_match(pt, pkgu->description, strlen(pkgu->description));
            
            pkguinf_free(pkgu);
        }
        
    }

    if (match)
        goto l_end;
    
    if (flags & (OPT_SEARCH_FL)) 
        match = search_pkg_files(pkg, pt);
    
 l_end:
    return match;
}


static int search(struct cmdarg *cmdarg)
{
    tn_array         *shpkgs = NULL;
    tn_array         *matched_pkgs = NULL;
    int              i, err = 0, display_bar = 0, bar_v;
    int              term_height;
    struct pattern   *pt;
    
    
    if ((pt = cmdarg->d) == NULL) {
        log(LOGERR, "search: no pattern given\n");
        err++;
        goto l_end;
    }
    cmdarg->d = NULL;            /* we'll free pattern myself */
    
    if (cmdarg->flags == 0)
        cmdarg->flags = OPT_SEARCH_DEFAULT;
    
    init_pcre();
    if (!pattern_compile(pt, n_array_size(cmdarg->pkgnames))) {
        err++;
        goto l_end;
    }
    
    sh_resolve_packages(cmdarg->pkgnames, cmdarg->sh_s->avpkgs, &shpkgs, 0);
    
    if (shpkgs == NULL)
        return 0;

    if (n_array_size(shpkgs) == 0) {
        n_array_free(shpkgs);
        shpkgs = cmdarg->sh_s->avpkgs;
    }

    
    matched_pkgs = n_array_new(16, NULL, NULL);

    if (n_array_size(shpkgs) > 5 && (cmdarg->flags & OPT_SEARCH_HDD)) {
        display_bar = 1;
        msg(0, "Searching packages...");
    }
    bar_v = 0;
    
    for (i=0; i<n_array_size(shpkgs); i++) {
        struct shpkg *shpkg = n_array_nth(shpkgs, i);
        
        if (pkg_match(shpkg->pkg, pt, cmdarg->flags)) 
            n_array_push(matched_pkgs, shpkg);
        
        if (display_bar) {
            int v, j;
            
            v = i * 40 / n_array_size(shpkgs);
            for (j = bar_v; j < v; j++)
                msg(0, "_.");
            bar_v = v;
        }
    }
    
    if (display_bar) 
        msg(0, "_done.\n");

    term_height = get_term_height();
    if (n_array_size(matched_pkgs) == 0) 
        printf_c(PRCOLOR_YELLOW, "No one package matches /%s/\n", pt->regexp);
    
    else if (n_array_size(matched_pkgs) < term_height)
        printf_c(PRCOLOR_YELLOW, "%d package(s) found:\n",
                 n_array_size(matched_pkgs));
    
        
    for (i=0; i<n_array_size(matched_pkgs); i++) {
        struct shpkg *shpkg;

        shpkg = n_array_nth(matched_pkgs, i);
        printf("%s\n", shpkg->nevr);
    }

    if (n_array_size(matched_pkgs) >= term_height)
        printf_c(PRCOLOR_YELLOW, "%d package(s) found.\n",
                 n_array_size(matched_pkgs));
        
 l_end:

    if (shpkgs && shpkgs != cmdarg->sh_s->avpkgs)
        n_array_free(shpkgs);
    
    if (matched_pkgs)
        n_array_free(matched_pkgs);
    
    if (cmdarg->d) {
        cmdarg->d = NULL;
    }

    pattern_free(pt);
    return 1;
}

