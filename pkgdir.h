/* $Id$ */
#ifndef  POLDEK_PKGDIR_H
#define  POLDEK_PKGDIR_H

#include <trurl/narray.h>

#include <vfile/vfile.h>

#include "misc.h"

#define PKGDIR_LDFROM_IDX         (1 << 0)
#define PKGDIR_LDFROM_DIR         (1 << 1)
#define PKGDIR_LDFROM_DB          (1 << 2)

#define PKGDIR_LOADED             (1 << 4) /* for idx */

#define PKGDIR_DIFF               (1 << 8)  /* is patch */
#define PKGDIR_PATCHED            (1 << 9)  /* patched  */
#define PKGDIR_UNIQED             (1 << 10) /* passed through pkgdir_uniq() */


/* packages.dir digest */

#define PDIGEST_SIZE DIGEST_SIZE_SHA1

#define PDIGEST_MODE_DEFAULT      (0 << 1)
#define PDIGEST_MODE_v016         (0 << 2)
#define PDIGEST_MODE_v016_COMPAT  (0 << 3)

struct pdigest {
    unsigned  mode;
    char      mdh[PDIGEST_SIZE + 1]; /* header */
    char      mdd[PDIGEST_SIZE + 1]; /* data */
    char      *md;                   /* digest of whole data, v016 compat */
};

struct pkgdir {
    char                 *name;
    char                 *path;            /* path | URL        */
    char                 *idxpath;         /* path | URL        */
    struct pdigest       pdg;
    tn_array             *pkgs;            /* struct *pkg[]     */
    
    tn_array             *depdirs;         /* char *[]          */
    tn_array             *foreign_depdirs; /* depdirs not presented in depdirs,
                                            but presented in other pkgdirs */
    struct pkgroup_idx  *pkgroups;
    struct vfile        *vf;              /* packages.dir.gz  handle */
    struct vfile        *vfmd;            /* packages.dir.mdd handle */
    unsigned            flags;
    time_t              ts;               /* timestamp */

    tn_array            *removed_pkgs;    /* for diffs, removed packages */

    time_t              ts_orig;          /* for pathes, ts of .orig idx */

    char                *mdd_orig;        /* for patched, digest of the current
                                             index (to one is patched to) */ 
};

struct pkgdir *pkgdir_malloc(void);
void pkgdir_free(struct pkgdir *pkgdir);

/*
  if pkgdir source is poldeksindex then loading is 2-phase:
   1) pkgdir_new() opens index and reads its header
   2) pkgdir_load() loads index confents
*/
#define PKGDIR_NEW_VERIFY (1 << 0)
struct pkgdir *pkgdir_new(const char *name, const char *path,
                          const char *pkg_prefix, unsigned flags);

/* ldflags */
#define PKGDIR_LD_SKIPBASTS   (1 << 10) /* don't load capreqs added by poldek */
#define PKGDIR_LD_FULLFLIST   (1 << 11) /* load full file list */
#define PKGDIR_LD_DESC        (1 << 12) /* load pkg info to memory */
#define PKGDIR_LD_NOUNIQ      (1 << 13) /* don't perform pkgdir_uniq() */


/* for mkidx */
#define PKGDIR_LD_RAW        (PKGDIR_LD_FULLFLIST | \
                             PKGDIR_LD_SKIPBASTS | \
                             PKGDIR_LD_DESC)

/* for verification */
#define PKGDIR_LD_VERIFY   (PKGDIR_LD_FULLFLIST | PKGDIR_LD_SKIPBASTS)

int pkgdir_load(struct pkgdir *pkgdir, tn_array *depdirs, unsigned ldflags);


struct pkgdir *pkgdir_load_db(const char *rootdir, const char *path);
struct pkgdir *pkgdir_load_dir(const char *name, const char *path);

#define PKGDIR_CREAT_NODESC   (1 << 0) /* don't save packages user level info */
#define PKGDIR_CREAT_wMD5     (1 << 1) /* create pathname.*md5 too            */
#define PKGDIR_CREAT_asCACHE  (1 << 2) /* don't verify pkgdir strictly        */
#define PKGDIR_CREAT_woTOC    (1 << 3) /* don't create pathname.dir.toc       */
#define PKGDIR_CREAT_wMD      (1 << 4) /* create pathname*.md (v016 compat file) */

int pkgdir_create_idx(struct pkgdir *pkgdir, const char *pathname,
                      unsigned flags);

struct pkgdir *pkgdir_diff(struct pkgdir *pkgdir, struct pkgdir *pkgdir2);
struct pkgdir *pkgdir_patch(struct pkgdir *pkgdir, struct pkgdir *pkgdir2);

int pkgdir_update(struct pkgdir *pkgdir, int *npatches);
int update_whole_pkgdir(const char *path);

int pkgdir_isremote(struct pkgdir *pkgdir);



extern const char *default_pkgidx_name;

#ifdef PKGDIR_INTERNAL

#define FILEFMT_MAJOR 1
#define FILEFMT_MINOR 0

extern const char *pdigest_ext;

#define PDIGEST_SIZEx2 (2 * PDIGEST_SIZE)

int pdigest_init(struct pdigest *pdg, char *mdbuf, int size);

int pdigest_read(struct pdigest *pdg, int fd, const char *path);
int pdigest_readvf(struct pdigest *pdg, struct vfile *vfmd);

int pdigest_verify(struct pdigest *pdg, struct vfile *vf);


int pkgdir_uniq(struct pkgdir *pkgdir);

int i_pkgdir_creat_md5(const char *pathname);
int i_pkgdir_verify_md5(const char *title, const char *pathname);


int i_pkgdir_creat_digest(struct pkgdir *pkgdir, const char *pathname,
                          int with_md);
struct vfile *i_pkgdir_open_digest(const char *path, int vfmode);
//int i_pkgdir_is_uptodate(const char *path, struct pdigest *pdg);

int mkdigest_path(char *path, int size, const char *pathname, const char *ext);



extern const char *pdir_default_pkgdir_name;
extern const char *pdir_packages_incdir;

extern const char *pdir_poldeksindex;
extern const char *pdir_poldeksindex_toc;

extern const char *pdir_tag_depdirs;
extern const char *pdir_tag_ts;
extern const char *pdir_tag_ts_orig;
extern const char *pdir_tag_removed;
extern const char *pdir_tag_endhdr;
extern const char *pdir_tag_endvarhdr;


#endif /* PKGDIR_INTERNAL */

#endif /* POLDEK_PKGDIR_H*/
