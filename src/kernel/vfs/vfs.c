/*
 * kernel/vfs/vfs.c
 *
 * Copyright(c) 2007-2014 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//#include <xboot.h>
#include <vfs/vfs.h>

/** file descriptor structure */
struct file {
	spinlock_t f_lock;	/* file lock */
	u32_t f_flags;			/* open flag */
	loff_t f_offset;		/* current position in file */
	struct vnode *f_vnode;		/* vnode */
};

/* size of vnode hash table, must power 2 */
#define VFS_VNODE_HASH_SIZE		(32)

struct vfs_ctrl {
	spinlock_t fs_list_lock;
	struct list_head fs_list;
	spinlock_t mnt_list_lock;
	struct list_head mnt_list;
	spinlock_t vnode_list_lock[VFS_VNODE_HASH_SIZE];
	struct list_head vnode_list[VFS_VNODE_HASH_SIZE];
	spinlock_t fd_bmap_lock;
	unsigned long *fd_bmap;
	struct file fd[VFS_MAX_FD];
//	struct vmm_notifier_block bdev_client;
};

static struct vfs_ctrl vfsc;

/** Compare two path strings and return matched length. */
static int count_match(const char *path, char *mount_root)
{
	int len = 0;

	while (*path && *mount_root) {
		if ((*path++) != (*mount_root++))
			break;
		len++;
	}

	if (*mount_root != '\0') {
		return 0;
	}

	if ((len == 1) && (*(path - 1) == '/')) {
		return 1;
	}

	if ((*path == '\0') || (*path == '/')) {
		return len;
	}

	return 0;
}

static int vfs_findroot(const char *path, struct mount **mp, char **root)
{
	struct mount *m, *tmp;
	int len, max_len = 0;

	if (!path || !mp || !root) {
		return -1;
	}

	/* find mount point from nearest path */
	m = NULL;

	vmm_mutex_lock(&vfsc.mnt_list_lock);

	list_for_each_entry(tmp, &vfsc.mnt_list, m_link) {
		len = count_match(path, tmp->m_path);
		if (len > max_len) {
			max_len = len;
			m = tmp;
		}
	}

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	if (m == NULL) {
		return -1;
	}

	*root = (char *)(path + max_len);
	while (**root == '/') {
		(*root)++;
	}
	*mp = m;

	return 0;
}

static int vfs_fd_alloc(void)
{
	int i, ret = -1;

	vmm_mutex_lock(&vfsc.fd_bmap_lock);

	for (i = 0; i < VFS_MAX_FD; i++) {
/*		if (!bitmap_isset(vfsc.fd_bmap, i)) {
			bitmap_setbit(vfsc.fd_bmap, i);
			ret = i;
			break;
		}*/
	}

	vmm_mutex_unlock(&vfsc.fd_bmap_lock);

	return ret;
}

static void vfs_fd_free(int fd)
{
	if (-1 < fd && fd < VFS_MAX_FD) {
		vmm_mutex_lock(&vfsc.fd_bmap_lock);

/*		if (bitmap_isset(vfsc.fd_bmap, fd)) {
			vmm_mutex_lock(&vfsc.fd[fd].f_lock);
			vfsc.fd[fd].f_flags = 0;
			vfsc.fd[fd].f_offset = 0;
			vfsc.fd[fd].f_vnode = NULL;
			vmm_mutex_unlock(&vfsc.fd[fd].f_lock);
			bitmap_clearbit(vfsc.fd_bmap, fd);
		}*/

		vmm_mutex_unlock(&vfsc.fd_bmap_lock);
	}
}

static struct file *vfs_fd_to_file(int fd)
{
	return (-1 < fd && fd < VFS_MAX_FD) ? &vfsc.fd[fd] : NULL;
}

static u32_t vfs_vnode_hash(struct mount *m, const char *path)
{
	u32_t val = 0;

	if (path) {
		while (*path) {
			val = ((val << 5) + val) + *path++;
		}
	}

	return (val ^ (u32_t)(unsigned long)m) & (VFS_VNODE_HASH_SIZE - 1);
}


static struct vnode *vfs_vnode_vget(struct mount *m, const char *path)
{
	int err;
	u32_t hash;
	struct vnode *v;

	v = NULL;
	hash = vfs_vnode_hash(m, path);

	if (!(v = vmm_zalloc(sizeof(struct vnode)))) {
		return NULL;
	}

	INIT_LIST_HEAD(&v->v_link);
	INIT_MUTEX(&v->v_lock);
	v->v_mount = m;
	arch_atomic_write(&v->v_refcnt, 1);
	if (strlcpy(v->v_path, path, sizeof(v->v_path)) >=
	    sizeof(v->v_path)) {
		vmm_free(v);
		return NULL;
	}

	/* request to allocate fs specific data for vnode. */
	vmm_mutex_lock(&m->m_lock);
	err = m->m_fs->vget(m, v);
	vmm_mutex_unlock(&m->m_lock);
	if (err) {
		vmm_free(v);
		return NULL;
	}

	arch_atomic_add(&m->m_refcnt, 1);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);
	list_add(&v->v_link, &vfsc.vnode_list[hash]);
	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	return v;
}

static struct vnode *vfs_vnode_lookup(struct mount *m, const char *path)
{
	u32_t hash;
	bool_t found = FALSE;
	struct vnode *v = NULL;

	hash = vfs_vnode_hash(m, path);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);

	list_for_each_entry(v, &vfsc.vnode_list[hash], v_link) {
		if ((v->v_mount == m) &&
		    (!strncmp(v->v_path, path, VFS_MAX_PATH))) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	if (!found) {
		return NULL;
	}

	arch_atomic_add(&v->v_refcnt, 1);

	return v;
}

static void vfs_vnode_vref(struct vnode *v)
{
	arch_atomic_add(&v->v_refcnt, 1);
}

static void vfs_vnode_vput(struct vnode *v)
{
	u32_t hash;

	if (arch_atomic_sub_return(&v->v_refcnt, 1)) {
		return;
	}

	hash = vfs_vnode_hash(v->v_mount, v->v_path);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);
	list_del(&v->v_link);
	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	/* deallocate fs specific data from this vnode */
	vmm_mutex_lock(&v->v_mount->m_lock);
	v->v_mount->m_fs->vput(v->v_mount, v);
	vmm_mutex_unlock(&v->v_mount->m_lock);

	arch_atomic_sub(&v->v_mount->m_refcnt, 1);

	vmm_free(v);
}

static int vfs_vnode_stat(struct vnode *v, struct SAPCE_stat *st)
{
	u32_t mode;

	memset(st, 0, sizeof(struct SAPCE_stat));
/*
	st->st_ino = (u64_t)(unsigned long)v;
	vmm_mutex_lock(&v->v_lock);
	st->st_size = v->v_size;
	mode = v->v_mode & (S_IRWXU|S_IRWXG|S_IRWXO);
	st->st_ctime = v->v_ctime;
	st->st_atime = v->v_atime;
	st->st_mtime = v->v_mtime;
	vmm_mutex_unlock(&v->v_lock);

	switch (v->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return VMM_EFAIL;
	};
	st->st_mode = mode;

	if (v->v_type == VCHR || v->v_type == VBLK)
		st->st_dev = (u64)(unsigned long)v->v_data;

	st->st_uid = 0;
	st->st_gid = 0;*/

	return 0;
}
