#ifndef __NEW_VFS_H__
#define __NEW_VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <spinlock.h>
#include <xboot/list.h>

#define VFS_MAX_PATH		(256)
#define	VFS_MAX_NAME		(64)
#define VFS_MAX_FD			(32)

struct SAPCE_stat;
struct dirent;
struct mount;
struct vnode;
struct filesystem;

struct SAPCE_stat {
	u64_t	st_ino;			/* file serial number */
	loff_t	st_size;     	/* file size */
	u32_t	st_mode;		/* file mode */
	u64_t	st_dev;			/* id of device containing file */
	u32_t	st_uid;			/* user ID of the file owner */
	u32_t	st_gid;			/* group ID of the file's group */
	u64_t	st_ctime;		/* file create time */
	u64_t	st_atime;		/* file access time */
	u64_t	st_mtime;		/* file modify time */
};

enum dirent_type {
	DT_UNK,
	DT_DIR,
	DT_REG,
	DT_BLK,
	DT_CHR,
	DT_FIFO,
	DT_LNK,
	DT_SOCK,
	DT_WHT,
};

/** dirent structure */
struct dirent {
	loff_t d_off;				/* offset in actual directory */
	u16_t d_reclen;				/* length of directory entry */
	enum dirent_type d_type; 	/* type of file */
	char d_name[VFS_MAX_NAME];	/* name must not be longer than this */
};

struct mount {
	struct list_head m_link;	/* link to next mount point */
	struct filesystem * m_fs;	/* mounted filesystem */
	void * m_dev;				/* mounted device */
	char m_path[VFS_MAX_PATH];	/* mounted path */
	u32_t m_flags;				/* mount flag */
	atomic_t m_refcnt;			/* reference count */
	struct vnode * m_root;		/* root vnode */
	struct vnode * m_covered;	/* vnode covered on parent fs */

	spinlock_t m_lock;			/* lock to protect members below m_lock and mount point operations */
	void * m_data;				/* private data for filesystem */
};

enum vnode_type {
	VREG,				/* regular file  */
	VDIR,	   			/* directory */
	VBLK,	    		/* block device */
	VCHR,	    		/* character device */
	VLNK,	    		/* symbolic link */
	VSOCK,	    		/* socks */
	VFIFO,	    		/* fifo */
	VUNK,				/* unknown */
};

enum vnode_flag {
	VNONE,				/* default vnode flag */
	VROOT,	   			/* root of its filesystem */
};

struct vnode {
	struct list_head v_link;		/* link for hash list */
	struct mount *v_mount;			/* mount point pointer */
	atomic_t v_refcnt;				/* reference count */
	char v_path[VFS_MAX_PATH];		/* pointer to path in fs */
	enum vnode_flag v_flags;		/* vnode flags (used by internally by vfs) */
	enum vnode_type v_type;			/* vnode type (set once by filesystem lookup()) */

	spinlock_t v_lock;		/* lock to protect members below v_lock and vnode operations */

	u64_t v_ctime;
	u64_t v_atime;
	u64_t v_mtime;
	u32_t v_mode;			/* vnode permissions (set once by filesystem lookup()) */
	loff_t v_size;			/* file size (updated by filesystem read/write) */
	void * v_data;			/* private data for fs */
};

struct filesystem {
	/* Filesystem name */
	const char *name;

	/* Mount point operations */
	int (*mount)(struct mount *, const char *, u32_t);
	int (*unmount)(struct mount *);
	int (*msync)(struct mount *);
	int (*vget)(struct mount *, struct vnode *);
	int (*vput)(struct mount *, struct vnode *);

	/* Vnode operations */
	size_t (*read)(struct vnode *, loff_t, void *, size_t);
	size_t (*write)(struct vnode *, loff_t, void *, size_t);
	int (*truncate)(struct vnode *, loff_t);
	int (*sync)(struct vnode *);
	int (*readdir)(struct vnode *, loff_t, struct dirent *);
	int (*lookup)(struct vnode *, const char *, struct vnode *);
	int (*create)(struct vnode *, const char *, u32_t);
	int (*remove)(struct vnode *, struct vnode *, const char *);
	int (*rename)(struct vnode *, const char *, struct vnode *, struct vnode *, const char *);
	int (*mkdir)(struct vnode *, const char *, u32_t);
	int (*rmdir)(struct vnode *, struct vnode *, const char *);
	int (*chmod)(struct vnode *, u32_t);
};

#ifdef __cplusplus
}
#endif

#endif /* __NEW_VFS_H__ */
