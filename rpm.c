/* 
  Copyright (C) 2000 Pawel A. Gajda (mis@k2.net.pl)
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License published by
  the Free Software Foundation (see file COPYING for details).
*/

/*
  $Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmio.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmmacro.h>
#include <trurl/nassert.h>
#include <trurl/narray.h>
#include <trurl/nstr.h>

#include <vfile/vfile.h>

#include "rpm.h"
#include "rpmadds.h"
#include "misc.h"
#include "log.h"
#include "pkg.h"
#include "dbpkg.h"
#include "capreq.h"
#include "rpmdb_it.h"

static
int header_evr_match_req(Header h, const struct capreq *req);
static
int header_cap_match_req(Header h, const struct capreq *req, int strict);

int rpm_initlib(tn_array *macros) 
{
    if (rpmReadConfigFiles(NULL, NULL) != 0) {
        log(LOGERR, "rpmlib init failed\n");
        return 0;
    }

    if (macros) {
        int i;
        
        for (i=0; i<n_array_size(macros); i++) {
            char *def, *macro;
            
            if ((macro = n_array_nth(macros, i)) == NULL)
                continue;
            
            if ((def = strchr(macro, ' ')) == NULL && 
                (def = strchr(macro, '\t')) == NULL) {
                log(LOGERR, "%s: invalid macro definition\n", macro);
                return 0;
                
            } else {
                char *sav = def;
                
                *def = '\0';
                def++;
                while(isspace(*def))
                    def++;
                msg(2, "addMacro %s %s\n", macro, def);
                addMacro(NULL, macro, NULL, def, RMIL_DEFAULT);
                *sav = ' ';
            }
        }
    }
    return 1;
}


rpmdb rpm_opendb(const char *dbpath, const char *rootdir, mode_t mode) 
{
    rpmdb db = NULL;
    
    if (dbpath)
        addMacro(NULL, "_dbpath", NULL, dbpath, RMIL_DEFAULT);

    if (rpmdbOpen(rootdir ? rootdir : "/", &db, mode, 0) != 0) {
        db = NULL;
        log(LOGERR, "failed to open rpm database\n");
    }
    
    return db;
}


void rpm_closedb(rpmdb db) 
{
    rpmdbClose(db);
    db = NULL;
}


static void rpm_die(void) 
{
    log(LOGERR, "database error\n");
    die();
}



tn_array *rpm_get_file_conflicted_dbpkgs(rpmdb db, const char *path,
                                         tn_array *unistdbpkgs)
{
    tn_array *cnfldbpkgs = dbpkg_array_new(4);
    
#ifdef HAVE_RPM_4_0
    rpmdbMatchIterator mi;
    Header h;

    mi = rpmdbInitIterator(db, RPMTAG_BASENAMES, path, 0);
    while((h = rpmdbNextIterator(mi)) != NULL) {
	unsigned int recno = rpmdbGetIteratorOffset(mi);

        if (unistdbpkgs && dbpkg_array_has(unistdbpkgs, recno))
            continue;

        n_array_push(cnfldbpkgs, dbpkg_new(recno, h));
	break;	
    }
    rpmdbFreeIterator(mi);
    
#else /* HAVE_RPM_4_0 */

    dbiIndexSet matches;
    int rc;

    matches.count = 0;
    matches.recs = NULL;
    rc = rpmdbFindByFile(db, path, &matches);

    if (rc != 0) {
        if (rc < 0) 
            rpm_die();
        
        
    } else {
        int i;
        for (i = 0; i < matches.count; i++) {
            unsigned recno = matches.recs[i].recOffset;
            Header h;
            
            if (unistdbpkgs && dbpkg_array_has(unistdbpkgs, recno))
                continue;
            
            if ((h = rpmdbGetRecord(db, recno)) == NULL)
                rpm_die();
            
            
            //headerGetEntry(h, RPMTAG_NAME, NULL, (void *)&name, NULL);
            //if (strcmp(name, pkg->name) != 0)
            
            n_array_push(cnfldbpkgs, dbpkg_new(recno, h));
            headerFree(h);
        }
    }
#endif	/* !HAVE_RPM_4_0 */

    if (n_array_size(cnfldbpkgs) == 0) {
        n_array_free(cnfldbpkgs);
        cnfldbpkgs = NULL;
    }
    
    return cnfldbpkgs;
}



static
int lookup_pkg(rpmdb db, const struct capreq *req, tn_array *unistdbpkgs)
{
    int rc = 0;

#ifdef HAVE_RPM_4_0
    rpmdbMatchIterator mi;
    Header h;

    mi = rpmdbInitIterator(db, RPMTAG_NAME, capreq_name(req), 0);
    while((h = rpmdbNextIterator(mi)) != NULL) {
	unsigned int recno = rpmdbGetIteratorOffset(mi);

        if (unistdbpkgs && dbpkg_array_has(unistdbpkgs, recno))
	    continue;
        
 	if (header_evr_match_req(h, req)) {
	    rc = 1;
	    break;
	}
    }
    rpmdbFreeIterator(mi);
    
#else /* HAVE_RPM_4_0 */
    dbiIndexSet matches;

    matches.count = 0;
    matches.recs = NULL;
    rc = rpmdbFindPackage(db, capreq_name(req), &matches);

    if (rc != 0) {
        if (rc < 0)
            rpm_die();
        rc = 0;
        
    } else if (rc == 0) {
        Header h;
        int i;
        
        for (i = 0; i < matches.count; i++) {
            unsigned recno = matches.recs[i].recOffset;
            
            if (unistdbpkgs && dbpkg_array_has(unistdbpkgs, recno))
                continue;

            if ((h = rpmdbGetRecord(db, recno)) == NULL)
                rpm_die();
            
            if (header_evr_match_req(h, req)) {
                rc = 1;
                headerFree(h);
                break;
            }

            headerFree(h);
        }
        
        dbiFreeIndexRecord(matches);
    }
#endif /* HAVE_RPM_4_0 */

    return rc;
}

static
int lookup_file(rpmdb db, const struct capreq *req, tn_array *unistdbpkgs)
{
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    int finded = 0;
    
    rpmdb_it_init(db, &it, RPMITER_FILE, capreq_name(req));
    while((dbpkg = rpmdb_it_get(&it)) != NULL) {
        if (n_array_bsearch(unistdbpkgs, dbpkg))
	    continue;
   	finded = 1;
	break;	
    }
    rpmdb_it_destroy(&it);
    return finded;
}


static
int lookup_cap(rpmdb db, const struct capreq *req, int strict,
               tn_array *unistdbpkgs)
{
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    int rc = 0;
    
    rpmdb_it_init(db, &it, RPMITER_CAP, capreq_name(req));
    while ((dbpkg = rpmdb_it_get(&it)) != NULL) {
        if (unistdbpkgs && n_array_bsearch(unistdbpkgs, dbpkg))
            continue;
        
        if (header_cap_match_req(dbpkg->h, req, strict)) {
            rc = 1;
            break;
        }
    }
    rpmdb_it_destroy(&it);
    return rc;
}


static
int header_evr_match_req(Header h, const struct capreq *req)
{
    struct pkg  pkg;
    uint32_t    *epoch;
    int         rc;
    
    headerNVR(h, (void*)&pkg.name, (void*)&pkg.ver, (void*)&pkg.rel);
    if (pkg.name == NULL || pkg.ver == NULL || pkg.rel == NULL) {
        log(LOGERR, "headerNVR failed\n");
        return 0;
    }

    if (headerGetEntry(h, RPMTAG_EPOCH, &rc, (void *)&epoch, NULL)) 
        pkg.epoch = *epoch;
    else
        pkg.epoch = 0;

    if (pkg_evr_match_req(&pkg, req))
        return 1;

    return 0;
}


static
int header_cap_match_req(Header h, const struct capreq *req, int strict)
{
    void        *saved_allocfn, *saved_freefn;
    struct pkg  pkg;
    int         rc;

    
    rc = 0;
    /* do not alloc on capreq obstack */
    set_capreq_allocfn(malloc, free, &saved_allocfn, &saved_freefn);

    pkg.caps = capreq_arr_new();
    get_pkg_caps(pkg.caps, h);
    
    if (n_array_size(pkg.caps) > 0) {
        n_array_sort(pkg.caps);
        rc = pkg_caps_match_req(&pkg, req, strict);
    }

    n_array_free(pkg.caps);
    set_capreq_allocfn(saved_allocfn, saved_freefn, NULL, NULL);

    return rc;
}

    
#if 0 
static
int is_req_match(rpmdb db, dbiIndexSet matches, const struct capreq *req) 
{
    Header h;
    int i, rc;

    rc = 0;
    for (i = 0; i < matches.count; i++) 
        if ((h = rpmdbGetRecord(db, matches.recs[i].recOffset))) {
            if (header_match_req(h, req, strict)) {
                rc = 1;
                headerFree(h);
                break;
            }
            headerFree(h);
        }

    dbiFreeIndexRecord(matches);
    return rc;
}
#endif


int rpm_dbmatch_req(rpmdb db, const struct capreq *req, int strict,
                    tn_array *unistallrnos) 
{
    int rc;
    int is_file;

    is_file = (*capreq_name(req) == '/' ? 1:0);

    if (!is_file && lookup_pkg(db, req, unistallrnos))
        return 1;
    
    rc = lookup_cap(db, req, strict, unistallrnos);
    if (rc)
        return 1;
    
    if (is_file && lookup_file(db, req, unistallrnos))
        return 1;
    
    return 0;
}


/*
 * Installation 
 */
static void progress(const unsigned long amount, const unsigned long total) 
{
    static unsigned long prev_v = 0, vv = 0;
    
    if (amount && amount == total) { /* last notification */
        msg(1, "_. (%ld kB)\n", total/1024);

    } else if (amount == 0) {     /* first notification */
        msg(1, "_.");
        vv = 0;
        prev_v = 0;
        
    } else if (total == 0) {     /* impossible */
        assert(0);

    } else {
        unsigned long i;
        unsigned long v = amount * 60 / total;
        for (i=prev_v; i<v; i++)
            msg(1, "_.");
        prev_v = v;
    }
}


static void *install_cb(const Header arg __attribute__((unused)),
                        const rpmCallbackType op, 
                        const unsigned long amount, 
                        const unsigned long total,
                        const void *pkgpath,
                        void *data __attribute__((unused)))
{
#ifdef RPM_V4    
    Header h = arg;
#endif    
    void *rc = NULL;
    
 
    switch (op) {
        case RPMCALLBACK_INST_OPEN_FILE:
        case RPMCALLBACK_INST_CLOSE_FILE:
            n_assert(0);
            break;

        case RPMCALLBACK_INST_START:
            msg(1, "Installing %s\n", n_basenam(pkgpath));
            progress(amount, total);
            break;

        case RPMCALLBACK_INST_PROGRESS:
            progress(amount, total);
            break;

        default:
                                /* do nothing */
    }
    
    return rc;
}	


int rpm_install(rpmdb db, const char *rootdir, const char *path,
                unsigned filterflags, unsigned transflags, unsigned instflags)
{
    rpmTransactionSet rpmts = NULL;
    rpmProblemSet probs = NULL;
    int issrc;
    struct vfile *vf;
    int rc;
    Header h = NULL;


    if (rootdir == NULL)
        rootdir = "";

    if ((vf = vfile_open(path, VFT_RPMIO, VFM_RO)) == NULL)
        return 0;
    
    rc = rpmReadPackageHeader(vf->vf_fdt, &h, &issrc, NULL, NULL);
    
    if (rc != 0) {
        switch (rc) {
            case 1:
                log(LOGERR, "%s: does not appear to be a RPM package\n", path);
                goto l_err;
                break;
                
            default:
                log(LOGERR, "%s: cannot be installed (hgw why)\n", path);
                goto l_err;
                break;
        }
        n_assert(0);
        
    } else {
        if (issrc) {
            log(LOGERR, "%s: pakiet�w �r�d�owych nie prowadzimy\n", path);
            goto l_err;
        }
        
        rpmts = rpmtransCreateSet(db, rootdir);
        rc = rpmtransAddPackage(rpmts, h, vf->vf_fdt, path, 
                                (instflags & INSTALL_UPGRADE) != 0, NULL);
        
        headerFree(h);	
        h = NULL;
        
        switch(rc) {
            case 0:
                break;
                
            case 1:
                log(LOGERR, "%s: rpm read error\n", path);
                goto l_err;
                break;
                
                
            case 2:
		log(LOGERR, "%s requires a newer version of RPM\n", path);
                goto l_err;
                break;
                
            default:
                log(LOGERR, "%s: rpmtransAddPackage() failed\n", path);
                goto l_err;
                break;
        }

        if ((instflags & INSTALL_NODEPS) == 0) {
            struct rpmDependencyConflict *conflicts;
            int numConflicts = 0;
            
            if (rpmdepCheck(rpmts, &conflicts, &numConflicts) != 0) {
                log(LOGERR, "%s: rpmdepCheck() failed\n", path);
                goto l_err;
            }
            
                
            if (conflicts) {
                log(LOGERR, "%s: failed dependencies:\n", path);
                printDepProblems(log_stream(), conflicts, numConflicts);
                rpmdepFreeConflicts(conflicts, numConflicts);
                goto l_err;
            }
        }

	rc = rpmRunTransactions(rpmts, install_cb,
                                (void *) ((long)instflags), 
                                NULL, &probs, transflags, filterflags);

        if (rc != 0) {
            if (rc > 0) {
                log(LOGERR, "%s: installation failed:\n", path);
                rpmProblemSetPrint(log_stream(), probs);
                goto l_err;
            } else {
                //log(LOGERR, "%s: installation failed (hgw why)\n", path);
            }
        }
    }

    
    vfile_close(vf);
    if (probs) 
        rpmProblemSetFree(probs);
    rpmtransFree(rpmts);
    return 1;
    
 l_err:
    vfile_close(vf);

    if (probs) 
        rpmProblemSetFree(probs);
    
    if (rpmts)
        rpmtransFree(rpmts);

    if (h)
        headerFree(h);

    return 0;
}


int rpm_dbmap(rpmdb db,
              void (*mapfn)(unsigned recno, void *header, void *arg),
              void *arg) 
{
    int n = 0;
    Header h;

#ifdef HAVE_RPM_4_0
    rpmdbMatchIterator mi;

    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, NULL, 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	unsigned int recno = rpmdbGetIteratorOffset(mi);
	mapfn(recno, h, arg);
	n++;
    }
    rpmdbFreeIterator(mi);
#else	/* !HAVE_RPM_4_0 */
    int recno;

    recno = rpmdbFirstRecNum(db);
    if (recno == 0)
        return 0;

    if (recno < 0)
        rpm_die();
    
    while (recno > 0) {
        if ((h = rpmdbGetRecord(db, recno))) {
            mapfn(recno, h, arg);
            n++;
            headerFree(h);
        }
        
        recno = rpmdbNextRecNum(db, recno);
    }
#endif	/* !HAVE_RPM_4_0 */
    
    return n;
}


static
int get_pkgs_requires_capn(rpmdb db, const char *capname,
                           tn_array *dbpkgs, tn_array *unistdbpkgs) 
{
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    int n = 0;
    
    rpmdb_it_init(db, &it, RPMITER_REQ, capname);
    while ((dbpkg = rpmdb_it_get(&it)) != NULL) {
        
        if (n_array_bsearch(unistdbpkgs, dbpkg))
	    continue;
        
        if (n_array_bsearch(dbpkgs, dbpkg))
	    continue;

        msg(1, "%s <- %s\n", capname, dbpkg_snprintf_s(dbpkg));
        n_array_push(dbpkgs, dbpkg_new(dbpkg->recno, dbpkg->h));
        n++;
    }
    rpmdb_it_destroy(&it);
    return n;
}


static 
int get_pkgs_requires_pkgfiles(rpmdb db, tn_array *dbpkgs,
                               struct dbpkg *dbpkg, tn_array *excldbpkgs) 
{
    int t1, t2, t3, c1, c2, c3;
    char **names = NULL, **dirs = NULL;
    int32_t   *diridxs;
    char      path[PATH_MAX], *prevdir;
    int       *dirlens;
    int       i;
    Header    h;

    h = dbpkg->h;
    n_assert(h);
    
    if (!headerGetEntry(h, RPMTAG_BASENAMES, (void*)&t1, (void*)&names, &c1))
        return 0;

    n_assert(t1 == RPM_STRING_ARRAY_TYPE);
    if (!headerGetEntry(h, RPMTAG_DIRNAMES, (void*)&t2, (void*)&dirs, &c2))
        goto l_endfunc;
    
    n_assert(t2 == RPM_STRING_ARRAY_TYPE);
    if (!headerGetEntry(h, RPMTAG_DIRINDEXES, (void*)&t3,(void*)&diridxs, &c3))
        goto l_endfunc;
    n_assert(t3 == RPM_INT32_TYPE);

    n_assert(c1 == c3);

    dirlens = alloca(sizeof(*dirlens) * c2);
    for (i=0; i < c2; i++) 
        dirlens[i] = strlen(dirs[i]);
    
    prevdir = NULL;
    for (i=0; i < c1; i++) {
        register int diri = diridxs[i];
        if (prevdir == dirs[diri]) {
            n_strncpy(&path[dirlens[diri]], names[i], PATH_MAX-dirlens[diri]);
        } else {
            char *endp;
            
            endp = n_strncpy(path, dirs[diri], sizeof(path));
            n_strncpy(endp, names[i], sizeof(path) - (endp - path));
            prevdir = dirs[diri];
        }

        get_pkgs_requires_capn(db, path, dbpkgs, excldbpkgs);
    }
    
 l_endfunc:
    
    if (c1 && names)
        rpm_headerEntryFree(names, t1);

    if (c2 && dirs)
        rpm_headerEntryFree(dirs, t2);

    return 1;
}


int rpm_get_pkg_pkgreqs(rpmdb db, tn_array *dbpkgs, struct dbpkg *dbpkg,
                        tn_array *unistdbpkgs)
{
    tn_array *caps;
    char *pkgname = NULL;
    int i;

    n_assert(dbpkg->h);
    
    headerGetEntry(dbpkg->h, RPMTAG_NAME, NULL, (void *)&pkgname, NULL);
    n_assert(pkgname);
    get_pkgs_requires_capn(db, pkgname, dbpkgs, unistdbpkgs);

    caps = capreq_arr_new();
    get_pkg_caps(caps, dbpkg->h);
    
    for (i=0; i<n_array_size(caps); i++) {
        struct capreq *cap = n_array_nth(caps, i);
        get_pkgs_requires_capn(db, capreq_name(cap), dbpkgs, unistdbpkgs);
    }
    
    n_array_free(caps);
    get_pkgs_requires_pkgfiles(db, dbpkgs, dbpkg, unistdbpkgs);
    return 0;
}


int rpm_get_obsoletedby_cap(rpmdb db, tn_array *dbpkgs, struct capreq *cap)
{
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    int n = 0;
    
    rpmdb_it_init(db, &it, RPMITER_NAME, capreq_name(cap));
    while ((dbpkg = rpmdb_it_get(&it)) != NULL) {
        if (n_array_bsearch(dbpkgs, dbpkg))
	    continue;

        if (header_evr_match_req(dbpkg->h, cap)) {
            n_array_push(dbpkgs, dbpkg_new(dbpkg->recno, dbpkg->h));
            n_array_sort(dbpkgs);
            n++;
        }
    }
    rpmdb_it_destroy(&it);
    return n;
}


static
int cmp_evr_header2pkg(Header h, const struct pkg *pkg)
{
    struct pkg  tmpkg;
    uint32_t    *epoch;
    int         rc;
    
    headerNVR(h, (void*)&tmpkg.name, (void*)&tmpkg.ver, (void*)&tmpkg.rel);
    if (tmpkg.name == NULL || tmpkg.ver == NULL || tmpkg.rel == NULL) {
        log(LOGERR, "headerNVR failed\n");
        return 0;
    }

    if (headerGetEntry(h, RPMTAG_EPOCH, &rc, (void *)&epoch, NULL)) 
        tmpkg.epoch = *epoch;
    else
        tmpkg.epoch = 0;

    return pkg_cmp_evr(pkg, &tmpkg);
}



int rpm_is_pkg_installed(rpmdb db, const struct pkg *pkg, int *cmprc,
                         struct dbpkg *dbpkgp)
{
    int count = 0;
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    
    rpmdb_it_init(db, &it, RPMITER_NAME, pkg->name);
    count = rpmdb_it_get_count(&it);
    if (count > 0 && (cmprc || dbpkg)) {
        dbpkg = rpmdb_it_get(&it);

        if (cmprc)
            *cmprc = cmp_evr_header2pkg(dbpkg->h, pkg);
        
        if (dbpkgp) {
            dbpkgp->recno = dbpkg->recno;
            dbpkgp->h = headerLink(dbpkg->h);
        }
    }

    rpmdb_it_destroy(&it);
    return count;
}


tn_array *rpm_get_conflicted_dbpkgs(rpmdb db, const struct capreq *cap,
                                    tn_array *unistdbpkgs)
{
    tn_array *dbpkgs = NULL;
    struct rpmdb_it it;
    const struct dbpkg *dbpkg;
    

    rpmdb_it_init(db, &it, RPMITER_CNFL, capreq_name(cap));
    while ((dbpkg = rpmdb_it_get(&it))) {
        if (dbpkg_array_has(unistdbpkgs, dbpkg->recno))
            continue;

        if (dbpkgs == NULL)
            dbpkgs = dbpkg_array_new(4);
            
        n_array_push(dbpkgs, dbpkg_new(dbpkg->recno, dbpkg->h));
    }

    return dbpkgs;
}

