/*
	exfat.h (29.08.09)
	Definitions of structures and constants used in exFAT file system
	implementation.

	Copyright (C) 2010-2013  Andrew Nayenko

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EXFAT_H_INCLUDED
#define EXFAT_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <exfat/exfatfs.h>
#include <exfat/version.h>
#include <exfat/byteorder.h>

#define EXFAT_BLK_SIZE		512

#define EXFAT_ERROR_SIZE_T		((size_t)-1)
#define EXFAT_ERROR_OFF_T	((off_t)-1)

#define EXFAT_NAME_MAX 256
#define EXFAT_ATTRIB_CONTIGUOUS 0x10000
#define EXFAT_ATTRIB_CACHED     0x20000
#define EXFAT_ATTRIB_DIRTY      0x40000
#define EXFAT_ATTRIB_UNLINKED   0x80000
#define IS_CONTIGUOUS(node) (((node).flags & EXFAT_ATTRIB_CONTIGUOUS) != 0)
#define SECTOR_SIZE(sb) (1 << (sb).sector_bits)
#define CLUSTER_SIZE(sb) (SECTOR_SIZE(sb) << (sb).spc_bits)
#define CLUSTER_INVALID(c) \
	((c) < EXFAT_FIRST_DATA_CLUSTER || (c) > EXFAT_LAST_DATA_CLUSTER)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DIV_ROUND_UP(x, d) (((x) + (d) - 1) / (d))
#define ROUND_UP(x, d) (DIV_ROUND_UP(x, d) * (d))

#define BMAP_GET(bitmap, index) \
	(((uint8_t*) bitmap)[(index) / 8] & (1u << ((index) % 8)))
#define BMAP_SET(bitmap, index) \
	((uint8_t*) bitmap)[(index) / 8] |= (1u << ((index) % 8))
#define BMAP_CLR(bitmap, index) \
	((uint8_t*) bitmap)[(index) / 8] &= ~(1u << ((index) % 8))

struct exfat_node
{
	struct exfat_node* parent;
	struct exfat_node* child;
	struct exfat_node* next;
	struct exfat_node* prev;

	int references;
	uint32_t fptr_index;
	cluster_t fptr_cluster;
	cluster_t entry_cluster;
	off_t entry_offset;
	cluster_t start_cluster;
	int flags;
	uint64_t size;
	time_t mtime, atime;
	le16_t name[EXFAT_NAME_MAX + 1];
};

enum exfat_mode
{
	EXFAT_MODE_RO,
	EXFAT_MODE_RW,
	EXFAT_MODE_ANY,
};

struct exfat_dev
{
	int fd;
	enum exfat_mode mode;
	off_t size; /* in bytes */

	int   part_num;
	off_t part_offset;
	off_t part_size;
#ifdef USE_UBLIO
	off_t pos;
	ublio_filehandle_t ufh;
#endif
};

struct exfat
{
	struct exfat_dev* dev;
	struct exfat_super_block* sb;
	le16_t* upcase;
	size_t upcase_chars;
	struct exfat_node* root;
	struct
	{
		cluster_t start_cluster;
		uint32_t size;				/* in bits */
		uint8_t* chunk;
		uint32_t chunk_size;		/* in bits */
		bool dirty;
	}
	cmap;
	char label[EXFAT_ENAME_MAX * 6 + 1]; /* a character can occupy up to
											6 bytes in UTF-8 */
	void* zero_cluster;
	int dmask, fmask;
	uid_t uid;
	gid_t gid;
	int ro;
	bool noatime;
};

/* in-core nodes iterator */
struct exfat_iterator
{
	struct exfat_node* parent;
	struct exfat_node* current;
};

struct exfat_human_bytes
{
	uint64_t value;
	const char* unit;
};

typedef void (*exfat_bug_handler_func)(void);

extern int exfat_errors;
extern int exfat_log_enabled;

extern exfat_bug_handler_func exfat_bug_handler;

void exfat_bug(const char* format, ...)
	__attribute__((format(printf, 1, 2)));
void exfat_error(const char* format, ...)
	__attribute__((format(printf, 1, 2)));
void exfat_warn(const char* format, ...)
	__attribute__((format(printf, 1, 2)));
void exfat_debug(const char* format, ...)
	__attribute__((format(printf, 1, 2)));

extern struct exfat_dev* exfat_open(const char* spec, enum exfat_mode mode);
extern int exfat_close(struct exfat_dev* dev);
extern int exfat_fsync(struct exfat_dev* dev);
extern enum exfat_mode exfat_get_mode(const struct exfat_dev* dev);
extern off_t exfat_get_size(const struct exfat_dev* dev);
extern off_t exfat_seek(struct exfat_dev* dev, off_t offset, int whence);
extern ssize_t exfat_read(struct exfat_dev* dev, void* buffer, size_t size);
extern ssize_t exfat_write(struct exfat_dev* dev, const void* buffer, size_t size);
extern int exfat_pread(struct exfat_dev* dev, void* buffer, size_t size,
		off_t offset);
extern int exfat_pwrite(struct exfat_dev* dev, const void* buffer, size_t size,
		off_t offset);
int exfat_cluster_pread(const struct exfat* ef, void* data, size_t cluster);
int exfat_cluster_pwrite(const struct exfat* ef, void* data, size_t cluster);
extern ssize_t exfat_generic_pread(const struct exfat* ef, struct exfat_node* node,
		void* buffer, size_t size, off_t offset);
extern ssize_t exfat_generic_pwrite(struct exfat* ef, struct exfat_node* node,
		const void* buffer, size_t size, off_t offset);

extern int exfat_opendir(struct exfat* ef, struct exfat_node* dir,
		struct exfat_iterator* it);
extern void exfat_closedir(struct exfat* ef, struct exfat_iterator* it);
extern struct exfat_node* exfat_readdir(struct exfat* ef, struct exfat_iterator* it);
extern int exfat_lookup(struct exfat* ef, struct exfat_node** node,
		const char* path);
extern int exfat_split(struct exfat* ef, struct exfat_node** parent,
		struct exfat_node** node, le16_t* name, const char* path);

extern off_t exfat_c2o(const struct exfat* ef, cluster_t cluster);
extern cluster_t exfat_next_cluster(const struct exfat* ef,
		const struct exfat_node* node, cluster_t cluster);
extern cluster_t exfat_advance_cluster(const struct exfat* ef,
		struct exfat_node* node, uint32_t count);
extern void exfat_flush_cmap(struct exfat* ef);
extern int exfat_truncate(struct exfat* ef, struct exfat_node* node, uint64_t size);
extern uint32_t exfat_count_free_clusters(const struct exfat* ef);
extern int exfat_find_used_sectors(const struct exfat* ef, off_t* a, off_t* b);

extern void exfat_stat(const struct exfat* ef, const struct exfat_node* node,
		struct stat* stbuf);
extern int exfat_get_name(const struct exfat_node* node, char* buffer, size_t n);
extern uint16_t exfat_start_checksum(const struct exfat_entry_meta1* entry);
extern uint16_t exfat_add_checksum(const void* entry, uint16_t sum);
extern le16_t exfat_calc_checksum(const struct exfat_entry_meta1* meta1,
		const struct exfat_entry_meta2* meta2, const le16_t* name);
extern uint32_t exfat_vbr_start_checksum(const void* sector, size_t size);
extern uint32_t exfat_vbr_add_checksum(const void* sector, size_t size, uint32_t sum);
extern le16_t exfat_calc_name_hash(const struct exfat* ef, const le16_t* name);
extern void exfat_humanize_bytes(uint64_t value, struct exfat_human_bytes* hb);
extern void exfat_print_info(const struct exfat_super_block* sb,
		uint32_t free_clusters);

int utf16_to_utf8(char* output, const le16_t* input, size_t outsize,
		size_t insize);
int utf8_to_utf16(le16_t* output, const char* input, size_t outsize,
		size_t insize);
size_t utf16_length(const le16_t* str);

extern struct exfat_node* exfat_get_node(struct exfat_node* node);
extern void exfat_put_node(struct exfat* ef, struct exfat_node* node);
extern int exfat_cache_directory(struct exfat* ef, struct exfat_node* dir);
extern void exfat_reset_cache(struct exfat* ef);
extern int exfat_flush_node(struct exfat* ef, struct exfat_node* node);
extern int exfat_unlink(struct exfat* ef, struct exfat_node* node);
extern int exfat_rmdir(struct exfat* ef, struct exfat_node* node);
extern int exfat_mknod(struct exfat* ef, const char* path);
extern int exfat_mkdir(struct exfat* ef, const char* path);
extern int exfat_rename(struct exfat* ef, const char* old_path, const char* new_path);
extern void exfat_utimes(struct exfat_node* node, const struct timespec tv[2]);
extern void exfat_update_atime(struct exfat_node* node);
extern void exfat_update_mtime(struct exfat_node* node);
extern const char* exfat_get_label(struct exfat* ef);
extern int exfat_set_label(struct exfat* ef, const char* label);

extern int exfat_mount(struct exfat* ef, const char* spec, const char* options);
extern void exfat_unmount(struct exfat* ef);

extern time_t exfat_exfat2unix(le16_t date, le16_t time, uint8_t centisec);
extern void exfat_unix2exfat(time_t unix_time, le16_t* date, le16_t* time,
		uint8_t* centisec);
extern void exfat_tzset(void);

extern int exfat_set_partition(struct exfat_dev *dev, int partnum);
extern struct exfat_super_block* exfat_find_sb(struct exfat_dev* dev);

#endif /* ifndef EXFAT_H_INCLUDED */
