/* 
  Copyright (C) 2000 - 2002 Pawel A. Gajda (mis@k2.net.pl)
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License published by
  the Free Software Foundation (see file COPYING for details).
*/

/*
  $Id$
*/

#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include <trurl/nassert.h>
#include <trurl/nstr.h>

#include <vfile/vfile.h>

#include "i18n.h"
#include "pkgset.h"
#include "pkgset-load.h"
#include "misc.h"
#include "log.h"


struct source *source_new(const char *pathspec, const char *pkg_prefix)
{
    struct source *src;
    struct stat st;
    const char *path, *p;
    char *name, *q;
    int len;

    
    p = pathspec;
    
    while (*p && *p != '|' && !isspace(*p))
        p++;

    if (*p == '\0') {           /* path only */
        path = pathspec;
        name = "gall";
        
    } else {
        path = p + 1;
        while (isspace(*path))
            path++;
        
        len = p - pathspec;
        name = alloca(len + 1);
        memcpy(name, pathspec, len);
        name[len] = '\0';
        
        if (*name == '[') 
            name++;
        
        if ((q = strrchr(name, ']')))
            *q = '\0';
        if (*name == '\0')
            name = "gall";
    }
    
    
    
    src = malloc(sizeof(*src));
    src->source_path = NULL;
    
    
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        len = strlen(path);
        
        if (path[len - 1] == '/') {
            src->source_path = malloc(len + 1);
            memcpy(src->source_path, path, len + 1);
            
        } else {
            src->source_path = malloc(len + 2 /* for '/' */);
            memcpy(src->source_path, path, len);
            src->source_path[len] = '/';
            src->source_path[len + 1] = '\0';
        }
    }

    if (src->source_path == NULL) 
        src->source_path = strdup(path);
    
    src->pkg_prefix = NULL;
    if (pkg_prefix)
        src->pkg_prefix = strdup(pkg_prefix);
    src->ldmethod = PKGSET_LD_NIL;
    src->source_name = strdup(name);
    DBGF("source_new %s -> %s\n", name, src->source_path);
    return src;
}

void source_free(struct source *src)
{
    free(src->source_path);
    if (src->pkg_prefix)
        free(src->pkg_prefix);
    if (src->source_name)
        free(src->source_name);
    free(src);
}


int source_cmp(struct source *s1, struct source *s2)
{
    return strcmp(s1->source_path, s2->source_path);
}

int source_cmp_name(struct source *s1, struct source *s2)
{
    return strcmp(s1->source_name, s2->source_name);
}


int source_update(struct source *src)
{
    return update_whole_pkgdir(src->source_path);
}

int pkgset_load(struct pkgset *ps, int ldflags, tn_array *sources)
{
    int i, j, iserr = 0;
    struct pkgdir *pkgdir = NULL;


    for (i=0; i < n_array_size(sources); i++) {
        struct source *src = n_array_nth(sources, i);

        if (src->ldmethod == PKGSET_LD_NIL) 
            src->ldmethod = PKGSET_LD_IDX;
        
        switch (src->ldmethod) {
            case PKGSET_LD_IDX:
                pkgdir = pkgdir_new(src->source_name, src->source_path,
                                    src->pkg_prefix, PKGDIR_NEW_VERIFY);
                if (pkgdir != NULL) 
                    break;
                
                if (is_dir(src->source_path)) 
                    src->ldmethod = PKGSET_LD_DIR; /* no break */
                else
                    break;
                
            case PKGSET_LD_DIR:
                msg(1, _("Loading %s..."), src->source_path);
                pkgdir = pkgdir_load_dir(src->source_name, src->source_path);
                break;

            default:
                n_assert(0);
        }

        if (pkgdir == NULL) {
            if (n_array_size(sources) > 1)
                logn(LOGWARN, _("%s: load failed, skipped"), src->source_path);
            continue;
        }
        
        n_array_push(ps->pkgdirs, pkgdir);
    }


    /* merge pkgdis depdirs into ps->depdirs */
    for (i=0; i<n_array_size(ps->pkgdirs); i++) {
        pkgdir = n_array_nth(ps->pkgdirs, i);
        
        if (pkgdir->depdirs) {
            for (j=0; j<n_array_size(pkgdir->depdirs); j++)
                n_array_push(ps->depdirs, n_array_nth(pkgdir->depdirs, j));
        }
    }

    n_array_sort(ps->depdirs);
    n_array_uniq(ps->depdirs);

    
    for (i=0; i<n_array_size(ps->pkgdirs); i++) {
        pkgdir = n_array_nth(ps->pkgdirs, i);

        if (pkgdir->flags & PKGDIR_LDFROM_IDX) {
            msgn(1, _("Loading %s..."), pkgdir->idxpath);
            if (!pkgdir_load(pkgdir, ps->depdirs, ldflags)) {
                logn(LOGERR, _("%s: load failed"), pkgdir->idxpath);
                iserr = 1;
            }
        }
    }
    
    if (!iserr) {
        /* merge pkgdirs packages into ps->pkgs */
        for (i=0; i<n_array_size(ps->pkgdirs); i++) {
            pkgdir = n_array_nth(ps->pkgdirs, i);
            for (j=0; j<n_array_size(pkgdir->pkgs); j++)
                n_array_push(ps->pkgs, pkg_link(n_array_nth(pkgdir->pkgs, j)));
        }
    }
    
    if (n_array_size(ps->pkgs)) {
        int n = n_array_size(ps->pkgs);
        msgn(1, ngettext("%d package read",
                        "%d packages read", n), n);
    }
    
    return n_array_size(ps->pkgs);
}
