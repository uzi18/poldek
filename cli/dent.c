/*
  Copyright (C) 2000 - 2002 Pawel A. Gajda <mis@pld.org.pl>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 as
  published by the Free Software Foundation (see file COPYING for details).

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
  $Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fnmatch.h>

#include <trurl/trurl.h>

#include "i18n.h"
#include "log.h"
#include "misc.h"
#include "cli.h"
#include "arg_packages.h"


struct pkg_dent *pkg_dent_new(const char *name, struct pkg *pkg,
                              int flags)
{
    struct pkg_dent *ent;
    int len = 0;

    if (name) {
        len += strlen(name) + 1;
        n_assert(flags & PKG_DENT_DIR);
    }
    
    ent = n_malloc(sizeof(*ent) + len);
    ent->_refcnt = 0;
    ent->flags = flags;
    ent->parent = NULL;
    
    if (name) {
        memcpy(ent->_buf, name, len);
        ent->name = ent->_buf;
    }
    
    if (flags & PKG_DENT_DIR) {
        ent->pkg_dent_ents = n_array_new(128, (tn_fn_free)pkg_dent_free,
                                         (tn_fn_cmp)pkg_dent_cmp);
        n_array_ctl(ent->pkg_dent_ents, TN_ARRAY_AUTOSORTED);

    } else {
        ent->name = pkg->nvr;
        ent->pkg_dent_pkg = pkg_link(pkg);
    }
    
    return ent;
}
#define pkg_dent_new_pkg(pkg) pkg_dent_new(NULL, pkg, 0);
#define pkg_dent_new_dir(name) pkg_dent_new(name, NULL, PKG_DENT_DIR);


struct pkg_dent *pkg_dent_link(struct pkg_dent *ent)
{
    ent->_refcnt++;
    return ent;
}

void pkg_dent_free(struct pkg_dent *ent)
{
    if (ent->_refcnt > 0) {
        ent->_refcnt--;
        return;
    }
    
    if (ent->flags & PKG_DENT_DIR)
        n_array_free(ent->pkg_dent_ents);
    else
        pkg_free(ent->pkg_dent_pkg);
    free(ent);
}


struct pkg *pkg_dent_getpkg(struct pkg_dent *ent) 
{
    if (ent->flags & PKG_DENT_DIR)
        return NULL;
    return ent->pkg_dent_pkg;
}

int pkg_dent_cmp(struct pkg_dent *e1, struct pkg_dent *e2)
{
    return strcmp(e1->name, e2->name);
}

int pkg_dent_cmp_rev(struct pkg_dent *e1, struct pkg_dent *e2)
{
    return strcmp(e2->name, e1->name);
};

int pkg_dent_strncmp(struct pkg_dent *ent, const char *name)
{
    return strncmp(ent->name, name, strlen(name));
}

int pkg_dent_strcmp(struct pkg_dent *ent, const char *name)
{
    return strcmp(ent->name, name);
}

void pkg_dent_sort(struct pkg_dent *ent,
                   int (*cmpf)(struct pkg_dent *, struct pkg_dent*))
{
    if (ent->flags & PKG_DENT_DIR)
        n_array_sort_ex(ent->pkg_dent_ents, (tn_fn_cmp)cmpf);
}

int pkg_dent_cmp_btime(struct pkg_dent *ent1, struct pkg_dent *ent2)
{
    int cmprc;
    int32_t btime1, btime2;
    
    btime1 = (ent1->flags & PKG_DENT_DIR) ? 0 : ent1->pkg_dent_pkg->btime;
    btime2 = (ent2->flags & PKG_DENT_DIR) ? 0 : ent2->pkg_dent_pkg->btime;

    cmprc = btime1 - btime2;
    if (cmprc)
        return cmprc;

    return strcmp(ent1->name, ent2->name);
}

int pkg_dent_cmp_btime_rev(struct pkg_dent *ent1, struct pkg_dent *ent2)
{
    return pkg_dent_cmp_btime(ent2, ent1);
}

int pkg_dent_cmp_bday(struct pkg_dent *ent1, struct pkg_dent *ent2)
{
    int cmprc, gmt_off;
    int32_t btime1, btime2;
    
    btime1 = (ent1->flags & PKG_DENT_DIR) ? 0 : ent1->pkg_dent_pkg->btime;
    btime2 = (ent2->flags & PKG_DENT_DIR) ? 0 : ent2->pkg_dent_pkg->btime;

    cmprc = ((btime1 + gmt_off) / 86400) - ((btime2 + gmt_off) / 86400);
    cmprc = btime1 - btime2;
    
    gmt_off = get_gmt_offs();
    
    if (cmprc)
        return cmprc;
    
    return strcmp(ent1->name, ent2->name);
}

int pkg_dent_cmp_bday_rev(struct pkg_dent *ent1, struct pkg_dent *ent2)
{
    return pkg_dent_cmp_bday(ent2, ent1);
}

static inline
struct pkg_dent *pkg_dent_addpkg(struct pkg_dent *dent, struct pkg *pkg)
{
    struct pkg_dent *ent;

    ent = pkg_dent_new_pkg(pkg);
    n_array_push(dent->pkg_dent_ents, ent);
    return ent;
}

inline
int pkg_dent_addpkgs(struct pkg_dent *dent, tn_array *pkgs)
{
    int i;
    struct pkg_dent *ent;
    
    for (i=0; i < n_array_size(pkgs); i++) {
        struct pkg *pkg = n_array_nth(pkgs, i);
        ent = pkg_dent_new_pkg(pkg);
        n_array_push(dent->pkg_dent_ents, ent);
    }
    
    return 1;
}


inline
struct pkg_dent *pkg_dent_adddir(struct pkg_dent *dent, const char *name)
{
    struct pkg_dent *ent;
    
    ent = pkg_dent_new_dir(name);
    printf("adddir %s\n", name);
    ent->parent = dent;
    n_array_push(dent->pkg_dent_ents, ent);
    n_array_sort(dent->pkg_dent_ents);
    return ent;
}


char *poclidek_dent_dirpath(char *path, int size, const struct pkg_dent *dent)
{
    tn_array *stack;
    int n = 0;

    
    stack = n_array_new(4, NULL, NULL);
    while (dent->parent) {
        n_array_push(stack, dent->name);
        dent = dent->parent;
    }

    path[0] = '/';
    path[1] = '\0';
    n = 1;
    while (n_array_size(stack))
        n += n_snprintf(&path[n], size - n, "%s/", n_array_shift(stack));
    
    if (n > 1)
        path[n - 1] = '\0';     /* eat '/' */
    n_array_free(stack);
    return path;
}



void poclidek_dent_init(struct poclidek_ctx *cctx)
{
    struct pkg_dent *root, *ent, *allav;
    int i;

    root = pkg_dent_new_dir("/");
    allav = pkg_dent_adddir(root, "all-avail");
    //cctx->dirs = n_array_new(8, free, (tn_fn_cmp)dir_cmp);

    for (i=0; i < n_array_size(cctx->ctx->pkgdirs); i++) {
        char *name, *p;
        struct pkgdir *pkgdir = n_array_nth(cctx->ctx->pkgdirs, i);
        

        name = n_strdup(pkgdir_idstr(pkgdir));
        p = name;
        while (*p) {
            if (!isprint(*p))
                *p = '.';
            p++;
        }

        ent = pkg_dent_adddir(root, name);
        pkg_dent_addpkgs(ent, pkgdir->pkgs);
        pkg_dent_addpkgs(allav, pkgdir->pkgs);
        free(name);
    }

    cctx->rootdir = root;
    cctx->homedir = allav;
    cctx->currdir = allav;
}


int poclidek_chdir(struct poclidek_ctx *cctx, const char *path) 
{
    const char **tl, **tl_save, *p;
    struct pkg_dent *ent;
    int rc = 0;

    if (n_str_eq(path, "."))
        return 1;
    
    if (n_str_eq(path, "..")) {
        if (cctx->currdir->parent)
            cctx->currdir = cctx->currdir->parent;
        return 1;
    }

    if ((p = strchr(path, '/')) == NULL) {
        printf("currdir %s, path %s\n", cctx->currdir->name, path);
        ent = n_array_bsearch_ex(cctx->currdir->pkg_dent_ents, path,
                                 (tn_fn_cmp)pkg_dent_strcmp);
        if (ent) {
            cctx->currdir = ent;
            return 1;
            
        } else {
            return 0;
        }
    }

    if (*path == '/')
        cctx->currdir = cctx->rootdir;
    
    tl = tl_save = n_str_tokl(path, "/");
    rc = 1;
    while (*tl) {
        if (n_str_ne(*tl, ""))
            if (!(rc = poclidek_chdir(cctx, *tl)))
                break;
        tl++;
    }
    n_str_tokl_free(tl_save);
    return rc;
}


int poclidek_chdirent(struct poclidek_ctx *cctx, const struct pkg_dent *dent)
{
    char path[PATH_MAX];

    poclidek_dent_dirpath(path, sizeof(path), dent);
    return poclidek_chdir(cctx, path);
}


struct pkg_dent *poclidek_dent_find(struct poclidek_ctx *cctx, const char *path)
{
    void *currdir = cctx->currdir, *dent = NULL;
    
    if (path == NULL || (path && poclidek_chdir(cctx, path)))
        dent = cctx->currdir;
    
    cctx->currdir = currdir;
    return dent;
}

    

tn_array *poclidek_get_dents(struct poclidek_ctx *cctx, const char *path)
{
    tn_array *ents = NULL;
    void *currdir = cctx->currdir;
    
    if (path && (n_str_eq(path, ".") || n_str_eq(path, "")))
        path = NULL;

    if (path == NULL)
        poclidek_load_packages(cctx, 1);
    
    if (path == NULL || (path && poclidek_chdir(cctx, path)))
        ents = cctx->currdir->pkg_dent_ents;
        
    cctx->currdir = currdir;
    return ents;
}


tn_array *poclidek_get_dent_packages(struct poclidek_ctx *cctx, const char *dir)
{
    tn_array *pkgs, *ents;
    int i;

    ents = poclidek_get_dents(cctx, dir);
    pkgs = n_array_new(n_array_size(ents), (tn_fn_free)pkg_free,
                       (tn_fn_cmp)pkg_nvr_strcmp);
    
    for (i=0; i < n_array_size(ents); i++) {
        struct pkg *pkg;

        if ((pkg = pkg_dent_getpkg(n_array_nth(ents, i))))
            n_array_push(pkgs, pkg_link(pkg));
    }
    return pkgs;
}


static
tn_array *do_resolve(struct arg_packages *aps,
                     tn_array *ents, unsigned flags);


tn_array *poclidek_resolve_dents(const char *path,
                                 struct poclidek_ctx *cctx,
                                 struct poldek_ts *ts,
                                 int exact)
{
    tn_array *ents;

    if ((ents = poclidek_get_dents(cctx, path)) == NULL)
        return NULL;

    return do_resolve(ts->aps, ents,
                      exact ? ARG_PACKAGES_RESOLV_EXACT : 0);
}


static
tn_array *do_resolve(struct arg_packages *aps,
                     tn_array *ents, unsigned flags)
{
    tn_array *ments = NULL, *masks;
    int i, j, nmasks;
    int *matches, *matches_bycmp;

    masks = arg_packages_get_pkgmasks(aps);
    nmasks = n_array_size(masks);
    
    for (i=0; i < nmasks; i++) {
        char  *mask = n_array_nth(masks, i);
        
        if (*mask == '*' && *(mask + 1) == '\0')
            return n_ref(ents);
    }

    matches = alloca(nmasks * sizeof(*matches));
    memset(matches, 0, nmasks * sizeof(*matches));

    matches_bycmp = alloca(nmasks * sizeof(*matches_bycmp));
    memset(matches_bycmp, 0, nmasks * sizeof(*matches_bycmp));
    
    ments = n_array_clone(ents);
    for (i=0; i < n_array_size(ents); i++) {
        struct pkg_dent *ent = n_array_nth(ents, i);
        
        for (j=0; j < nmasks; j++) {
            char *mask = n_array_nth(masks, j);
            int  skip = 0;

            switch (*mask) {
                case '~':
                case '!':           /* for backward compatybility */
                    skip = 1;       /* optional package */
                    break;
                    
                case  '@':
                    mask++;
                    break;
            }
            

            if (fnmatch(mask, ent->name, 0) == 0) {
                n_array_push(ments, pkg_dent_link(ent));
                matches_bycmp[j]++;
                matches[j]++;
            }
        }
    }
    
    
    for (j=0; j < n_array_size(masks); j++) {
        const char *mask = n_array_nth(masks, j);
        
        if (matches[j] == 0 && (flags & ARG_PACKAGES_RESOLV_MISSINGOK) == 0) {
            logn(LOGERR, _("%s: no such package or directory"), mask);
            n_array_clean(ments);
        }
        
        if ((flags & ARG_PACKAGES_RESOLV_UNAMBIGUOUS) == 0 && matches_bycmp[j] > 1) {
            int pri = (flags & ARG_PACKAGES_RESOLV_EXACT) ? LOGERR : LOGWARN;
            logn(pri, _("%s: ambiguous name"), mask);
            if (flags & ARG_PACKAGES_RESOLV_EXACT)
                n_array_clean(ments);
        }
    }

    
    n_array_sort(ments);
    n_array_uniq(ments);
    
    //if (flags & ARG_PACKAGES_RESOLV_UNAMBIGUOUS)
    //    n_array_uniq_ex(pkgs, (tn_fn_cmp)pkg_cmp_name_uniq);
    
    if (n_array_size(ments) == 0) {
        n_array_free(ments);
        ments = NULL;
    }
    
    return ments;
}


