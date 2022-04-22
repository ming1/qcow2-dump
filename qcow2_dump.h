/*
 * QCOW2 DUMP v0.07
 *
 * Copyright (c) 2016 SANGFOR TECHNOLOGIES CO. LTD.
 *
 * Author: YOUPLUS <zhang_youjia@126.com>
 *
 * This program can be distributed under the terms of the GNU LGPLv2.
 * See the file COPYING.LIB.
 */

//gcc -g -o2 -o qcow2-dump fs_magic.h qcow2_dump.h qcow2_dump.c
//strip qcow2-dump

#ifndef QCOW2_DUMP_H
#define QCOW2_DUMP_H

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/resource.h>


#ifndef UINT64_C
#define UINT64_C(x) x##ULL
#endif

#define ALLOC_CLUSTER_IMRT

#define	INVALID		-1
#define	INT_MAX		2147483647
#define	MIN(a,b)	((a)>(b)?(b):(a))
#define MAX(a,b)	((a)>(b)?(a):(b))
#define	ONE_M		(1024 * 1024)
#define	ONE_G		(1024 * 1024 * 1024)

#define CLUSTER_REUSE_MAX	(8 * ONE_M)

#define ACTIVE_CLUSTER_REUSED_REFCOUNT	2

#ifndef ROUND_UP
#define ROUND_UP(n,d) (((n) + (d) - 1) & -(d))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#define QEMU_PACKED __attribute__((packed))
#define QCOW_MAGIC (('Q' << 24) | ('F' << 16) | ('I' << 8) | 0xfb)

#define SECTOR_ALIGN		512

#define MIN_CLUSTER_BITS	9
#define MAX_CLUSTER_BITS	21
#define QCOW_MAX_SNAPSHOTS	65536

/* 8 MB refcount table is enough for 2 PB images at 64k cluster size
 * (128 GB for 512 byte clusters, 2 EB for 2 MB clusters) */
#define QCOW_MAX_REFTABLE_SIZE 0x800000

/* 32 MB L1 table is enough for 2 PB images at 64k cluster size
 * (128 GB for 512 byte clusters, 2 EB for 2 MB clusters) */
#define QCOW_MAX_L1_SIZE 0x2000000

/* Allow for an average of 1k per snapshot table entry, should be plenty of
 * space for snapshot names and IDs */
#define QCOW_MAX_SNAPSHOTS_SIZE (1024 * QCOW_MAX_SNAPSHOTS)


//引用计数表项掩码
#define REFT_OFFSET_MASK 0xfffffffffffffe00ULL

//L1表项掩码
#define L1E_OFFSET_MASK	 0x00fffffffffffe00ULL

//L2表项掩码
#define L2E_OFFSET_MASK	 0x00fffffffffffe00ULL


/* indicate that the refcount of the referenced cluster is exactly one. */
#define QCOW_OFLAG_COPIED		(1ULL << 63)

/* indicate that the cluster is compressed (they never have the copied flag) */
#define QCOW_OFLAG_COMPRESSED	(1ULL << 62)

/* The cluster reads as all zeros */
#define QCOW_OFLAG_ZERO			(1ULL << 0)

#define QCOW_OFLAG	(QCOW_OFLAG_COPIED | QCOW_OFLAG_COMPRESSED | QCOW_OFLAG_ZERO)

//主机序转大端uint64_t
#define cpu_to_be64 htobe64

//主机序转大端uint32_t
#define cpu_to_be32 htobe32

//主机序转大端uint16_t
#define cpu_to_be16 htobe16

//大端uint64_t转主机序
#define be64_to_cpu be64toh

//大端uint32_t转主机序
#define be32_to_cpu be32toh

//大端uint16_t转主机序
#define be16_to_cpu be16toh

//大端uint64_t转主机序(修改原变量)
#define be64_to_cpus(x) ((*(x)) = be64toh(*(x)))

//大端uint32_t转主机序(修改原变量)
#define be32_to_cpus(x) ((*(x)) = be32toh(*(x)))

//大端uint16_t转主机序(修改原变量)
#define be16_to_cpus(x) ((*(x)) = be16toh(*(x)))


/* Incompatible feature bits */
enum {
	QCOW2_INCOMPAT_DIRTY_BITNR  ,
	QCOW2_INCOMPAT_CORRUPT_BITNR= 1,
	QCOW2_INCOMPAT_DIRTY		= 1 << QCOW2_INCOMPAT_DIRTY_BITNR,
	QCOW2_INCOMPAT_CORRUPT		= 1 << QCOW2_INCOMPAT_CORRUPT_BITNR,

	QCOW2_INCOMPAT_MASK			= QCOW2_INCOMPAT_DIRTY | QCOW2_INCOMPAT_CORRUPT,
};

//是否以彩色输出
enum {
	COLOR_OFF	= 0,
	COLOR_ON	= 1,
};

//是否加锁互斥
enum {
	FLOCK_OFF	= 0,
	FLOCK_ON	= 1,
};

//是否输出索引entry中的QCOW_OFLAG/QCOW_OFLAG_ZERO/QCOW_OFLAG_COMPRESSED标志
enum {
	FLAG_OFF	= 0,
	FLAG_ON		= 1,
};

//是否检查模板的存在
enum {
	ACCESS_OFF	= 0,
	ACCESS_ON	= 1,
};

//是否检查镜像的压缩
enum {
	COMPRESS_OFF= 0,
	COMPRESS_ON	= 1,
};

//是否重建重用簇
enum {
	REUSE_OFF	= 0,
	REUSE_ON	= 1,
};


//设置qcow2头部标志
enum {
	MARK_NONE		= 0,
	MARK_DIRTY		= 1,
	MARK_CORRUPT	= 2,
};

//清除qcow2头部标志
enum {
	CLEAN_NONE		= 0,
	CLEAN_DIRTY		= 1,
	CLEAN_CORRUPT	= 2,
};

//隐藏功能, 检查qcow2镜像是否为metadata/full模式
enum {
	PREALLOC_NONE	= 0,
	PREALLOC_CHECK	= 1,
};

//-m info  查看镜像头等基本信息
//-m check 精简输出, 只输出必要信息用于判断镜像是否损坏(元数据是否不对齐/引用计数是否有误等)
//-m error 精简输出 + 输出元数据出错的位置(正常元数据不输出)
//-m dump  输出所有元数据
enum {
	M_INFO	= 0,
	M_CHECK	= 1,
	M_ERROR	= 2,
	M_DUMP	= 3,
	M_EDIT	= 4,
	M_COPY	= 5,
};

//修复引用计数泄漏/错误操作
enum {
	FIX_NONE	= 0,
	FIX_CHECK	= 1,
	FIX_LEAKS	= 2,
	FIX_ERRORS	= 3,
	FIX_ALL		= 4,
};

//回滚快照操作
enum {
	REVERT_NONE	= 0,
	REVERT_APPLY= 1,
};

//删除快照操作
enum {
	SN_NONE		= 0,
	SN_DELETE	= 1,
	SN_EXCLUDE	= 2,
};

//O_SNAPSHOT 输出L1/L2索引表元数据(active snapshot/inactive snapshot/all)
//O_REFCOUNT 输出引用计数表元数据
//O_ALL      输出L1/L2索引表 + 引用计数表
enum {
	O_SNAPSHOT  = 0,
	O_REFCOUNT	= 1,
	O_ALL		= 2,
};

enum {
	S_IMAGE_GOOD		= 0,	//镜像完好
	S_IMAGE_LEAK		= 1,	//引用计数泄漏
	S_IMAGE_COPIED		= 2,	//引用计数与COPIED标志不匹配错误(没有引用计数错误情况下)
	S_IMAGE_METADATA	= 3,	//镜像完好, 但是镜像头部被置了corrupt标记
	S_IMAGE_WRONG		= 4,	//引用计数错误
	S_IMAGE_SN_HEADER	= 5,	//镜像快照头损坏
	S_IMAGE_CORRUPT		= 6,	//镜像损坏
	S_IMAGE_FLAG		= 7,	//镜像头部被置了corrupt标记
	S_IMAGE_OTHER		= 8,
};

//copy from qemu
typedef struct QCowHeader{
	uint32_t magic;
	uint32_t version;
	uint64_t backing_file_offset;
	uint32_t backing_file_size;
	uint32_t cluster_bits;
	uint64_t size;		/* in bytes */
	uint32_t crypt_method;
	uint32_t l1_size;	/* XXX: save number of clusters instead ? */
	uint64_t l1_table_offset;
	uint64_t refcount_table_offset;
	uint32_t refcount_table_clusters;
	uint32_t nb_snapshots;
	uint64_t snapshots_offset;

	/* The following fields are only valid for version >= 3 */
	uint64_t incompatible_features;
	uint64_t compatible_features;
	uint64_t autoclear_features;

	uint32_t refcount_order;
	uint32_t header_length;
} QEMU_PACKED QCowHeader;

typedef struct QEMU_PACKED QCowSnapshotHeader {
	/* header is 8 byte aligned */
	uint64_t l1_table_offset;

	uint32_t l1_size;
	uint16_t id_str_size;
	uint16_t name_size;

	uint32_t date_sec;
	uint32_t date_nsec;

	uint64_t vm_clock_nsec;

	uint32_t vm_state_size;
	uint32_t extra_data_size; /* for extension */
	/* extra data follows */
	/* id_str follows */
	/* name follows  */
} QCowSnapshotHeader;

typedef struct QEMU_PACKED QCowSnapshotExtraData {
	uint64_t vm_state_size_large;
	uint64_t disk_size;
} QCowSnapshotExtraData;

typedef struct QCowSnapshot {
	uint64_t l1_table_offset;
	uint32_t l1_size;
	char *id_str;
	char *name;
	uint64_t disk_size;
	uint64_t vm_state_size;
	uint32_t date_sec;
	uint32_t date_nsec;
	uint64_t vm_clock_nsec;
} QCowSnapshot;


typedef uint64_t Qcow2GetRefcountFunc(const void *refcount_array, uint64_t index);
typedef void Qcow2SetRefcountFunc(void *refcount_array, uint64_t index, uint64_t value);


//可根据需要设置宏INDEX_CN的大小
#define INDEX_CN	5

typedef struct index_info {
	uint64_t index_offset;
	int l1_index;
	int l2_index;
} index_info;

typedef struct cluster_reuse_info {
	uint64_t cluster_offset;
	index_info l1_l2[INDEX_CN];
	int refcount;
} cluster_reuse_info;

typedef struct qcow2_state {
	int fd;
	int lock;
	int flags;
	int mark;
	int clean;
	int display;
	int mode;
	int reuse;
	int repair;
	int apply;
	int delete;
	int64_t write;
	int64_t dst_offset;
	int64_t src_offset;
	int width;
	int output;
	char* snapshot;
	char* filename;
	int access_base;
	int check_compress;
	int active_corrupt;
	uint32_t snapshot_id;
	uint32_t corrupt_id;

	QCowHeader header;
	int image_status;

	uint64_t fs_type;
	uint64_t disk_size;
	uint64_t seek_end;
	uint32_t cluster_bits;
	uint64_t cluster_size;
	uint32_t cluster_sectors;
	uint32_t l2_bits;
	uint32_t l2_size;
	uint32_t snapshots_size;
	uint32_t l1_vm_state_index;
	uint32_t csize_shift;
	uint32_t csize_mask;
	uint64_t cluster_offset_mask;

	//从镜像读取的实际引用计数[包括引用计数修复之后的refcout]
	uint64_t **refcount_array;
	uint64_t refcount_array_size;

	//用于计算引用计数
	void* refcount_table;
	int64_t nb_clusters;
	int64_t corruptions;
	int64_t leaks;
	int64_t unused;
	int64_t used;
	int64_t rebuild;
	int64_t l1_copied;
	int64_t l2_copied;

	int refcount_bits;
	uint64_t refcount_max;
	int refcount_table_size;
	int refcount_block_bits;
	int refcount_block_size;

	Qcow2GetRefcountFunc *get_refcount;
	Qcow2SetRefcountFunc *set_refcount;

	//用于统计
	int64_t ref_unaligned;
	int64_t ref_invalid;
	int64_t ref_unused;
	int64_t ref_used;
	int64_t entry_miss;

	int64_t l1_unaligned;
	int64_t l1_invalid;
	int64_t l1_unused;
	int64_t l1_used;

	int64_t l2_unaligned;
	int64_t l2_invalid;
	int64_t l2_unused;
	int64_t l2_used;
	int64_t l2_compressed;
	int64_t active_copied;

	int l2_not_prealloc;

	//用于检查预分配/动态分配镜像是否正常.
	int prealloc;
	int not_prealloc;

	//用于检查簇被重新分配的情况
	cluster_reuse_info* cluster_reuse;
	int cluster_reuse_max;
	int cluster_reuse_cn;
	int cluster_reused;

	//模板镜像路径
	char backing_file[1024];
	char *source_file;
} qcow2_state;

#endif
