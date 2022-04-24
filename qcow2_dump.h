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


//���ü�����������
#define REFT_OFFSET_MASK 0xfffffffffffffe00ULL

//L1��������
#define L1E_OFFSET_MASK	 0x00fffffffffffe00ULL

//L2��������
#define L2E_OFFSET_MASK	 0x00fffffffffffe00ULL


/* indicate that the refcount of the referenced cluster is exactly one. */
#define QCOW_OFLAG_COPIED		(1ULL << 63)

/* indicate that the cluster is compressed (they never have the copied flag) */
#define QCOW_OFLAG_COMPRESSED	(1ULL << 62)

/* The cluster reads as all zeros */
#define QCOW_OFLAG_ZERO			(1ULL << 0)

#define QCOW_OFLAG	(QCOW_OFLAG_COPIED | QCOW_OFLAG_COMPRESSED | QCOW_OFLAG_ZERO)

//������ת���uint64_t
#define cpu_to_be64 htobe64

//������ת���uint32_t
#define cpu_to_be32 htobe32

//������ת���uint16_t
#define cpu_to_be16 htobe16

//���uint64_tת������
#define be64_to_cpu be64toh

//���uint32_tת������
#define be32_to_cpu be32toh

//���uint16_tת������
#define be16_to_cpu be16toh

//���uint64_tת������(�޸�ԭ����)
#define be64_to_cpus(x) ((*(x)) = be64toh(*(x)))

//���uint32_tת������(�޸�ԭ����)
#define be32_to_cpus(x) ((*(x)) = be32toh(*(x)))

//���uint16_tת������(�޸�ԭ����)
#define be16_to_cpus(x) ((*(x)) = be16toh(*(x)))


/* Incompatible feature bits */
enum {
	QCOW2_INCOMPAT_DIRTY_BITNR  ,
	QCOW2_INCOMPAT_CORRUPT_BITNR= 1,
	QCOW2_INCOMPAT_DIRTY		= 1 << QCOW2_INCOMPAT_DIRTY_BITNR,
	QCOW2_INCOMPAT_CORRUPT		= 1 << QCOW2_INCOMPAT_CORRUPT_BITNR,

	QCOW2_INCOMPAT_MASK			= QCOW2_INCOMPAT_DIRTY | QCOW2_INCOMPAT_CORRUPT,
};

//�Ƿ��Բ�ɫ���
enum {
	COLOR_OFF	= 0,
	COLOR_ON	= 1,
};

//�Ƿ��������
enum {
	FLOCK_OFF	= 0,
	FLOCK_ON	= 1,
};

//�Ƿ��������entry�е�QCOW_OFLAG/QCOW_OFLAG_ZERO/QCOW_OFLAG_COMPRESSED��־
enum {
	FLAG_OFF	= 0,
	FLAG_ON		= 1,
};

//�Ƿ���ģ��Ĵ���
enum {
	ACCESS_OFF	= 0,
	ACCESS_ON	= 1,
};

//�Ƿ��龵���ѹ��
enum {
	COMPRESS_OFF= 0,
	COMPRESS_ON	= 1,
};

//�Ƿ��ؽ����ô�
enum {
	REUSE_OFF	= 0,
	REUSE_ON	= 1,
};


//����qcow2ͷ����־
enum {
	MARK_NONE		= 0,
	MARK_DIRTY		= 1,
	MARK_CORRUPT	= 2,
};

//���qcow2ͷ����־
enum {
	CLEAN_NONE		= 0,
	CLEAN_DIRTY		= 1,
	CLEAN_CORRUPT	= 2,
};

//���ع���, ���qcow2�����Ƿ�Ϊmetadata/fullģʽ
enum {
	PREALLOC_NONE	= 0,
	PREALLOC_CHECK	= 1,
};

//-m info  �鿴����ͷ�Ȼ�����Ϣ
//-m check �������, ֻ�����Ҫ��Ϣ�����жϾ����Ƿ���(Ԫ�����Ƿ񲻶���/���ü����Ƿ������)
//-m error ������� + ���Ԫ���ݳ����λ��(����Ԫ���ݲ����)
//-m dump  �������Ԫ����
enum {
	M_INFO	= 0,
	M_CHECK	= 1,
	M_ERROR	= 2,
	M_DUMP	= 3,
	M_EDIT	= 4,
	M_COPY	= 5,
};

//�޸����ü���й©/�������
enum {
	FIX_NONE	= 0,
	FIX_CHECK	= 1,
	FIX_LEAKS	= 2,
	FIX_ERRORS	= 3,
	FIX_ALL		= 4,
};

//�ع����ղ���
enum {
	REVERT_NONE	= 0,
	REVERT_APPLY= 1,
};

//ɾ�����ղ���
enum {
	SN_NONE		= 0,
	SN_DELETE	= 1,
	SN_EXCLUDE	= 2,
};

//O_SNAPSHOT ���L1/L2������Ԫ����(active snapshot/inactive snapshot/all)
//O_REFCOUNT ������ü�����Ԫ����
//O_ALL      ���L1/L2������ + ���ü�����
enum {
	O_SNAPSHOT  = 0,
	O_REFCOUNT	= 1,
	O_ALL		= 2,
};

enum {
	S_IMAGE_GOOD		= 0,	//�������
	S_IMAGE_LEAK		= 1,	//���ü���й©
	S_IMAGE_COPIED		= 2,	//���ü�����COPIED��־��ƥ�����(û�����ü������������)
	S_IMAGE_METADATA	= 3,	//�������, ���Ǿ���ͷ��������corrupt���
	S_IMAGE_WRONG		= 4,	//���ü�������
	S_IMAGE_SN_HEADER	= 5,	//�������ͷ��
	S_IMAGE_CORRUPT		= 6,	//������
	S_IMAGE_FLAG		= 7,	//����ͷ��������corrupt���
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


//�ɸ�����Ҫ���ú�INDEX_CN�Ĵ�С
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

	//�Ӿ����ȡ��ʵ�����ü���[�������ü����޸�֮���refcout]
	uint64_t **refcount_array;
	uint64_t refcount_array_size;

	//���ڼ������ü���
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

	//����ͳ��
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

	//���ڼ��Ԥ����/��̬���侵���Ƿ�����.
	int prealloc;
	int not_prealloc;

	//���ڼ��ر����·�������
	cluster_reuse_info* cluster_reuse;
	int cluster_reuse_max;
	int cluster_reuse_cn;
	int cluster_reused;

	//ģ�徵��·��
	char backing_file[1024];
	char *source_file;
} qcow2_state;

#endif
