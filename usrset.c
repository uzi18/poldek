/* 
   Copyright (C) 2000 - 2002 Pawel A. Gajda (mis@k2.net.pl)
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License published by
  the Free Software Foundation (see file COPYING for details).
*/

/*
  $Id$
*/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <trurl/nassert.h>
#include <trurl/narray.h>
#include <trurl/nstr.h>
#include <trurl/nmalloc.h>

#include <vfile/vfile.h>

#include "i18n.h"
#include "log.h"
#include "usrset.h"
#include "misc.h"
#include "pkgmisc.h"
#include "rpm/rpm_pkg_ld.h"


inline static int deftype(const char *str) 
{
    n_assert(str);
    while (*str) {
        if (isspace(*str) || *str == '&') 
            return 0;
        
        if (strchr("?*[", *str))
            return PKGDEF_PATTERN;
        str++;
    }
    return PKGDEF_REGNAME;
}


void pkgdef_free(struct pkgdef *pdef)
{
    if (pdef->pkg)
        pkg_free(pdef->pkg);
    pdef->pkg = NULL;
    free(pdef);
}


int pkgdef_cmp(struct pkgdef *pdef1, struct pkgdef *pdef2)
{
    int cmprc = 0;
    
    if (pdef1->pkg && pdef2->pkg) 
        if ((cmprc = pkg_cmp_name(pdef1->pkg, pdef2->pkg)))
            return cmprc;
        
    if ((cmprc = pdef1->tflags - pdef2->tflags)) 
        return cmprc;
        
    if (pdef1->tflags & PKGDEF_VIRTUAL)
        cmprc = strcmp(pdef1->virtname, pdef2->virtname);
    
    return cmprc;
}


__inline__
static int argvlen(char **argv) 
{
    int i = 0, len = 0;
    while (argv[i])
        len += strlen(argv[i++]) + 1;
    return len;
}

static 
int pkgdef_new_str(struct pkgdef **pdefp, char *buf, int buflen,
                   const char *fpath, int nline)
{
    struct pkgdef      *pdef = NULL;
    char               *p, *s[1024];
    int                n, tflags = 0, deftyp = 0;
    const char         **tl, **tl_save;
    const char         *evrstr = NULL, *name = NULL, *virtname = NULL;
    const char         *version = NULL, *release = NULL;
    int32_t            epoch = 0;
    

    n = buflen;
    *pdefp = NULL;
    
    s[0] = NULL;

    while (n && isspace(buf[n - 1]))
        buf[--n] = '\0';
    
    p = buf;
    while(isspace(*p))
        p++;
        
    if (*p == '\0' || *p == '#')
        return 0;

    while (*p && !isalnum(*p)) {
        switch (*p) {
            case '~':
            case '!':           /* for backward compatybility */
                tflags |= PKGDEF_OPTIONAL;
                break;
                
            case  '@': 
                tflags |= PKGDEF_VIRTUAL;
                break;
                
            default:
                tflags |= PKGDEF_REGNAME;
        }
        p++;
    }
    
    if (!isalnum(*p)) {
        if (nline > 0)
            logn(LOGERR, _("%s:%d: syntax error"), fpath, nline);
        else 
            logn(LOGERR, _("syntax error in package definition"));
        return -1;
    }

    tl = tl_save = n_str_tokl(p, "#\t ");
    
    if (tflags & PKGDEF_VIRTUAL) {
        virtname = tl[0];
        if (virtname) 
            name = tl[1];
        
        if (name) 
            evrstr = tl[2];
        
    } else {
        virtname = NULL;
        name = tl[0];
        evrstr = tl[1];
    }
        
    DBGF("virtname = %s, name = %s, evrstr = %s, %d\n",
         virtname, name, evrstr, tflags);
    
    if (name) {
        deftyp = deftype(name);
        if (name && deftyp == 0) {
            if (nline) 
                logn(LOGERR, _("%s:%d %s: invalid package name"),
                 fpath, nline, name);
            else 
                logn(LOGERR, _("%s: invalid package name"), name);
            
            n_str_tokl_free(tl_save);
            return -1;
        }
    }

        
    pdef = n_malloc(sizeof(*pdef) + (virtname ? strlen(virtname) + 1 : 0));
    pdef->tflags = tflags | deftyp;

    if (name == NULL) {
        pdef->pkg = NULL;
            
    } else {
        if (evrstr) 
            parse_evr((char*)evrstr, &epoch, &version, &release);
        
        if (version == NULL)
            version = "";
        
        if (release == NULL)
            release = "";
                
        pdef->pkg = pkg_new(name, epoch, version, release, NULL, NULL);
    }

    if (virtname) 
        strcpy(pdef->virtname, virtname);
        
    *pdefp = pdef;

    n_str_tokl_free(tl_save);
    return pdef ? 1 : 0;
}


static 
int pkgdef_new_pkg(struct pkgdef **pdefp, struct pkg *pkg)
{
    struct pkgdef *pdef;
    

    pdef = n_malloc(sizeof(*pdef));
    pdef->tflags = PKGDEF_REGNAME;
    pdef->pkg = pkg_link(pkg);
    *pdefp = pdef;

    return pdef ? 1 : 0;
}

static 
int pkgdef_new_pkgfile(struct pkgdef **pdefp, const char *path)
{
    struct pkg *pkg;
    struct pkgdef *pdef;
    

    if ((pkg = pkg_ldrpm(path, PKG_LDNEVR)) == NULL)
        return -1;

    pdef = n_malloc(sizeof(*pdef));
    pdef->tflags = PKGDEF_PKGFILE;
    pdef->pkg = pkg;
    *pdefp = pdef;

    return pdef ? 1 : 0;
}


struct usrpkgset *usrpkgset_new(void) 
{
    struct usrpkgset *ups;

    ups = n_malloc(sizeof(*ups));
    ups->pkgdefs = n_array_new(64, (tn_fn_free)pkgdef_free,
                               (tn_fn_cmp) pkgdef_cmp);
    ups->path = NULL;
    return ups;
}


void usrpkgset_free(struct usrpkgset *ups) 
{
    if (ups->path)
        free(ups->path);

    if (ups->pkgdefs)
        n_array_free(ups->pkgdefs);
    free(ups);
}


int usrpkgset_add_list(struct usrpkgset *ups, const char *fpath)
{
    char buf[1024];
    struct vfile *vf;
    int nline, rc = 1;
    
    if ((vf = vfile_open(fpath, VFT_STDIO, VFM_RO)) == NULL) 
        return 0;

    nline = 0;
    while (fgets(buf, sizeof(buf), vf->vf_stream)) {
        struct pkgdef *pdef;
        
        nline++;
        
        switch (pkgdef_new_str(&pdef, buf, strlen(buf), fpath, nline)) {
            case 0:
                break;
                
            case 1:
                n_array_push(ups->pkgdefs, pdef);
                break;
                
            case -1:
                logn(LOGERR, _("%s: give up at %d"), fpath, nline);
                rc = 0;
                break;
                
            default:
                n_assert(0);
                die();
        }
        
        if (rc == 0)
            break;
    }
    
    vfile_close(vf);
    
    if (rc) {
        n_array_sort(ups->pkgdefs);
        ups->path = n_strdup(fpath);
    }

    return rc;
}

int usrpkgset_add_str(struct usrpkgset *ups, char *def, int deflen) 
{
    struct pkgdef *pdef = NULL;

    if ((pkgdef_new_str(&pdef, def, deflen, NULL, -1)) > 0) {
        n_array_push(ups->pkgdefs, pdef);
        return 1;
    }
    
    return 0;
}

int usrpkgset_add_pkgfile(struct usrpkgset *ups, const char *path) 
{
    struct pkgdef *pdef = NULL;
    
    if ((pkgdef_new_pkgfile(&pdef, path)) > 0) {
        n_array_push(ups->pkgdefs, pdef);
        return 1;
    }
    
    return 0;
}

int usrpkgset_add_pkg(struct usrpkgset *ups, struct pkg *pkg) 
{
    struct pkgdef *pdef = NULL;
    
    if ((pkgdef_new_pkg(&pdef, pkg)) > 0) {
        n_array_push(ups->pkgdefs, pdef);
        return 1;
    }
    
    return 0;
}


int usrpkgset_setup(struct usrpkgset *ups) 
{
    if (ups->pkgdefs) {
        int n;

        n = n_array_size(ups->pkgdefs);
        n_array_sort(ups->pkgdefs);
        n_array_uniq(ups->pkgdefs);

        if (n != n_array_size(ups->pkgdefs)) {
            msgn(1, _("Removed %d duplicates from given packages"),
                n - n_array_size(ups->pkgdefs));
        }
    }

    return 1;
}

int pkgdef_match_pkg(const struct pkgdef *pdef, const struct pkg *pkg) 
{
    int rc = 1;
    
    if (pdef->pkg->epoch && pkg->epoch != pdef->pkg->epoch)
        rc = 0;
    
    if (rc && *pdef->pkg->ver) 
        if (strcmp(pdef->pkg->ver, pkg->ver) != 0) 
            rc = 0;
    
    if (rc && *pdef->pkg->rel)
        if (strcmp(pdef->pkg->rel, pkg->rel) != 0)
            rc = 0;
#if 0    
    msgn(1, "MATCH %d e%d e%d %s %s", rc, 
        pkg->epoch, pdef->pkg->epoch,
        pkg_snprintf_s(pkg),
        pkg_snprintf_s0(pdef->pkg));
#endif    
    return rc;
}
