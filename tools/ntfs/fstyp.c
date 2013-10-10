#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <libnvpair.h>

#include <libfstyp_module.h>

#include <ntfs-3g/volume.h>
#include <ntfs-3g/device.h>
#include <ntfs-3g/device_io.h>

typedef struct fstyp_ntfs {
    int           fd;
    struct ntfs_device*  dev;
    ntfs_volume*  vol;
    nvlist_t      *attr;
} fstyp_ntfs_t;

int fstyp_mod_init(int fd, off64_t offset, fstyp_mod_handle_t *handle);
void    fstyp_mod_fini(fstyp_mod_handle_t handle);
int fstyp_mod_ident(fstyp_mod_handle_t handle);
int fstyp_mod_get_attr(fstyp_mod_handle_t handle, nvlist_t **attrp);
int fstyp_mod_dump(fstyp_mod_handle_t handle, FILE *fout, FILE *ferr);

static int ntfs_device_fstyp_io_open(struct ntfs_device *dev, int flags);
static int ntfs_device_fstyp_io_close(struct ntfs_device *dev);

static int ntfs_get_attr(fstyp_ntfs_t *h);

struct ntfs_device_operations ntfs_device_fstyp_io_ops;
boolean_t io_ops_initialized = B_FALSE;

int
fstyp_mod_init(int fd, off64_t offset, fstyp_mod_handle_t *handle)
{
    fstyp_ntfs_t *h;
    int* d_private;
    
    if (offset != 0) {
        return (FSTYP_ERR_OFFSET);
    }
    
    d_private = malloc(sizeof(int));
    if(d_private == NULL) {
        return (FSTYP_ERR_NOMEM);
    }

    if ((h = calloc(1, sizeof (fstyp_ntfs_t))) == NULL) {
        free(d_private);
        return (FSTYP_ERR_NOMEM);
    }
    
    h->dev = NULL;
    h->vol = NULL;
	h->attr = NULL;
    h->fd = fd;

    *d_private = fd;
    
    /* Ignore all logs! */
    ntfs_log_set_handler(ntfs_log_handler_null);

    /* XXX: Hack our open routine cause we already have fd */
    if(!io_ops_initialized) {
        memcpy(&ntfs_device_fstyp_io_ops,
               &ntfs_device_default_io_ops,
               sizeof(struct ntfs_device_operations));
        
        ntfs_device_fstyp_io_ops.open  = ntfs_device_fstyp_io_open;
        ntfs_device_fstyp_io_ops.close = ntfs_device_fstyp_io_close;
        
        io_ops_initialized = B_TRUE;
    }
    
    h->dev = ntfs_device_alloc("ntfs", 0, &ntfs_device_fstyp_io_ops, d_private);
    if(h->dev == NULL) {
        free(d_private);
        free(h);
        return (FSTYP_ERR_NOMEM);
    }

    *handle = (fstyp_mod_handle_t)h;
    return (0);
}

void
fstyp_mod_fini(fstyp_mod_handle_t handle)
{
    fstyp_ntfs_t *h = (fstyp_ntfs_t *)handle;
    
    free(h->dev->d_private);
    
    if(h->vol != NULL) {
        ntfs_umount(h->vol, 0);
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
    fstyp_ntfs_t *h = (fstyp_ntfs_t *)handle;
    
    if(h->vol == NULL) {
        h->vol = ntfs_volume_startup(h->dev, NTFS_MNT_RDONLY);
        
        if (h->vol == NULL) {
            free(h);
            return 1;
        }
    }

    return 0;
}

int
fstyp_mod_get_attr(fstyp_mod_handle_t handle, nvlist_t **attrp)
{
    fstyp_ntfs_t *h = (fstyp_ntfs_t *)handle;
    int error;

    if (h->attr == NULL) {
        if (nvlist_alloc(&h->attr, NV_UNIQUE_NAME_TYPE, 0)) {
            return (FSTYP_ERR_NOMEM);
        }
        if ((error = ntfs_get_attr(h)) != 0) {
            nvlist_free(h->attr);
            h->attr = NULL;
            return (error);
        }
    }

    *attrp = h->attr;
    return (0);
}

int
fstyp_mod_dump(fstyp_mod_handle_t handle, FILE *fout, FILE *ferr)
{
    fstyp_ntfs_t *h = (fstyp_ntfs_t *)handle;
    ntfs_volume* vol = h->vol;
    
    fprintf(fout, "Volume Information \n");
    fprintf(fout, "\tName of device: %s\n", vol->dev->d_name);
    fprintf(fout, "\tDevice state: %lu\n", vol->dev->d_state);
    fprintf(fout, "\tVolume Name: %s\n", vol->vol_name);
    fprintf(fout, "\tVolume State: %lu\n", vol->state);
    fprintf(fout, "\tVolume Flags: 0x%04x", (int)vol->flags);
    if (vol->flags & VOLUME_IS_DIRTY)
        fprintf(fout, " DIRTY");
    if (vol->flags & VOLUME_MODIFIED_BY_CHKDSK)
        fprintf(fout, " MODIFIED_BY_CHKDSK");
    fprintf(fout, "\n");
    fprintf(fout, "\tVolume Version: %u.%u\n", vol->major_ver, vol->minor_ver);
    fprintf(fout, "\tSector Size: %hu\n", vol->sector_size);
    fprintf(fout, "\tCluster Size: %u\n", (unsigned int)vol->cluster_size);
    fprintf(fout, "\tIndex Block Size: %u\n", (unsigned int)vol->indx_record_size);
    fprintf(fout, "\tVolume Size in Clusters: %lld\n",
            (long long)vol->nr_clusters);

    fprintf(fout, "MFT Information \n");
    fprintf(fout, "\tMFT Record Size: %u\n", (unsigned int)vol->mft_record_size);
    fprintf(fout, "\tMFT Zone Multiplier: %u\n", vol->mft_zone_multiplier);
    fprintf(fout, "\tMFT Data Position: %lld\n", (long long)vol->mft_data_pos);
    fprintf(fout, "\tMFT Zone Start: %lld\n", (long long)vol->mft_zone_start);
    fprintf(fout, "\tMFT Zone End: %lld\n", (long long)vol->mft_zone_end);
    fprintf(fout, "\tMFT Zone Position: %lld\n", (long long)vol->mft_zone_pos);
    fprintf(fout, "\tCurrent Position in First Data Zone: %lld\n",
            (long long)vol->data1_zone_pos);
    fprintf(fout, "\tCurrent Position in Second Data Zone: %lld\n",
            (long long)vol->data2_zone_pos);
    fprintf(fout, "\tAllocated clusters %lld (%2.1lf%%)\n",
            (long long)vol->mft_na->allocated_size
                >> vol->cluster_size_bits,
            100.0*(vol->mft_na->allocated_size
                >> vol->cluster_size_bits)
                / vol->nr_clusters);
    fprintf(fout, "\tLCN of Data Attribute for FILE_MFT: %lld\n",
            (long long)vol->mft_lcn);
    fprintf(fout, "\tFILE_MFTMirr Size: %d\n", vol->mftmirr_size);
    fprintf(fout, "\tLCN of Data Attribute for File_MFTMirr: %lld\n",
            (long long)vol->mftmirr_lcn);
    fprintf(fout, "\tSize of Attribute Definition Table: %d\n",
            (int)vol->attrdef_len);
    fprintf(fout, "\tNumber of Attached Extent Inodes: %d\n",
            (int)vol->mft_ni->nr_extents);
}

static int ntfs_get_attr(fstyp_ntfs_t *h) {
    if(h->vol == NULL) {
        return 1;
    }
    
    (void) nvlist_add_string(h->attr, "vol_name", h->vol->vol_name);
    (void) nvlist_add_uint32(h->attr, "vol_state", (uint32_t) h->vol->state);
    (void) nvlist_add_uint32(h->attr, "vol_flags", (uint32_t) h->vol->flags);
    
    return 0;
}

static int ntfs_device_fstyp_io_open(struct ntfs_device *dev, int flags) {
    /* Already opened it by fstyp - ignore it */
    NDevSetOpen(dev);
    
    return 0;
}

static int ntfs_device_fstyp_io_close(struct ntfs_device *dev) {
	NDevClearOpen(dev);

    return 0;
}
