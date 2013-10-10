#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <libnvpair.h>

#include <libfstyp_module.h>

#include <exfat/exfat.h>

extern int exfat_log_enabled;

typedef struct fstyp_exfat {
    int           			fd;
    struct exfat_dev*  	dev;
    struct exfat_super_block*  sb;
    nvlist_t      			*attr;
} fstyp_exfat_t;

int fstyp_mod_init(int fd, off64_t offset, fstyp_mod_handle_t *handle);
void    fstyp_mod_fini(fstyp_mod_handle_t handle);
int fstyp_mod_ident(fstyp_mod_handle_t handle);
int fstyp_mod_get_attr(fstyp_mod_handle_t handle, nvlist_t **attrp);
int fstyp_mod_dump(fstyp_mod_handle_t handle, FILE *fout, FILE *ferr);

static int exfat_get_attr(fstyp_exfat_t *h);

int
fstyp_mod_init(int fd, off64_t offset, fstyp_mod_handle_t *handle)
{
	fstyp_exfat_t *h;
	struct exfat_dev*  	dev;
    
    if (offset != 0) {
        return (FSTYP_ERR_OFFSET);
    }

    if((dev = malloc(sizeof(struct exfat_dev))) == NULL) {
    	return (FSTYP_ERR_NOMEM);
    }

    /* Supress logging to stdout */
    exfat_log_enabled = 0;

    dev->part_num = 0;
	dev->part_offset = 0;
	dev->part_size = 0;

    dev->fd   = fd;
    dev->mode = EXFAT_MODE_RO;
    dev->size = exfat_seek(dev, 0, SEEK_END);

    if(dev->size <= 0) {
    	free(dev);
    	return	FSTYP_ERR_IO;
    }

    exfat_seek(dev, 0, SEEK_SET);

    if ((h = calloc(1, sizeof (fstyp_exfat_t))) == NULL) {
	   free(dev);
	   return (FSTYP_ERR_NOMEM);
    }

    h->dev = dev;
	h->attr = NULL;
	h->sb = NULL;
	h->fd = fd;

    *handle = (fstyp_mod_handle_t)h;
    return (0);
}

void
fstyp_mod_fini(fstyp_mod_handle_t handle)
{
	fstyp_exfat_t *h = (fstyp_exfat_t *)handle;
    
	if (h->dev != NULL) {
		free(h->dev);
		h->dev = NULL;
	}
        
    if (h->attr != NULL) {
        nvlist_free(h->attr);
        h->attr = NULL;
    }
    
    free(h);
}

int
fstyp_mod_ident(fstyp_mod_handle_t handle)
{
	fstyp_exfat_t *h = (fstyp_exfat_t *)handle;
    
    if(h->sb == NULL) {
    	h->sb = exfat_find_sb(h->dev);

        if (h->sb == NULL) {
            free(h);
            return 1;
        }
    }

    return 0;
}

int
fstyp_mod_get_attr(fstyp_mod_handle_t handle, nvlist_t **attrp)
{
	fstyp_exfat_t *h = (fstyp_exfat_t *)handle;
    int error;

    if (h->attr == NULL) {
        if (nvlist_alloc(&h->attr, NV_UNIQUE_NAME_TYPE, 0)) {
            return (FSTYP_ERR_NOMEM);
        }
        if ((error = exfat_get_attr(h)) != 0) {
            nvlist_free(h->attr);
            h->attr = NULL;
            return (error);
        }
    }

    *attrp = h->attr;
    return (0);
}

#define FSTYP_NO_CONV
#define FSTYP_DUMP(fmt, field, conv)		fprintf(fout, #field ": %" fmt "\n", conv(h->sb->field))
#define FSTYP_DUMP2(fmt, f1, f2, conv)		fprintf(fout, #f1 ": %" fmt "\t" #f2 ": %" fmt "\n",	\
												conv(h->sb->f1), conv(h->sb->f2))


int
fstyp_mod_dump(fstyp_mod_handle_t handle, FILE *fout, FILE *ferr)
{
	fstyp_exfat_t *h = (fstyp_exfat_t *)handle;

	if(h->sb == NULL) {
		return 1;
	}

    fprintf(fout, "major: %d\tminor: %d\n", h->sb->version.major, h->sb->version.minor);
    FSTYP_DUMP2(PRId64, sector_start, sector_count, le64_to_cpu);
    FSTYP_DUMP2(PRId32, fat_sector_start, fat_sector_count, le32_to_cpu);
    FSTYP_DUMP2(PRId32, cluster_sector_start, cluster_count, le32_to_cpu);
    FSTYP_DUMP (PRId32, rootdir_cluster, le32_to_cpu);
    FSTYP_DUMP (PRIx32, volume_serial, le32_to_cpu);
    FSTYP_DUMP (PRIx16, volume_state, le16_to_cpu);

    return 0;
}

static int exfat_get_attr(fstyp_exfat_t *h) {
    if(h->sb == NULL) {
        return 1;
    }
    
    (void) nvlist_add_uint64(h->attr, "sector_count",  le64_to_cpu(h->sb->sector_count));
    (void) nvlist_add_uint32(h->attr, "cluster_count", le32_to_cpu(h->sb->cluster_count));
    (void) nvlist_add_uint32(h->attr, "serial", le32_to_cpu(h->sb->volume_serial));
    
    (void) nvlist_add_uint8(h->attr, "ver_minor", h->sb->version.minor);
    (void) nvlist_add_uint8(h->attr, "ver_major", h->sb->version.major);

    return 0;
}
