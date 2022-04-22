
#ifndef FS_MAGIC_H
#define FS_MAGIC_H

#include <stdint.h>


#ifndef NULL
#define NULL ((void *)0)
#endif

#define ADFS_SUPER_MAGIC		0xadf5
#define AFFS_SUPER_MAGIC		0xadff
#define AFS_SUPER_MAGIC			0x5346414F
#define AUTOFS_SUPER_MAGIC		0x0187
#define CODA_SUPER_MAGIC		0x73757245
#define CRAMFS_MAGIC			0x28cd3d45	/* some random number */
#define CRAMFS_MAGIC_WEND		0x453dcd28	/* magic number with the wrong endianess */
#define DEBUGFS_MAGIC			0x64626720
#define SECURITYFS_MAGIC		0x73636673
#define SELINUX_MAGIC			0xf97cff8c
#define SMACK_MAGIC				0x43415d53	/* "SMAC" */
#define RAMFS_MAGIC				0x858458f6	/* some random number */
#define TMPFS_MAGIC				0x01021994
#define HUGETLBFS_MAGIC 		0x958458f6	/* some random number */
#define SQUASHFS_MAGIC			0x73717368
#define ECRYPTFS_SUPER_MAGIC	0xf15f
#define EFS_SUPER_MAGIC			0x414A53
#define EXT2_SUPER_MAGIC		0xEF53
#define EXT3_SUPER_MAGIC		0xEF53
#define EXT4_SUPER_MAGIC		0xEF53
#define XENFS_SUPER_MAGIC		0xabba1974
#define BTRFS_SUPER_MAGIC		0x9123683E
#define NILFS_SUPER_MAGIC		0x3434
#define F2FS_SUPER_MAGIC		0xF2F52010
#define HPFS_SUPER_MAGIC		0xf995e849
#define ISOFS_SUPER_MAGIC		0x9660
#define JFFS2_SUPER_MAGIC		0x72b6
#define PSTOREFS_MAGIC			0x6165676C
#define EFIVARFS_MAGIC			0xde5e81e4
#define HOSTFS_SUPER_MAGIC		0x00c0ffee

#define MSDOS_SUPER_MAGIC		0x4d44		/* MD */
#define NCP_SUPER_MAGIC			0x564c		/* Guess, what 0x564c is :-) */
#define NFS_SUPER_MAGIC			0x6969
#define OPENPROM_SUPER_MAGIC	0x9fa1
#define QNX4_SUPER_MAGIC		0x002f		/* qnx4 fs detection */
#define QNX6_SUPER_MAGIC		0x68191122	/* qnx6 fs detection */

#define REISERFS_SUPER_MAGIC	0x52654973	/* used by gcc */

#define SMB_SUPER_MAGIC			0x517B
#define CGROUP_SUPER_MAGIC		0x27e0eb

#define STACK_END_MAGIC			0x57AC6E9D

#define V9FS_MAGIC				0x01021997

#define BDEVFS_MAGIC            0x62646576
#define BINFMTFS_MAGIC          0x42494e4d
#define DEVPTS_SUPER_MAGIC		0x1cd1
#define FUTEXFS_SUPER_MAGIC		0xBAD1DEA
#define PIPEFS_MAGIC            0x50495045
#define PROC_SUPER_MAGIC		0x9fa0
#define SOCKFS_MAGIC			0x534F434B
#define SYSFS_MAGIC				0x62656572
#define USBDEVICE_SUPER_MAGIC	0x9fa2
#define MTD_INODE_FS_MAGIC      0x11307854
#define ANON_INODE_FS_MAGIC		0x09041934
#define BTRFS_TEST_MAGIC		0x73727279

#define FUSE_SUPER_MAGIC 		0x65735546
#define CEPH_SUPER_MAGIC		0x00c36400
#define GFS2_MAGIC				0x01161970
#define JFS_SUPER_MAGIC			0x3153464a	/* "JFS1" */
#define NTFS_SB_MAGIC			0x5346544e	/* 'NTFS' */
#define VXFS_SUPER_MAGIC		0xa501FCF5
#define	XFS_SB_MAGIC			0x58465342	/* 'XFSB' */


#define	SFFS_MAGIC				0x53464653	/* 'SFFS' */


struct fs_magic {
	uint64_t magic;
	const char* fs_name;
} fs_type[] = {
	{ ADFS_SUPER_MAGIC,			"adfs" },
	{ AFFS_SUPER_MAGIC,			"affs" },
	{ AFS_SUPER_MAGIC,			"afs" },
	{ AUTOFS_SUPER_MAGIC,		"autofs" },
	{ CODA_SUPER_MAGIC,			"coda" },
	{ CRAMFS_MAGIC,				"cramfs" },
	{ DEBUGFS_MAGIC,			"debugfs" },
	{ SECURITYFS_MAGIC,			"securityfs" },
	{ SELINUX_MAGIC,			"selinux" },
	{ SMACK_MAGIC,				"smack" },
	{ RAMFS_MAGIC,				"ramfs" },
	{ TMPFS_MAGIC,				"tmpfs" },
	{ HUGETLBFS_MAGIC,			"hugetlbfs" },
	{ SQUASHFS_MAGIC,			"squashfs" },
	{ ECRYPTFS_SUPER_MAGIC,		"ecryptfs" },
	{ EFS_SUPER_MAGIC,			"efs" },
//	{ EXT2_SUPER_MAGIC,			"ext2" },
//	{ EXT3_SUPER_MAGIC,			"ext3" },
	{ EXT4_SUPER_MAGIC,			"ext4/ext3/ext2" },
	{ XENFS_SUPER_MAGIC,		"xenfs" },
	{ BTRFS_SUPER_MAGIC,		"btrfs" },
	{ NILFS_SUPER_MAGIC,		"nilfs" },
	{ F2FS_SUPER_MAGIC,			"f2fs" },
	{ HPFS_SUPER_MAGIC,			"hpfs" },
	{ ISOFS_SUPER_MAGIC,		"isofs" },
	{ JFFS2_SUPER_MAGIC,		"jffs2" },
	{ PSTOREFS_MAGIC,			"pstorefs" },
	{ EFIVARFS_MAGIC,			"efivarfs" },
	{ HOSTFS_SUPER_MAGIC,		"hostfs" },
	{ MSDOS_SUPER_MAGIC,		"msdos" },
	{ NCP_SUPER_MAGIC,			"ncp" },
	{ NFS_SUPER_MAGIC,			"nfs" },
	{ OPENPROM_SUPER_MAGIC,		"openprom" },
	{ QNX4_SUPER_MAGIC,			"qnx4" },
	{ QNX6_SUPER_MAGIC,			"qnx6" },
	{ REISERFS_SUPER_MAGIC,		"reisterfs" },
	{ SMB_SUPER_MAGIC,			"smb" },
	{ CGROUP_SUPER_MAGIC,		"cgroup" },
	{ STACK_END_MAGIC,			"stack_end" },
	{ V9FS_MAGIC,				"v9fs" },
	{ BDEVFS_MAGIC,				"bdevfs" },
	{ BINFMTFS_MAGIC,			"binfmtfs" },
	{ DEVPTS_SUPER_MAGIC,		"devpts" },
	{ FUTEXFS_SUPER_MAGIC,		"futexfs" },
	{ PIPEFS_MAGIC,				"pipefs" },
	{ PROC_SUPER_MAGIC,			"proc" },
	{ SOCKFS_MAGIC,				"sockfs" },
	{ SYSFS_MAGIC,				"sysfs" },
	{ USBDEVICE_SUPER_MAGIC,	"usbdevice" },
	{ MTD_INODE_FS_MAGIC,		"mtd_inode_fs" },
	{ ANON_INODE_FS_MAGIC,		"anon_inode_fs" },
	{ BTRFS_TEST_MAGIC,			"btrfs_test" },
	{ FUSE_SUPER_MAGIC,			"fuse" },
	{ CEPH_SUPER_MAGIC,			"ceph" },
	{ GFS2_MAGIC,				"gfs2" },
	{ JFS_SUPER_MAGIC,			"jfs" },
	{ NTFS_SB_MAGIC,			"ntfs" },
	{ VXFS_SUPER_MAGIC,			"vxfs" },
	{ XFS_SB_MAGIC,				"xfs" },

	{ SFFS_MAGIC,				"sffs" },
};

#endif
