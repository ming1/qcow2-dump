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

#include "fs_magic.h"
#include "qcow2_dump.h"


#if defined ( __GNUC__ ) && ( __GNUC__ > 3 )
#define offsetof( type, field ) __builtin_offsetof ( type, field )
#else
#define offsetof( type, field ) ( ( size_t ) &( ( ( type * ) NULL )->field ) )
#endif

//彩色显示
#define	COLOR_NONE				"\033[0m"
#define	FONT_COLOR_RED			"\033[1;31m"
#define	BLINK_COLOR_RED			"\033[5;31m"
#define	FONT_COLOR_GREEN		"\033[1;32m"
#define	BLINK_COLOR_GREEN		"\033[5;32m"
#define	FONT_COLOR_YELLOW		"\033[1;33m"
#define	BLINK_COLOR_YELLOW		"\033[5;33m"
#define	FONT_COLOR_CYAN			"\033[1;36m"

#define QCOW2_DUMP_VERSION "qcow2-dump version 0.07, Copyright (c) 2016 YOUPLUS\n"


static uint64_t qcow2_get_refcount_ro0(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro1(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro2(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro3(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro4(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro5(const void *refcount_array, uint64_t index);
static uint64_t qcow2_get_refcount_ro6(const void *refcount_array, uint64_t index);

static void qcow2_set_refcount_ro0(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro1(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro2(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro3(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro4(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro5(void *refcount_array, uint64_t index, uint64_t value);
static void qcow2_set_refcount_ro6(void *refcount_array, uint64_t index, uint64_t value);

//支持refcount bits特性: 1 - 64 bit [即: 0 <= refcount_order && refcount_order <= 6]
static Qcow2GetRefcountFunc *const get_refcount_funcs[] = {
	&qcow2_get_refcount_ro0,
	&qcow2_get_refcount_ro1,
	&qcow2_get_refcount_ro2,
	&qcow2_get_refcount_ro3,
	&qcow2_get_refcount_ro4,
	&qcow2_get_refcount_ro5,
	&qcow2_get_refcount_ro6
};

//支持refcount bits特性: 1 - 64 bit [即: 0 <= refcount_order && refcount_order <= 6]
//qemu-img create -f qcow2 -o preallocation=metadata -o refcount_bits=4  test.qcow2 1G
//qemu-img create -f qcow2 -o preallocation=metadata -o refcount_bits=8  test.qcow2 1G
//qemu-img create -f qcow2 -o preallocation=metadata -o refcount_bits=16 test.qcow2 1G
//qemu-img create -f qcow2 -o preallocation=metadata -o refcount_bits=32 test.qcow2 1G
//qemu-img snapshot -c sn1 test.qcow2
//qemu-img snapshot -c sn2 test.qcow2
//qemu-img snapshot -c sn3 test.qcow2
//qcow2-dump -m dump -o refcount test.qcow2 > /tmp/refcount.log 验证引用计数
static Qcow2SetRefcountFunc *const set_refcount_funcs[] = {
	&qcow2_set_refcount_ro0,
	&qcow2_set_refcount_ro1,
	&qcow2_set_refcount_ro2,
	&qcow2_set_refcount_ro3,
	&qcow2_set_refcount_ro4,
	&qcow2_set_refcount_ro5,
	&qcow2_set_refcount_ro6
};

static uint64_t qcow2_refcount_array_byte_size(qcow2_state* q2s, uint64_t entries);
static void qcow2_calculate_refcounts(qcow2_state* q2s, uint64_t offset, uint64_t size);
static void qcow2_image_status(qcow2_state* q2s, int status);

static inline void qcow2_cluster_reuse_sort(qcow2_state* q2s);
static void qcow2_cluster_reuse_iteration(qcow2_state* q2s, int l1_index,
					int l2_index, uint64_t base_offset, uint64_t offset);

static uint64_t qcow2_alloc_cluster_imrt(qcow2_state* q2s);


//命令行选项及使用说明
static void usage(void)
{
	printf(FONT_COLOR_CYAN);
	printf("Usage: qcow2-dump [-l] [-f] [-d none|color] [-m check|error|dump] [-o refcount|snapshot|all] [-s active|inactive|all|id/name] filename\n\n");
	printf(COLOR_NONE);

	printf("-h | --help\n");
	printf("-v | --version\n");
	printf("-H | --header\n");
	printf("-l | --lock\n");		//对镜像加锁互斥
	printf("-f | --flags\n");
	printf("-b | --base\n");		//不检查镜像的模板是否存在
	printf("-c | --compress\n");	//检查镜像的压缩
	printf("-r | --reuse\n");		//重建重用簇(rebuild reuse active cluster)
//	printf("-p | --prealloc\n");	//隐藏功能, 检查qcow2镜像是不是预分配/动态分配模式.
	printf("-M | --mark     corrupt|dirty\n");
	printf("-C | --clean    corrupt|dirty\n");
	printf("-V | --value    dec/hex value\n");					//修改已损坏的索引表项值, 支持十/十六进制
	printf("-O | --offset   dec/hex offset\n");					//指定要修改的偏移, 支持十/十六进制
	printf("-S | --src      dec/hex offset\n");					//指定源的偏移, 支持十/十六进制
	printf("-F | --file     source of -m copy\n");				//指定源文件
	printf("-w | --width    1/2/4/8/... byte(s)\n");			//指定字节宽度
	printf("-A | --apply    snapshot[N]|snapshot id/name\n");	//镜像修复功能: 快照回滚
	printf("-D | --delete   snapshot[N]|snapshot id/name\n");	//镜像修复功能: 删除指定快照
	printf("-E | --exclude  snapshot[N]|snapshot id/name\n");	//镜像修复功能: 删除指定id/name之外的所有快照
	printf("-d | --display  none|color "						//是否彩色输出
			FONT_COLOR_GREEN "[default: color]\n" COLOR_NONE);
	printf("-o | --output   refcount|snapshot|all "				//输出数据类型
			FONT_COLOR_GREEN "[default: all]\n" COLOR_NONE);
	printf("-R | --repair   none|check|leak|error|all "			//修复引用计数泄漏/错误
			FONT_COLOR_GREEN "[default: check]\n" COLOR_NONE);
	printf("-s | --snapshot active|inactive|all|id/name "
			FONT_COLOR_GREEN "[default: all]\n" COLOR_NONE);
	printf("-m | --mode     info|check|error|dump|edit|copy "
			FONT_COLOR_GREEN "[default: check]\n\n" COLOR_NONE);//输出模式

	printf(FONT_COLOR_CYAN);
	printf("[SAFE OPERATION]:\n");
	printf(COLOR_NONE);

	printf(FONT_COLOR_GREEN);
	printf("eg: qcow2-dump [-m check] filename\n");
	printf("eg: qcow2-dump [-m check] -c filename\n");
	printf("eg: qcow2-dump [-m check] -l filename\n\n");

	printf("eg: qcow2-dump -m error filename\n");
	printf("eg: qcow2-dump -m error -c filename\n");
	printf("eg: qcow2-dump -m error -l filename\n\n");

	printf("eg: qcow2-dump -m dump filename > /var/dump.log\n");
	printf("eg: qcow2-dump -m dump -c filename > /var/dump.log\n");
	printf("eg: qcow2-dump -m dump -l filename > /var/dump.log\n\n");
	printf(COLOR_NONE);

	printf(FONT_COLOR_CYAN);
	printf("[DANGEROUS OPERATION]:\n");
	printf(COLOR_NONE);

	printf(FONT_COLOR_RED);
	printf("eg: qcow2-dump -A snapshot id/name filename\n");	//回滚到指定快照
	printf("eg: qcow2-dump -A snapshot[N] filename\n");			//回滚到以十六进制数为序号找到的快照, 从0x0开始
	printf("eg: qcow2-dump -D snapshot id/name filename\n");	//删除指定快照
	printf("eg: qcow2-dump -D snapshot[N] filename\n");			//删除以十六进制数为序号找到的快照, 从0x0开始
	printf("eg: qcow2-dump -D all filename\n");					//删除所有快照, 并删除快照头
	printf("eg: qcow2-dump -E snapshot id/name filename\n");	//删除指定id/name之外的所有快照
	printf("eg: qcow2-dump -E snapshot[N] filename\n");			//删除以十六进制数为序号找到之外的快照, 从0x0开始
	printf("eg: qcow2-dump -E 0 filename\n\n");					//删除所有快照, 并删除快照头

	printf("eg: qcow2-dump -R leak|error|all filename\n");		//修复引用计数泄漏/错误
	printf("eg: qcow2-dump -R leak|error|all -r filename\n\n");	//重建重用簇(rebuild reuse active cluster)

	printf("eg: qcow2-dump -m edit -O offset -V value [-w 8] filename\n");		//修改offset偏移处的值为value(8字节)
	printf("eg: qcow2-dump -m edit -O offset -V value -w 1/2/4/8 filename\n\n");//修改offset偏移处的值为value

	printf("eg: qcow2-dump -m copy -O offset -S offset -w length filename\n");
	printf("eg: qcow2-dump -m copy -O offset -S offset -w length [-F source] filename\n\n");
	printf(COLOR_NONE);

	printf(FONT_COLOR_YELLOW);
	printf("eg: qcow2-dump -C corrupt filename\n");
	printf("eg: qcow2-dump -M corrupt filename\n\n");
	printf(COLOR_NONE);

	printf("eg: qcow2-dump [-m check] filename > /dev/null; echo $?\n");
	printf("eg: qcow2-dump [-m check] -d none filename > /var/check.log\n");
	printf("eg: qcow2-dump -m error -d none filename > /var/error.log\n");
	printf("eg: qcow2-dump -m error -d none -R none filename > /var/error.log\n");
	printf("eg: qcow2-dump -m error -d none -o refcount filename > /var/refcount.log\n");
	printf("eg: qcow2-dump -m error -d none -o snapshot filename > /var/snapshot.log\n");
	printf("eg: qcow2-dump -m error -d none -s active filename > /var/active.log\n");
	printf("eg: qcow2-dump -m error -d none -s inactive filename > /var/inactive.log\n");
	printf("eg: qcow2-dump -m dump -o refcount filename > /var/refcount.log\n");
	printf("eg: qcow2-dump -m dump -o snapshot filename > /var/snapshot.log\n");
	printf("eg: qcow2-dump -m dump -o snapshot -s active filename > /var/active.log\n");
	printf("eg: qcow2-dump -m dump -o snapshot -s 1 filename > /var/snapshot1.log\n");
	printf("eg: qcow2-dump -m dump -o snapshot -s inactive filename > /var/inactive.log\n");
	printf("eg: qcow2-dump -m dump -f -o snapshot -s active filename > /var/active.log\n\n");
}

//版本及版权
static void version(void)
{
	printf(QCOW2_DUMP_VERSION);
}

static void qcow2_header_offsetof(void);

//解析命令行参数, 设置默认参数
static void parse_args(qcow2_state* q2s, int argc, char **argv)
{
	int ch;
	int error = 0;
	int opt_ind = 0;
	int64_t value;

	const char *short_options = "hvHlfbcrpM:C:d:m:o:s:R:A:D:E:O:V:S:w:F:";
	static const struct option long_options[] = {
		{"help",		no_argument, NULL, 'h'},
		{"version",		no_argument, NULL, 'v'},
		{"header",		no_argument, NULL, 'H'},
		{"lock",		no_argument, NULL, 'l'},
		{"flags",		no_argument, NULL, 'f'},
		{"base",		no_argument, NULL, 'b'},
		{"compress",	no_argument, NULL, 'c'},
		{"reuse",		no_argument, NULL, 'r'},
		{"prealloc",	no_argument, NULL, 'p'},
		{"mark",		required_argument, NULL, 'M'},
		{"clean",		required_argument, NULL, 'C'},
		{"display",		required_argument, NULL, 'd'},
		{"mode",		required_argument, NULL, 'm'},
		{"output",		required_argument, NULL, 'o'},
		//引用计数修复功能：只能修复L1/L2 table表项不存在unaligned和invalid的情况(支持重建refcount table表)
		{"repair",		required_argument, NULL, 'R'},
		{"snapshot",	required_argument, NULL, 's'},
		//用于active snapshot的L1/L2表损坏, inactive snapshotL1/L2表完好时, 快照回滚. 与qemu-img的实现不同.
		{"apply",		required_argument, NULL, 'A'},
		{"delete",		required_argument, NULL, 'D'},
		{"exclude",		required_argument, NULL, 'E'},
		{"offset",		required_argument, NULL, 'O'},
		{"value",		required_argument, NULL, 'V'},
		{"src",			required_argument, NULL, 'S'},
		{"width",		required_argument, NULL, 'w'},
		{"file",		required_argument, NULL, 'F'},
		{ NULL, 0, NULL, 0 }
	};

	//默认值
	q2s->fd     = -1;
	q2s->lock   = FLOCK_OFF;
	q2s->flags  = FLAG_OFF;
	q2s->reuse  = REUSE_OFF;
	q2s->access_base = ACCESS_ON;
	q2s->check_compress = COMPRESS_OFF;
	q2s->prealloc = PREALLOC_NONE;
	q2s->mark   = MARK_NONE;
	q2s->clean  = CLEAN_NONE;
	q2s->display= COLOR_ON;
	q2s->mode   = M_CHECK;
	q2s->repair = FIX_CHECK;
	q2s->apply  = REVERT_NONE;
	q2s->delete = SN_NONE;
	q2s->width  = 8;
	q2s->write  = INVALID;
	q2s->dst_offset = INVALID;
	q2s->src_offset = INVALID;
	q2s->output = O_ALL;
	q2s->snapshot = "all";

	while ((ch = getopt_long(argc, argv, short_options, long_options, &opt_ind)) != -1) {
		switch (ch) {
		case 'h':
			usage();
			exit(0);
		case 'v':
			version();
			exit(0);
		case 'H':
			qcow2_header_offsetof();
			exit(0);
		case 'l':
			q2s->lock = FLOCK_ON;
			break;
		case 'f':
			q2s->flags = FLAG_ON;
			break;
		case 'b':
			q2s->access_base = ACCESS_OFF;
			break;
		case 'c':
			q2s->check_compress = COMPRESS_ON;
			break;
		case 'r':
			q2s->reuse = REUSE_ON;
			break;
		case 'p':
			q2s->prealloc = PREALLOC_CHECK;
			break;
		case 'M':
			if (!strcmp(optarg, "none")) {
				q2s->mark = MARK_NONE;
			} else if (!strcmp(optarg, "dirty")) {
				q2s->mark = MARK_DIRTY;
			} else if (!strcmp(optarg, "corrupt")) {
				q2s->mark = MARK_CORRUPT;
			} else {
				error = 1;
			}
			break;
		case 'C':
			if (!strcmp(optarg, "none")) {
				q2s->clean = CLEAN_NONE;
			} else if (!strcmp(optarg, "dirty")) {
				q2s->clean = CLEAN_DIRTY;
			} else if (!strcmp(optarg, "corrupt")) {
				q2s->clean = CLEAN_CORRUPT;
			} else {
				error = 1;
			}
			break;
		case 'd':
			if (!strcmp(optarg, "none")) {
				q2s->display = COLOR_OFF;
			} else if (!strcmp(optarg, "color")) {
				q2s->display = COLOR_ON;
			} else {
				error = 1;
			}
			break;
		case 'm':
			if (!strcmp(optarg, "info")) {
				q2s->mode = M_INFO;
			} else if (!strcmp(optarg, "check")) {
				q2s->mode = M_CHECK;
			} else if (!strcmp(optarg, "error")) {
				q2s->mode = M_ERROR;
			} else if (!strcmp(optarg, "dump")) {
				q2s->mode = M_DUMP;
			} else if (!strcmp(optarg, "edit")) {
				q2s->mode = M_EDIT;
				q2s->lock = FLOCK_ON;
			} else if (!strcmp(optarg, "copy")) {
				q2s->mode = M_COPY;
				q2s->lock = FLOCK_ON;
			} else {
				error = 1;
			}
			break;
		case 'F':
			q2s->source_file = optarg;
			break;
		case 'o':
			if (!strcmp(optarg, "snapshot")) {
				q2s->output = O_SNAPSHOT;
			} else if (!strcmp(optarg, "refcount")) {
				q2s->output = O_REFCOUNT;
			} else if (!strcmp(optarg, "all")) {
				q2s->output = O_ALL;
			} else {
				error = 1;
			}
			break;
		case 's':
			q2s->snapshot = optarg;
			break;
		case 'R':
			if (!strcmp(optarg, "none")) {
				q2s->repair = FIX_NONE;
			} else if (!strcmp(optarg, "check")) {
				q2s->repair = FIX_CHECK;
			} else if (!strcmp(optarg, "leak")) {
				q2s->repair = FIX_LEAKS;
				q2s->lock = FLOCK_ON;
			} else if (!strcmp(optarg, "error")) {
				q2s->repair = FIX_ERRORS;
				q2s->lock = FLOCK_ON;
			} else if (!strcmp(optarg, "all")) {
				q2s->repair = FIX_ALL;
				q2s->lock = FLOCK_ON;
			} else {
				error = 1;
			}
			break;
		case 'A':
			q2s->apply = REVERT_APPLY;
			q2s->snapshot = optarg;
			q2s->lock = FLOCK_ON;
			break;
		case 'D':
			q2s->delete = SN_DELETE;
			q2s->snapshot = optarg;
			q2s->lock = FLOCK_ON;
			break;
		case 'E':
			q2s->delete = SN_EXCLUDE;
			q2s->snapshot = optarg;
			q2s->lock = FLOCK_ON;
			break;
		case 'O':
			if (optarg[0] == '0' && tolower(optarg[1]) == 'x') {
				sscanf(optarg, "%lx", &value);
			} else {
				sscanf(optarg, "%ld", &value);
			}
			q2s->dst_offset = value;
			break;
		case 'V':
			if (optarg[0] == '0' && tolower(optarg[1]) == 'x') {
				sscanf(optarg, "%lx", &value);
			} else {
				sscanf(optarg, "%ld", &value);
			}
			q2s->write = value;
			break;
		case 'S':
			if (optarg[0] == '0' && tolower(optarg[1]) == 'x') {
				sscanf(optarg, "%lx", &value);
			} else {
				sscanf(optarg, "%ld", &value);
			}
			q2s->src_offset = value;
			break;
			break;
		case 'w':
			if (optarg[0] == '0' && tolower(optarg[1]) == 'x') {
				sscanf(optarg, "%lx", &value);
			} else {
				sscanf(optarg, "%ld", &value);
			}
			q2s->width = value;
			break;
		case '?':
			printf("Try `%s --help' for more information.\n", argv[0]);
			exit(-EINVAL);	/* Invalid argument */
			break;
		}
	}
	if ((argc - optind) != 1) {
		error = 1;
	}
	if (error) {
		printf("Invalid number of argument.\n"
			   "Try `%s --help' for more information.\n", argv[0]);
		exit(-EINVAL);		/* Invalid argument */
	}

	if (q2s->mode != M_CHECK && q2s->repair > FIX_CHECK) {
		q2s->repair = FIX_CHECK;
	}

	q2s->filename = argv[optind];
}

//以彩色输出
static inline int is_color_display(qcow2_state* q2s)
{
	return q2s->mode != M_DUMP && q2s->display == COLOR_ON;
}

static inline int64_t align_offset(int64_t offset, int64_t n)
{
	offset = (offset + n - 1) & ~(n - 1);
	return offset;
}

//判断文件偏移是否簇对齐
static inline int64_t offset_into_cluster(int cluster_size, int64_t offset)
{
	return offset & (cluster_size - 1);
}

static inline int64_t qcow2_start_of_cluster(qcow2_state* q2s, int64_t offset)
{
	return offset & ~(q2s->cluster_size - 1);
}

static inline int qcow2_size_to_clusters(qcow2_state* q2s, int64_t size)
{
	QCowHeader *header = &q2s->header;

	return (size + (q2s->cluster_size - 1)) >> header->cluster_bits;
}

static inline int64_t qcow2_size_to_l1(qcow2_state* q2s, int64_t size)
{
	int shift = q2s->cluster_bits + q2s->l2_bits;
	return (size + (1ULL << shift) - 1) >> shift;
}

static inline int qcow2_offset_to_l2_index(qcow2_state* q2s, int64_t offset)
{
	return (offset >> q2s->cluster_bits) & (q2s->l2_size - 1);
}

//内存快照一级索引下标
static inline int64_t qcow2_vm_state_offset(qcow2_state* q2s)
{
	return (int64_t)q2s->l1_vm_state_index << (q2s->cluster_bits + q2s->l2_bits);
}

static inline uint64_t qcow2_max_refcount_clusters(qcow2_state* q2s)
{
	return QCOW_MAX_REFTABLE_SIZE >> q2s->cluster_bits;
}

//超出正常qcow2镜像文件大小范围, 标准: 1.2倍于qcow2镜像文件的结尾offset
//因为检查时, qcow2镜像可能在增长(没有使用-l参数, 检查时没有对镜像加锁)
static inline int64_t qcow2_out_of_range(qcow2_state* q2s, int64_t offset)
{
	return (offset > 6 * q2s->seek_end / 5);
}

//元数据偏移有效性检查
static int qcow2_validate_table_offset(qcow2_state* q2s, uint64_t offset,
									uint64_t entries, size_t entry_len)
{
	uint64_t size;

	/* Use signed INT64_MAX as the maximum even for uint64_t header fields,
	 * because values will be passed to qemu functions taking int64_t. */
	if (entries > INT64_MAX / entry_len) {
		return -EINVAL;
	}

	size = entries * entry_len;

	if (INT64_MAX - size < offset) {
		return -EINVAL;
	}

	/* Tables must be cluster aligned */
	if (offset_into_cluster(q2s->cluster_size, offset)) {
		return -EINVAL;
	}

	if (qcow2_out_of_range(q2s, offset + size)) {
		printf("Line: %d | offset: 0x%lx out of range, seek_end: 0x%lx\n",
				__LINE__, offset, q2s->seek_end);
		return -EFBIG;
	}

	return 0;
}

//判断qcow2镜像是否已损坏
static inline int qcow2_is_corrupt(qcow2_state* q2s)
{
	return  q2s->ref_unaligned || q2s->ref_invalid
			|| q2s->l1_unaligned || q2s->l1_invalid
			|| q2s->l2_unaligned || q2s->l2_invalid;
}

//判断qcow2镜像索引表是否已损坏
static inline int qcow2_L1L2_is_corrupt(qcow2_state* q2s)
{
	return  q2s->l1_unaligned || q2s->l1_invalid
			|| q2s->l2_unaligned || q2s->l2_invalid;
}

//判断qcow2镜像快照头是否已损坏
static inline int qcow2_is_sn_header_corrupt(qcow2_state* q2s)
{
	return q2s->image_status == S_IMAGE_SN_HEADER;
}

//qcow2镜像引用计数表是否损坏
static inline int qcow2_is_refcount_corrupt(qcow2_state* q2s)
{
	return q2s->ref_unaligned || q2s->ref_invalid;
}

static inline int qcow2_rebuild_reused_active_cluster(qcow2_state* q2s)
{
	return q2s->reuse;
}

//qcow2镜像索引表是否损坏
static inline int qcow2_is_snapshot_corrupt(int l1_unaligned, int l1_invalid,
											int l2_unaligned, int l2_invalid)
{
	return l1_unaligned || l1_invalid || l2_unaligned || l2_invalid;
}

//qcow2镜像是否有完好的快照
static inline int qcow2_has_good_snapshot(qcow2_state* q2s)
{
	return q2s->snapshot_id;
}

//判断引用计数是否有错误
static inline int qcow2_maybe_wrong(qcow2_state* q2s)
{
	return q2s->corruptions;
}

//判断引用计数是否与COPIED标志匹配
static inline int qcow2_maybe_copied(qcow2_state* q2s)
{
	return q2s->l1_copied || q2s->l2_copied;
}

//判断引用计数是否有泄漏
static inline int qcow2_maybe_leak(qcow2_state* q2s)
{
	return q2s->leaks;
}

//判断是否检测qcow2镜像的所有元数据
static inline int qcow2_is_check_all(qcow2_state* q2s)
{
	QCowHeader* header = &q2s->header;

	int all_snapshot = !strcmp(q2s->snapshot, "all")
			|| (!strcmp(q2s->snapshot, "active") && !header->nb_snapshots);

	return q2s->mode != M_INFO && all_snapshot;
}

//info模式, 查看镜像头等基本信息
static inline int qcow2_is_info_mode(qcow2_state* q2s)
{
	return q2s->mode == M_INFO;
}

//是否设置qcow2头部DIRTY/CORRUPT标志
static inline int qcow2_is_mark_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->mark > MARK_NONE;
}

//是否清除qcow2头部DIRTY/CORRUPT标志
static inline int qcow2_is_clean_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->clean > CLEAN_NONE;
}

//判断是否为快照回滚模式
static inline int qcow2_is_revert_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->apply == REVERT_APPLY;
}

//判断是否为删除指定快照模式
static inline int qcow2_is_delete_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->delete == SN_DELETE;
}

//判断是否为删除指定之外快照模式
static inline int qcow2_is_exclude_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->delete == SN_EXCLUDE;
}

//判断是否为编辑模式
static inline int qcow2_is_edit_mode(qcow2_state* q2s)
{
	return q2s->mode == M_EDIT;
}

//判断是否为复制模式
static inline int qcow2_is_copy_mode(qcow2_state* q2s)
{
	return q2s->mode == M_COPY;
}

//判断是否为引用计数错误/泄漏修复模式
static inline int qcow2_is_repair_mode(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && q2s->repair >= FIX_LEAKS;
}

//是否修复引用计数泄漏
static inline int qcow2_is_repair_leaks(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && (q2s->repair == FIX_LEAKS || q2s->repair == FIX_ALL);
}

//是否修复引用计数错误
static inline int qcow2_is_repair_errors(qcow2_state* q2s)
{
	return q2s->mode == M_CHECK && (q2s->repair == FIX_ERRORS || q2s->repair == FIX_ALL);
}

//判断是否为修复COPIED标志的模式
static inline int qcow2_is_repair_copied(qcow2_state* q2s)
{
	return !qcow2_L1L2_is_corrupt(q2s) && q2s->mode == M_CHECK && q2s->repair >= FIX_ERRORS;
}

//镜像是否需要重建refcount structure
static inline int qcow2_need_rebuild_refcount(qcow2_state* q2s)
{
	return q2s->rebuild;
}

//是否加锁互斥
static inline int qcow2_open_mutex(qcow2_state* q2s)
{
	return q2s->lock == FLOCK_ON;
}

//是否检查模板的存在
static inline int qcow2_check_base_exist(qcow2_state* q2s)
{
	return q2s->access_base == ACCESS_ON;
}

static inline int qcow2_strict_mode(qcow2_state* q2s)
{
	return (!qcow2_is_mark_mode(q2s) && !qcow2_is_clean_mode(q2s)
			&& !qcow2_is_edit_mode(q2s) && !qcow2_is_copy_mode(q2s)
			&& !qcow2_is_info_mode(q2s));
}

//打开qcow2镜像并且读取qcow2镜像头部信息
static int qcow2_open(qcow2_state* q2s)
{
	int fd = -1;
	int i, n, ret;
	QCowHeader header;
	struct stat statbuf;
	struct statfs fs = {0};

	if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色		//输出绿色
		printf(FONT_COLOR_GREEN "\nFile:" FONT_COLOR_CYAN " %s\n" COLOR_NONE, q2s->filename);
	} else {
		printf("\nFile: %s\n", q2s->filename);
	}
	printf("----------------------------------------------------------------\n\n");

	//samba不支持O_DIRECT, 另外O_DIRECT需要内存对齐.
	if (qcow2_is_mark_mode(q2s) || qcow2_is_clean_mode(q2s)
		|| qcow2_is_repair_mode(q2s) || qcow2_is_revert_mode(q2s)
		|| qcow2_is_delete_mode(q2s) || qcow2_is_exclude_mode(q2s)
		|| qcow2_is_edit_mode(q2s) || qcow2_is_copy_mode(q2s)) {
		fd = open(q2s->filename, O_RDWR);
	} else {
		fd = open(q2s->filename, O_RDONLY);
	}
	if (fd < 0) {
		ret = -errno;
		printf("Line: %d | open failed: %d\n", __LINE__, ret);
		return ret;
	}
	q2s->fd = fd;

	if (qcow2_open_mutex(q2s)) {
		ret = flock(fd, LOCK_EX | LOCK_NB);
		if (ret == -1 && errno != ENOLCK) {
			ret = -errno;
			printf("Line: %d | flock LOCK_EX failed: %d\n", __LINE__, ret);
			return ret;
		}
	}

	memset(&header, 0, sizeof(header));
	ret = pread(fd, &header, sizeof(header), 0);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | pread failed: %d\n", __LINE__, ret);
		return ret;
	}

	be32_to_cpus(&header.magic);
	be32_to_cpus(&header.version);
	be64_to_cpus(&header.backing_file_offset);
	be32_to_cpus(&header.backing_file_size);
	be64_to_cpus(&header.size);
	be32_to_cpus(&header.cluster_bits);
	be32_to_cpus(&header.crypt_method);
	be64_to_cpus(&header.l1_table_offset);
	be32_to_cpus(&header.l1_size);
	be64_to_cpus(&header.refcount_table_offset);
	be32_to_cpus(&header.refcount_table_clusters);
	be64_to_cpus(&header.snapshots_offset);
	be32_to_cpus(&header.nb_snapshots);

	if (header.magic != QCOW_MAGIC) {
		printf("Line: %d | Image is not in qcow2 format, magic:0x%x\n",
				__LINE__, header.magic);
		return -EINVAL;
	}
	if (header.version < 2 || header.version > 3) {
		printf("Line: %d | QCOW2 version: %d\n", __LINE__, header.version);
		return -ENOTSUP;
	}

	/* Initialise version 3 header fields */
	if (header.version == 2) {
		header.incompatible_features	= 0;
		header.compatible_features		= 0;
		header.autoclear_features		= 0;
		header.refcount_order			= 4;
		header.header_length			= 72;
	} else {
		be64_to_cpus(&header.incompatible_features);
		be64_to_cpus(&header.compatible_features);
		be64_to_cpus(&header.autoclear_features);
		be32_to_cpus(&header.refcount_order);
		be32_to_cpus(&header.header_length);

		if (header.header_length < 104) {
			printf("Line: %d | qcow2 header too short\n", __LINE__);
			return -EINVAL;
		}
	}

	q2s->header = header;

	q2s->cluster_bits = header.cluster_bits;
	q2s->cluster_size = 1 << q2s->cluster_bits;
	q2s->cluster_sectors = 1 << (q2s->cluster_bits - 9);
	q2s->l2_bits = header.cluster_bits - 3; /* L2 is always one cluster */
	q2s->l2_size = 1 << q2s->l2_bits;

	/* 2^(q2s->refcount_order - 3) is the refcount width in bytes */
	/* 0 <= refcount_order <=6 */
	if (header.refcount_order < 0 || header.refcount_order > 6) {
		ret = -EINVAL;
		printf("Line: %d | Invalid refcount_order: %u\n", __LINE__, header.cluster_bits);
		return ret;
	}

	q2s->refcount_bits = 1 << header.refcount_order;
	q2s->refcount_max = UINT64_C(1) << (q2s->refcount_bits - 1);
	q2s->refcount_max += q2s->refcount_max - 1;
	q2s->refcount_block_bits = header.cluster_bits - (header.refcount_order - 3);
	q2s->refcount_block_size = 1 << q2s->refcount_block_bits;
	q2s->refcount_table_size = header.refcount_table_clusters << (q2s->cluster_bits - 3);
	q2s->get_refcount = get_refcount_funcs[header.refcount_order];
	q2s->set_refcount = set_refcount_funcs[header.refcount_order];

	q2s->csize_shift = (62 - (q2s->cluster_bits - 8));
	q2s->csize_mask = (1 << (q2s->cluster_bits - 8)) - 1;
	q2s->cluster_offset_mask = (1LL << q2s->csize_shift) - 1;

	/* Initialise cluster size */
	if (header.cluster_bits < MIN_CLUSTER_BITS
		|| header.cluster_bits > MAX_CLUSTER_BITS) {
		ret = -EINVAL;
		printf("Line: %d | Unsupported cluster size: 2^%u\n", __LINE__, header.cluster_bits);
		return ret;
	}

	if (header.header_length > q2s->cluster_size) {
		ret = -EINVAL;
		printf("Line: %d | QCOW2 header: %u exceeds cluster size\n",
				__LINE__, header.header_length);
		return ret;
	}

	if (header.refcount_table_clusters > qcow2_max_refcount_clusters(q2s)) {
		ret = -EINVAL;
		printf("Line: %d | Reference table is too large: %u\n",
				__LINE__, header.refcount_table_clusters);
		return ret;
	}

	/* Snapshot table offset/length */
	if (header.nb_snapshots > QCOW_MAX_SNAPSHOTS) {
		ret = -EINVAL;
		printf("Line: %d | Too many snapshots: %u\n",
				__LINE__, header.nb_snapshots);
		return ret;
	}

	/* the level 1 table */
	if (header.l1_size > QCOW_MAX_L1_SIZE / sizeof(uint64_t)) {
		ret = -EFBIG;
		printf("Line: %d | Active L1 table: %u too large\n",
				__LINE__, header.l1_size);
		return ret;
	}

	/* the level 1 table of vm state */
	q2s->l1_vm_state_index = qcow2_size_to_l1(q2s, header.size);
	if (q2s->l1_vm_state_index > INT_MAX) {
		ret = -EFBIG;
		printf("Line: %d | QCOW2 Image: %u is too big\n",
				__LINE__, q2s->l1_vm_state_index);
		return ret;
	}

	/* the L1 table must contain at least enough entries to put
	   header.size bytes */
	if (header.l1_size < q2s->l1_vm_state_index) {
		ret = -EINVAL;
		printf("Line: %d | L1 table: %u is too small, l1_vm_state: %u\n",
				__LINE__, header.l1_size, q2s->l1_vm_state_index);
		return ret;
	}

	q2s->seek_end = lseek(q2s->fd, 0, SEEK_END);
	if (q2s->seek_end < 0) {
		ret = -errno;
		printf("Line: %d | lseek failed: %d\n", __LINE__, ret);
		return ret;
	}

	if (fstat(q2s->fd, &statbuf) < 0) {
		ret = -errno;
		q2s->disk_size = 0;
		printf("Line: %d | fstat error: %d\n", __LINE__, ret);
		return ret;
	} else {
		q2s->disk_size = (int64_t)statbuf.st_blocks * 512;
	}

	if (fstatfs(fd, &fs) < 0) {
		ret = -errno;
		q2s->fs_type = 0;
		printf("Line: %d | fstatfs failed: %d\n", __LINE__, ret);
	} else {
		q2s->fs_type = fs.f_type;
	}

	if (qcow2_strict_mode(q2s)) {
		ret = qcow2_validate_table_offset(q2s, header.refcount_table_offset,
									q2s->refcount_table_size, sizeof(uint64_t));
		if (ret < 0) {
			printf("Line: %d | Invalid reference count table offset: 0x%lx\n",
					__LINE__, header.refcount_table_offset);
			return ret;
		}

		ret = qcow2_validate_table_offset(q2s, header.snapshots_offset,
									header.nb_snapshots,
									sizeof(QCowSnapshotHeader));
		if (ret < 0) {
			printf("Line: %d | Invalid snapshot header offset: 0x%lx\n",
					__LINE__, header.snapshots_offset);
			return ret;
		}

		ret = qcow2_validate_table_offset(q2s, header.l1_table_offset,
									header.l1_size, sizeof(uint64_t));
		if (ret < 0) {
			printf("Line: %d | Invalid L1 table offset: 0x%lx\n",
					__LINE__, header.l1_table_offset);
			return ret;
		}

		if (header.backing_file_offset > q2s->cluster_size) {
			ret = -EINVAL;
			printf("Line: %d | Invalid backing file offset: 0x%lx\n",
					__LINE__, header.backing_file_offset);
			return ret;
		}
	}

	if (header.backing_file_offset && header.backing_file_size) {
		uint32_t size = header.backing_file_size;
		if (size >= sizeof(q2s->backing_file)
			|| size > MIN(1023, q2s->cluster_size - header.backing_file_offset)) {
			ret = -EINVAL;
			printf("Line: %d | Backing file name too long\n", __LINE__);
			return ret;
		}
		ret = pread(fd, q2s->backing_file, size, header.backing_file_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pread backing_file failed: %d\n", __LINE__, ret);
			return ret;
		}
		if (qcow2_check_base_exist(q2s) && !strstr(q2s->backing_file, "://")) {
			if (access(q2s->backing_file, F_OK) != 0) {//非路径访问的模板不检查是否存在
				ret = -ENOENT;
				printf("Line: %d | backing_file: %s not exist: %d\n",
						__LINE__, q2s->backing_file, ret);
				return ret;
			}
		}
	}

	//只有检查所有元数据时, 才检查引用计数
	if (qcow2_strict_mode(q2s) && qcow2_is_check_all(q2s)) {
		char* reuse_max_env;
		uint64_t refcount_table_size;
		int64_t size = MAX(header.size, q2s->seek_end);

		//用于计算引用计数
		size = qcow2_size_to_clusters(q2s, size) + SECTOR_ALIGN;	//!预留一部分空间
		q2s->nb_clusters = align_offset(size, SECTOR_ALIGN);
		refcount_table_size = qcow2_refcount_array_byte_size(q2s, q2s->nb_clusters);
		q2s->refcount_table = malloc(refcount_table_size);
		if (!q2s->refcount_table) {
			printf("Line: %d | malloc IMRT failed\n", __LINE__);
			return -ENOMEM;
		}
		//所有引用计数从0开始计数
		memset(q2s->refcount_table, 0, refcount_table_size);

		//对header簇引用计数
		qcow2_calculate_refcounts(q2s, 0, q2s->cluster_size);

		//从镜像读取的实际引用计数
		size = header.refcount_table_clusters << (header.cluster_bits - 3);
		q2s->refcount_array_size = size + 1;
		size = q2s->refcount_array_size * sizeof(uint64_t *);
		q2s->refcount_array = malloc(size);
		if (!q2s->refcount_array) {
			printf("Line: %d | malloc failed\n", __LINE__);
			return -ENOMEM;
		}
		memset(q2s->refcount_array, 0, size);

		//用于检查引用计数出现错误后, 簇被重新分配, 导致多个索引entry指向同一簇
		//通过环境变量设置最大检测数: export CLUSTER_REUSE_MAX=8388608
		reuse_max_env = getenv("CLUSTER_REUSE_MAX");
		if (reuse_max_env) {
			sscanf(reuse_max_env, "%d", &q2s->cluster_reuse_max);
		}
		if (reuse_max_env == NULL
			|| q2s->cluster_reuse_max == 0
			|| q2s->cluster_reuse_max < q2s->cluster_size) {
			q2s->cluster_reuse_max = q2s->cluster_size;
		}
		//支持最大检测8M个簇是否被重用
		if (q2s->cluster_reuse_max > CLUSTER_REUSE_MAX) {
			q2s->cluster_reuse_max = CLUSTER_REUSE_MAX;
		}
		q2s->cluster_reuse = malloc(sizeof(cluster_reuse_info) * q2s->cluster_reuse_max);
		if (!q2s->cluster_reuse) {
			printf("Line: %d | malloc failed\n", __LINE__);
			return -ENOMEM;
		}
		memset(q2s->cluster_reuse, 0, sizeof(cluster_reuse_info) * q2s->cluster_reuse_max);
		for (i = 0; i < q2s->cluster_reuse_max; i++) {
			for (n = 0; n < INDEX_CN; n++) {
				q2s->cluster_reuse[i].l1_l2[n].l1_index = INVALID;
				q2s->cluster_reuse[i].l1_l2[n].l2_index = INVALID;
			}
		}
		q2s->cluster_reuse_cn = 0;
		q2s->cluster_reused = 0;
	}

	return 0;
}

//匹配镜像所在的文件系统
//有新的文件系统需要匹配, 在fs_type[]数组中添加魔数和名称即可
static void qcow2_match_in_fs(qcow2_state* q2s)
{
	int i;
	int count = sizeof(fs_type) / sizeof(fs_type[0]);

	//遍历匹配镜像所在的文件系统
	for (i = 0; i < count; i++) {
		if (fs_type[i].magic == q2s->fs_type) {
			if (is_color_display(q2s)) {
				printf("fs_type: " FONT_COLOR_GREEN "%s\n" COLOR_NONE,
						fs_type[i].fs_name);
			} else {
				printf("fs_type: %s\n", fs_type[i].fs_name);
			}
			break;
		}
	}
	if (i == count) {
		if (q2s->fs_type) {
			if (is_color_display(q2s)) {
				printf("fs_type: " FONT_COLOR_YELLOW "0x%lx\n" COLOR_NONE,
						q2s->fs_type);
			} else {
				printf("fs_type: 0x%lx\n", q2s->fs_type);
			}
		} else {
			if (is_color_display(q2s)) {
				printf("fs_type: " FONT_COLOR_RED "unknown\n" COLOR_NONE);
			} else {
				printf("fs_type: unknown\n");
			}
		}
	}
}

//以二进制输出标志位
static void qcow2_print_binary_flags(qcow2_state* q2s, uint64_t num)
{
	int i;

	for (i = 7; i >= 0; i--) {//只输出最低位的8 bit
		if (num & (1 << i)) {
			if (is_color_display(q2s)) {
				printf(FONT_COLOR_YELLOW "1" COLOR_NONE);
			} else {
				printf("1");
			}
		} else {
			printf("0");
		}
	}
	printf("\n");
}

//输出qcow2镜像头部信息
static void qcow2_header(qcow2_state* q2s)
{
	QCowHeader* header = &q2s->header;

	if (header->incompatible_features & QCOW2_INCOMPAT_CORRUPT) {
		qcow2_image_status(q2s, S_IMAGE_FLAG);
	}

	printf("magic: 0x%x\n", header->magic);
	if (is_color_display(q2s)) {
		printf("version: " FONT_COLOR_CYAN "%d\n" COLOR_NONE, header->version);
	} else {
		printf("version: %d\n", header->version);
	}
	printf("backing_file_offset: 0x%lx\n", header->backing_file_offset);
	printf("backing_file_size: %u\n", header->backing_file_size);
	if (header->backing_file_offset && header->backing_file_size) {
		printf("backing_file: %s\n", q2s->backing_file);
	}

	qcow2_match_in_fs(q2s);

	printf("virtual_size: %ld / %ldM / %ldG\n", header->size,
			header->size/ONE_M, header->size/ONE_G);
	printf("disk_size: %ld / %ldM / %ldG\n", q2s->disk_size,
			q2s->disk_size/ONE_M, q2s->disk_size/ONE_G);
	if (is_color_display(q2s)) {
		printf("seek_end: %ld [" FONT_COLOR_GREEN "0x%lx" COLOR_NONE
				"] / %ldM / %ldG\n", q2s->seek_end, q2s->seek_end,
				q2s->seek_end/ONE_M, q2s->seek_end/ONE_G);
		printf("cluster_bits: " FONT_COLOR_CYAN "%d\n" COLOR_NONE,
				header->cluster_bits);		//default: 16
		printf("cluster_size: " FONT_COLOR_CYAN "%lu\n" COLOR_NONE,
				q2s->cluster_size);			//default: 64K
	} else {
		printf("seek_end: %ld [0x%lx] / %ldM / %ldG\n", q2s->seek_end,
				q2s->seek_end, q2s->seek_end/ONE_M, q2s->seek_end/ONE_G);
		printf("cluster_bits: %d\n", header->cluster_bits);		//default: 16
		printf("cluster_size: %lu\n", q2s->cluster_size);		//default: 64K
	}
	printf("crypt_method: %d\n", header->crypt_method);
	printf("csize_shift: %d\n", q2s->csize_shift);
	printf("csize_mask: %d\n", q2s->csize_mask);
	if (is_color_display(q2s)) {
		printf("cluster_offset_mask: " FONT_COLOR_CYAN "0x%lx\n" COLOR_NONE,
				q2s->cluster_offset_mask);
		printf("l1_table_offset: " FONT_COLOR_GREEN "0x%lx\n" COLOR_NONE,
				header->l1_table_offset);
		printf("l1_size: " FONT_COLOR_GREEN "%d\n" COLOR_NONE,
				header->l1_size);
		printf("l1_vm_state_index: " FONT_COLOR_GREEN "%d\n" COLOR_NONE,
				q2s->l1_vm_state_index);
		printf("l2_size: " FONT_COLOR_CYAN "%d\n" COLOR_NONE, q2s->l2_size);
		printf("refcount_order: " FONT_COLOR_CYAN "%d\n" COLOR_NONE,
				header->refcount_order);	//default: 4
		printf("refcount_bits: " FONT_COLOR_CYAN "%d\n" COLOR_NONE,
				q2s->refcount_bits);		//default: 16
		printf("refcount_block_bits: " FONT_COLOR_CYAN "%d\n" COLOR_NONE,
				q2s->refcount_block_bits);
		printf("refcount_block_size: " FONT_COLOR_CYAN "%d\n" COLOR_NONE,
				q2s->refcount_block_size);
		printf("refcount_table_offset: " FONT_COLOR_GREEN "0x%lx\n" COLOR_NONE,
				header->refcount_table_offset);
		printf("refcount_table_clusters: " FONT_COLOR_GREEN "%d\n" COLOR_NONE,
				header->refcount_table_clusters);
	} else {
		printf("cluster_offset_mask: 0x%lx\n", q2s->cluster_offset_mask);
		printf("l1_table_offset: 0x%lx\n", header->l1_table_offset);
		printf("l1_size: %d\n", header->l1_size);
		printf("l1_vm_state_index: %d\n", q2s->l1_vm_state_index);
		printf("l2_size: %d\n", q2s->l2_size);
		printf("refcount_order: %d\n", header->refcount_order);	//default: 4
		printf("refcount_bits: %d\n", q2s->refcount_bits);		//default: 16
		printf("refcount_block_bits: %d\n", q2s->refcount_block_bits);
		printf("refcount_block_size: %d\n", q2s->refcount_block_size);
		printf("refcount_table_offset: 0x%lx\n", header->refcount_table_offset);
		printf("refcount_table_clusters: %d\n", header->refcount_table_clusters);
	}
	if (is_color_display(q2s)) {
		printf("snapshots_offset: " FONT_COLOR_GREEN "0x%lx\n" COLOR_NONE,
				header->snapshots_offset);
		printf("nb_snapshots: " FONT_COLOR_GREEN "%u\n" COLOR_NONE,
				header->nb_snapshots);
	} else {
		printf("snapshots_offset: 0x%lx\n", header->snapshots_offset);
		printf("nb_snapshots: %u\n", header->nb_snapshots);
	}

	printf("incompatible_features: ");
	qcow2_print_binary_flags(q2s, header->incompatible_features);
	printf("compatible_features: ");
	qcow2_print_binary_flags(q2s, header->compatible_features);
	printf("autoclear_features: ");
	qcow2_print_binary_flags(q2s, header->autoclear_features);

	printf("\n================================================================\n\n");
}

//输出qcow2头部字段的偏移
static void qcow2_header_offsetof(void)
{
	QCowHeader header;

	printf(FONT_COLOR_GREEN);
	printf("\nQCOW2 HEADER:\n");
	printf(COLOR_NONE);
	printf("------------------------------------------------\n\n");

	printf("offsetof magic:                    "
			FONT_COLOR_CYAN "%zu   [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, magic), sizeof(header.magic));
	printf("offsetof version:                  "
			FONT_COLOR_CYAN "%zu   [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, version), sizeof(header.version));
	printf("offsetof backing_file_offset:      "
			FONT_COLOR_CYAN "%zu   [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, backing_file_offset),
			sizeof(header.backing_file_offset));
	printf("offsetof backing_file_size:        "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, backing_file_size),
			sizeof(header.backing_file_size));
	printf("offsetof cluster_bits:             "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, cluster_bits),
			sizeof(header.cluster_bits));
	printf("offsetof size:                     "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, size), sizeof(header.size));
	printf("offsetof crypt_method:             "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, crypt_method),
			sizeof(header.crypt_method));
	printf("offsetof l1_size:                  "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, l1_size), sizeof(header.l1_size));
	printf("offsetof l1_table_offset:          "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, l1_table_offset),
			sizeof(header.l1_table_offset));
	printf("offsetof refcount_table_offset:    "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, refcount_table_offset),
			sizeof(header.refcount_table_offset));
	printf("offsetof refcount_table_clusters:  "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, refcount_table_clusters),
			sizeof(header.refcount_table_clusters));
	printf("offsetof nb_snapshots:             "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, nb_snapshots),
			sizeof(header.nb_snapshots));
	printf("offsetof snapshots_offset:         "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, snapshots_offset),
			sizeof(header.snapshots_offset));

	/* The following fields are only valid for version >= 3 */
	printf("offsetof incompatible_features:    "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, incompatible_features),
			sizeof(header.incompatible_features));
	printf("offsetof compatible_features:      "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, compatible_features),
			sizeof(header.compatible_features));
	printf("offsetof autoclear_features:       "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, autoclear_features),
			sizeof(header.autoclear_features));

	printf("offsetof refcount_order:           "
			FONT_COLOR_CYAN "%zu  [%zu bytes]\n" COLOR_NONE,
			offsetof(QCowHeader, refcount_order),
			sizeof(header.refcount_order));
	printf("offsetof header_length:            "
			FONT_COLOR_CYAN "%zu [%zu bytes]\n\n" COLOR_NONE,
			offsetof(QCowHeader, header_length),
			sizeof(header.header_length));

	printf("================================================\n");
}

//设置qcow2头部的DIRTY/CORRUPT标志
static int qcow2_mark_flags(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	uint64_t features;
	int ret = 0;
	char* flag = NULL;

	switch(q2s->mark) {
	case MARK_DIRTY:
		features = QCOW2_INCOMPAT_DIRTY;
		flag = "DIRTY";
		break;
	case MARK_CORRUPT:
		features = QCOW2_INCOMPAT_CORRUPT;
		flag = "CORRUPT";
		break;
	}

	features = cpu_to_be64(header->incompatible_features | features);
	ret = pwrite(q2s->fd, &features, sizeof(features),
				offsetof(QCowHeader, incompatible_features));
	if (ret < 0) {
		ret = -errno;
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_RED "\nMark %s flag failed: %d\n" COLOR_NONE, flag, ret);
		} else {
			printf("\nMark %s flag failed: %d\n", flag, ret);
		}
		goto out;
	} else {
		ret = 0;
		fsync(q2s->fd);
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_GREEN "\nMark %s flag success\n" COLOR_NONE, flag);
		} else {
			printf("\nMark %s flag success\n", flag);
		}
	}

out:
	printf("\n================================================================\n");
	return ret;
}

//清除qcow2头部的DIRTY/CORRUPT标志
static int qcow2_clean_flags(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	uint64_t features;
	int ret = 0;
	char* flag = NULL;

	switch(q2s->clean) {
	case CLEAN_DIRTY:
		features = QCOW2_INCOMPAT_DIRTY;
		flag = "DIRTY";
		break;
	case CLEAN_CORRUPT:
		features = QCOW2_INCOMPAT_CORRUPT;
		flag = "CORRUPT";
		break;
	}

	features = cpu_to_be64(header->incompatible_features & ~features);
	ret = pwrite(q2s->fd, &features, sizeof(features),
				offsetof(QCowHeader, incompatible_features));
	if (ret < 0) {
		ret = -errno;
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_RED "\nClean %s flag failed: %d\n" COLOR_NONE, flag, ret);
		} else {
			printf("\nClean %s flag failed: %d\n", flag, ret);
		}
		goto out;
	} else {
		ret = 0;
		fsync(q2s->fd);
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_GREEN "\nClean %s flag success\n" COLOR_NONE, flag);
		} else {
			printf("\nClean %s flag success\n", flag);
		}
	}

out:
	printf("\n================================================================\n");
	return ret;
}

//只有在二级有输出时, 才输出上一级的信息
static void qcow2_entry_print(char* entry_buff, int* first)
{
	if (entry_buff && entry_buff[0] && first && *first) {
		*first = 0;
		printf("%s", entry_buff);
	}
}

//是否输出L1表表项或L2表表项的flag标记
static int qcow2_print_flags(qcow2_state* q2s)
{
	return q2s->flags == FLAG_ON;
}

//L1表表项或L2表表项的flag标记
static void qcow2_entry_flags(qcow2_state* q2s, char* entry_buff,
								int64_t offset, int enter)
{
	uint64_t compress = offset & QCOW_OFLAG_COMPRESSED;
	int64_t data_offset = offset;
	char flags[64] = {0};

	if (qcow2_print_flags(q2s) && (offset & QCOW_OFLAG)) {
		if (compress) {
			data_offset = offset & q2s->cluster_offset_mask;
			data_offset |= QCOW_OFLAG_COMPRESSED;
		}
		sprintf(flags, "  [0x%lx:", data_offset);
		if (offset & QCOW_OFLAG_COPIED) {
			strcat(flags, " COPIED");
		}
		if (offset & QCOW_OFLAG_COMPRESSED) {
			strcat(flags, " COMPRESSED");
		} else if (offset & QCOW_OFLAG_ZERO) {
			strcat(flags, " ZERO");
		}
		strcat(flags, "]");
	}
	if (enter) {
		strcat(flags, "\n");
	}
	strcat(entry_buff, flags);
}

//虚拟磁盘的偏移地址
static inline uint64_t qcow2_vdisk_offset(qcow2_state* q2s,
							uint32_t l1_index, uint32_t l2_index)
{
	QCowHeader *header = &q2s->header;

	return (((uint64_t)l1_index << (header->cluster_bits + header->cluster_bits - 3))
			+ ((uint64_t)l2_index << header->cluster_bits));
}

//qcow2镜像active快照或inactive快照的L2表检测与统计
static int qcow2_l2_table(qcow2_state* q2s, int l1_index, int64_t l2_offset,
						char* l1_buff, int print, int last_l1)
{
	int i, ret = 0, first = 1;
	uint64_t *l2_table;
	uint64_t compress = 0;
	uint32_t nb_csectors = 0;
	uint64_t offset, zero_offset, size, vdisk_offset;
	int unaligned = 0, invalid = 0, unused = 0, used = 0, compressed = 0;
	char l2_buff[128] = {0};

	l2_table = malloc(q2s->cluster_size);
	if (!l2_table) {
		ret = -ENOMEM;
		printf("Line: %d | malloc l2_table failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(l2_table, 0, q2s->cluster_size);

	ret = pread(q2s->fd, l2_table, q2s->cluster_size, l2_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | l2_table pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	//对L2 table引用计数
	qcow2_calculate_refcounts(q2s, l2_offset, q2s->cluster_size);

	for(i = 0; i < q2s->l2_size; i++) {
		be64_to_cpus(&l2_table[i]);

		compress = 0;
		offset = l2_table[i] & L2E_OFFSET_MASK;
		zero_offset = l2_table[i] & QCOW_OFLAG_ZERO;

		if (q2s->check_compress) {
			compress = l2_table[i] & QCOW_OFLAG_COMPRESSED;
			if (compress) {//参考check_refcounts_l2函数
				offset = l2_table[i] & q2s->cluster_offset_mask;
				nb_csectors = ((l2_table[i] >> q2s->csize_shift) &
								q2s->csize_mask) + 1;
				++compressed;
				++q2s->l2_compressed;
				zero_offset = 0;
			}
		}

		//隐藏功能, 检查预分配/动态分配镜像是否正常.
		//检查预分配/动态分配镜像的功能已开启. 并且没有打过快照.
		if (q2s->prealloc == PREALLOC_CHECK
			&& !compress && q2s->header.nb_snapshots == 0) {
			if (offset && (offset != l2_offset + (i + 1) * q2s->cluster_size)) {
				++q2s->not_prealloc;
				printf("NOT Prealloc[%d] "
						"l2_offset: 0x%lx, index:%d, data offset: 0x%lx\n",
						q2s->not_prealloc, l2_offset, i, offset);
			}
		}

		if (!offset) {//offset为0是否要输出？统计？
			++unused;
			++q2s->l2_unused;
			if (!last_l1) {//预分配/动态分配镜像, 最后一个l1表项指向的l2表表项可能会有0.
				//另外一个方法: 根据virtual_size算出l2表项个数, 然后计算有多少个l2表项为0
				++q2s->l2_not_prealloc;
			}
			if (!zero_offset) {
				continue;
			}
		}
		if (last_l1 && unused) {
			++q2s->l2_not_prealloc;
		}

		vdisk_offset = qcow2_vdisk_offset(q2s, l1_index, i);
		if (!compress && offset_into_cluster(q2s->cluster_size, offset)) {
			++unaligned;
			++q2s->l2_unaligned;
			if (print) {//-m error/dump时输出
				qcow2_entry_print(l1_buff, &first);
				printf("	l2 table[%4d], data offset: 0x%lx [0x%lx]",
						i, offset, l2_table[i]);
				if (is_color_display(q2s)) {	//-m error时输出彩色
					printf(FONT_COLOR_RED " [unaligned]" COLOR_NONE);
				} else {
					printf(" [unaligned]");
				}
				//输出虚拟磁盘中对应的地址
				printf(" | vdisk offset: 0x%lx\n", vdisk_offset);
			}
		} else if (qcow2_out_of_range(q2s, offset)) {
			++invalid;
			++q2s->l2_invalid;
			if (print) {//-m error/dump时输出
				qcow2_entry_print(l1_buff, &first);
				printf("	l2 table[%4d], data offset: 0x%lx [0x%lx]",
						i, offset, l2_table[i]);
				if (is_color_display(q2s)) {	//-m error时输出彩色
					printf(FONT_COLOR_RED " [invalid]" COLOR_NONE);
				} else {
					printf(" [invalid]");
				}
				//输出虚拟磁盘中对应的地址
				printf(" | vdisk offset: 0x%lx\n", vdisk_offset);
			}
		} else {
			//对data簇引用计数
			if (compress) {
				size = nb_csectors * SECTOR_ALIGN;
			} else {
				size = q2s->cluster_size;
			}
			if (!zero_offset) {//不带ZERO标记的L2表项
				++used;
				++q2s->l2_used;
			} else {
				//L2表项为0x0 + ZERO标记0x1, eg: 0x1
				if (l2_table[i] == QCOW_OFLAG_ZERO) {
					size = 0;
				} else {//正常的L2表项 + ZERO标记, eg: 0x50001
					size = q2s->cluster_size;
				}
			}

			qcow2_calculate_refcounts(q2s, offset & ~(SECTOR_ALIGN-1), size);
			if (print && q2s->mode == M_DUMP) {
				qcow2_entry_print(l1_buff, &first);
				memset(l2_buff, 0, sizeof(l2_buff));
				sprintf(l2_buff, "	l2 table[%4d], data offset: 0x%lx", i, offset);
				qcow2_entry_flags(q2s, l2_buff, l2_table[i], 0);
				//输出虚拟磁盘中对应的地址
				printf("%s | vdisk offset: 0x%lx\n", l2_buff, vdisk_offset);
			}
		}
	}
	//空的L2 table也dump出L2 table的offset信息
	if (q2s->mode == M_DUMP && first) {
		qcow2_entry_print(l1_buff, &first);
	}
	if (print && !first) {
		printf("\n	L2 Entry: unaligned: %d, invalid: %d, unused: %d, used: %d",
					unaligned, invalid, unused, used);
		if (compressed) {
			printf(", compressed: %d", compressed);
		}
		printf("\n	--------------------------------------------------------\n\n");
	}

out:
	free(l2_table);
	return ret;
}

//qcow2镜像active快照或inactive快照的L1表检测与统计
static int qcow2_l1_table(qcow2_state* q2s, uint64_t l1_table_offset,
						int64_t l1_size, int print)
{
	int64_t l1_size2 = align_offset(l1_size * sizeof(uint64_t), 512);
	uint64_t *l1_table = NULL;
	int64_t l2_offset;
	int i, ret = 0, last_l1 = 0;
	char l1_buff[128] = {0};

	if (!l1_table_offset || !l1_size) {//快照头已损坏
		if (is_color_display(q2s)) {
			printf("L1 Table:       "
					FONT_COLOR_RED "[offset: 0x%lx, len: %ld]\n\n" COLOR_NONE,
					l1_table_offset, l1_size);
		} else {
			printf("L1 Table:       [offset: 0x%lx, len: %ld]\n\n",
					l1_table_offset, l1_size);
		}
		goto out;
	}
	printf("L1 Table:       [offset: 0x%lx, len: %ld]\n\n", l1_table_offset, l1_size);

	l1_table = malloc(l1_size2);
	if (!l1_table) {
		ret = -ENOMEM;
		printf("Line: %d | malloc l1_table failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(l1_table, 0, l1_size2);

	ret = pread(q2s->fd, l1_table, l1_size2, l1_table_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | l1_table pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	//对L1 table引用计数
	qcow2_calculate_refcounts(q2s, l1_table_offset, l1_size2);

	for (i = 0; i < l1_size; i++) {
		be64_to_cpus(&l1_table[i]);
		l2_offset = l1_table[i] & L1E_OFFSET_MASK;

		if (!l2_offset) {//l2_offset为0是否要输出？统计？
			++q2s->l1_unused;
			continue;
		}
		if (i == l1_size - 1) {
			last_l1 = 1;
		}
		if (offset_into_cluster(q2s->cluster_size, l2_offset)) {
			++q2s->l1_unaligned;
			if (print) {//-m error/dump时输出
				printf("l1 table[%4d], l2 offset: 0x%lx", i, l2_offset);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf(FONT_COLOR_RED " [unaligned]\n" COLOR_NONE);
				} else {
					printf(" [unaligned]\n");
				}
			}
		} else if (qcow2_out_of_range(q2s, l2_offset)) {
			++q2s->l1_invalid;
			if (print) {//-m error/dump时输出
				printf("l1 table[%4d], l2 offset: 0x%lx", i, l2_offset);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf(FONT_COLOR_RED " [invalid]\n" COLOR_NONE);
				} else {
					printf(" [invalid]\n");
				}
			}
		} else {
			++q2s->l1_used;
			if (print) {//sprintf到buffer中, 作为参数传到qcow2_l2_table.
				memset(l1_buff, 0, sizeof(l1_buff));
				sprintf(l1_buff, "l1 table[%4d], l2 offset: 0x%lx", i, l2_offset);
				qcow2_entry_flags(q2s, l1_buff, l1_table[i], 1);
			}
			ret = qcow2_l2_table(q2s, i, l2_offset, l1_buff, print, last_l1);
			if (ret < 0) {
				goto out;
			}
		}
	}

out:
	if (l1_table) {
		free(l1_table);
	}

	return ret;
}

//输出qcow2镜像active快照或inactive快照的L1表检测统计情况
static void qcow2_l1_result(qcow2_state* q2s,
							int64_t l1_unaligned, int64_t l1_invalid,
							int64_t l1_unused, int64_t l1_used,
							int64_t l2_unaligned, int64_t l2_invalid,
							int64_t l2_unused, int64_t l2_used,
							int64_t l2_compressed)
{
	if (is_color_display(q2s)) {	//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_CYAN "Result:\n" COLOR_NONE);		//输出青色

		printf("L1 Table:       unaligned: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
				"invalid: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
				"unused: %ld, used: %ld\n",
				l1_unaligned, l1_invalid, l1_unused, l1_used);

		printf("L2 Table:       unaligned: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
				"invalid: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
				"unused: %ld, used: %ld\n",
				l2_unaligned, l2_invalid, l2_unused, l2_used);
		if (l2_compressed) {
			printf("                compressed: " FONT_COLOR_YELLOW "%ld\n" COLOR_NONE,
				l2_compressed);
		}
	} else {
		printf("Result:\n");

		printf("L1 Table:       unaligned: %ld, "
				"invalid: %ld, unused: %ld, used: %ld\n",
				l1_unaligned, l1_invalid, l1_unused, l1_used);

		printf("L2 Table:       unaligned: %ld, "
				"invalid: %ld, unused: %ld, used: %ld\n",
				l2_unaligned, l2_invalid, l2_unused, l2_used);
		if (l2_compressed) {
			printf("                compressed: %ld\n", l2_compressed);
		}
	}
}

//设置qcow2镜像active快照状态(是否损坏)
static inline void qcow2_set_active_status(qcow2_state* q2s)
{
	q2s->active_corrupt = qcow2_is_snapshot_corrupt(q2s->l1_unaligned,
													q2s->l1_invalid,
													q2s->l2_unaligned,
													q2s->l2_invalid);
}

//qcow2镜像active快照是否损坏
static inline int qcow2_is_active_corrupt(qcow2_state* q2s)
{
	return q2s->active_corrupt;
}

//检测qcow2镜像active快照的L1表和L2表簇对齐情况
static int qcow2_active_snapshot(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	int ret = 0, print = 0;

	if (!strcmp(q2s->snapshot, "active") || !strcmp(q2s->snapshot, "all")) {
		print = (q2s->mode != M_CHECK) && (q2s->output != O_REFCOUNT);
	}

	if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_GREEN "Active Snapshot:\n" COLOR_NONE);	//输出绿色
	} else {
		printf("Active Snapshot:\n");
	}
	printf("----------------------------------------------------------------\n");

	ret = qcow2_l1_table(q2s, header->l1_table_offset, header->l1_size, print);
	if (ret < 0) {
		goto out;
	}

	qcow2_set_active_status(q2s);

	qcow2_l1_result(q2s, q2s->l1_unaligned, q2s->l1_invalid,
					q2s->l1_unused, q2s->l1_used, q2s->l2_unaligned,
					q2s->l2_invalid, q2s->l2_unused, q2s->l2_used,
					q2s->l2_compressed);

	printf("\n================================================================\n\n");

out:
	return ret;
}

static inline uint64_t qcow2_get_image_refcount(qcow2_state* q2s, uint64_t offset,
							uint64_t *refcount_array_index, uint64_t *block_index)
{
	QCowHeader *header = &q2s->header;
	uint64_t cluster_offset, cluster_index;
	uint64_t *refcount_array;
	uint64_t refcount;

	cluster_offset = qcow2_start_of_cluster(q2s, offset);
	cluster_index = cluster_offset >> header->cluster_bits;
	*refcount_array_index = cluster_index >> q2s->refcount_block_bits;
	*block_index = cluster_index & (q2s->refcount_block_size - 1);

	if (*refcount_array_index >= q2s->refcount_array_size) {
		printf("Line: %d | out of range: table index: %lu >= table size: %ld\n",
				__LINE__, *refcount_array_index, q2s->refcount_array_size);
		abort();
	}

	//从镜像读取的实际refcount[包括引用计数修复之后的refcout]
	refcount_array = q2s->refcount_array[*refcount_array_index];
	if (!refcount_array) {
		return 0;
	}
	refcount = q2s->get_refcount(refcount_array, *block_index);

	return refcount;
}

static int qcow2_check_l2_copied(qcow2_state* q2s, int l1_index,
								int64_t l2_offset, int print)
{
	QCowHeader *header = &q2s->header;
	int i, ret = 0, l2_dirty = 0;
	uint64_t *l2_table;
	uint64_t l2_entry;
	uint64_t refcount;
	uint64_t offset;
	uint64_t compress = 0;
	uint64_t table_index = 0, block_index = 0;

	l2_table = malloc(q2s->cluster_size);
	if (!l2_table) {
		ret = -ENOMEM;
		printf("Line: %d | malloc l2_table failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(l2_table, 0, q2s->cluster_size);

	ret = pread(q2s->fd, l2_table, q2s->cluster_size, l2_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | l2_table pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	for(i = 0; i < q2s->l2_size; i++) {
		be64_to_cpus(&l2_table[i]);
		l2_entry = l2_table[i];
		offset = l2_table[i] & L2E_OFFSET_MASK;

		if (q2s->check_compress) {
			compress = l2_table[i] & QCOW_OFLAG_COMPRESSED;
			if (compress) {
				offset = l2_table[i] & q2s->cluster_offset_mask;
			}
		}

		if (!offset
			|| offset_into_cluster(q2s->cluster_size, offset)
			|| qcow2_out_of_range(q2s, offset)) {
			continue;
		}

		qcow2_cluster_reuse_iteration(q2s, l1_index, i, l2_offset, offset);

		if (compress) {//压缩的数据项
			continue;
		}

		//压缩的数据项不计算
		if (l2_table[i] & QCOW_OFLAG) {
			++q2s->active_copied;
		}

		//从镜像读取的实际refcount[包括引用计数修复之后的refcout]
		refcount = qcow2_get_image_refcount(q2s, offset, &table_index, &block_index);

		//压缩的数据项不检查COPIED标志
		if ((refcount == 1) != ((l2_entry & QCOW_OFLAG_COPIED) != 0)) {
			++l2_dirty;
			++q2s->l2_copied;
			l2_table[i] = refcount == 1
								? l2_entry |  QCOW_OFLAG_COPIED
								: l2_entry & ~QCOW_OFLAG_COPIED;
			if (print) {
				if (is_color_display(q2s)) {
					printf(FONT_COLOR_RED "%s OFLAG_COPIED " COLOR_NONE
							"Data cluster:   l1_index=%d, l2_index=%d, l2_entry=0x%lx\n",
							q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
							l1_index, i, l2_entry);
					printf("%s Refcount block: table_index=%ld, block_index=%ld, refcount=%ld\n",
							q2s->repair >= FIX_ERRORS ? "                      " : "                  ",
							table_index, block_index, refcount);
				} else {
					printf("%s OFLAG_COPIED Data cluster:   "
							"l1_index=%d, l2_index=%d, l2_entry=0x%lx\n",
							q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
							l1_index, i, l2_entry);
					printf("%s Refcount block: table_index=%ld, block_index=%ld, refcount=%ld\n",
							q2s->repair >= FIX_ERRORS ? "                      " : "                  ",
							table_index, block_index, refcount);
				}
			}
		}
		l2_table[i] = cpu_to_be64(l2_table[i]);
	}
	//重建重用簇时, 不修复COPIED ERROR
	if (qcow2_is_repair_copied(q2s) && l2_dirty
		&& !qcow2_rebuild_reused_active_cluster(q2s)) {
		ret = pwrite(q2s->fd, l2_table, q2s->cluster_size, l2_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | l2_table pwrite failed: %d\n", __LINE__, ret);
		} else {
			ret = 0;
			fsync(q2s->fd);
		}
	}

out:
	free(l2_table);
	return ret;
}

static int qcow2_check_l1_copied(qcow2_state* q2s,
								uint64_t l1_table_offset,
								int64_t l1_size, int print)
{
	QCowHeader *header = &q2s->header;
	int64_t l1_size2 = align_offset(l1_size * sizeof(uint64_t), 512);
	uint64_t *l1_table = NULL;
	int i, ret = 0;
	uint64_t l1_entry;
	uint64_t refcount;
	int64_t l2_offset;
	uint64_t table_index = 0, block_index = 0;

	l1_table = malloc(l1_size2);
	if (!l1_table) {
		ret = -ENOMEM;
		printf("Line: %d | malloc l1_table failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(l1_table, 0, l1_size2);

	ret = pread(q2s->fd, l1_table, l1_size2, l1_table_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | l1_table pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	for (i = 0; i < l1_size; i++) {
		be64_to_cpus(&l1_table[i]);
		l1_entry = l1_table[i];
		l2_offset = l1_table[i] & L1E_OFFSET_MASK;

		if (!l2_offset || offset_into_cluster(q2s->cluster_size, l2_offset)
			|| qcow2_out_of_range(q2s, l2_offset)) {
			continue;
		}

		qcow2_cluster_reuse_iteration(q2s, i, INVALID, l1_table_offset, l2_offset);

		//从镜像读取的实际refcount[包括引用计数修复之后的refcout]
		refcount = qcow2_get_image_refcount(q2s, l2_offset, &table_index, &block_index);

		if ((refcount == 1) != ((l1_entry & QCOW_OFLAG_COPIED) != 0)) {
			++q2s->l1_copied;
			l1_table[i] = refcount == 1
								? l1_entry |  QCOW_OFLAG_COPIED
								: l1_entry & ~QCOW_OFLAG_COPIED;
			if (print) {
				if (is_color_display(q2s)) {
					printf(FONT_COLOR_RED "%s OFLAG_COPIED " COLOR_NONE
							"L2 cluster:     l1_index=%d, l1_entry=0x%lx\n",
							q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
							i, l1_entry);
					printf("%s Refcount block: table_index=%ld, block_index=%ld, refcount=%ld\n",
							q2s->repair >= FIX_ERRORS ? "                      " : "                  ",
							table_index, block_index, refcount);
				} else {
					printf("%s OFLAG_COPIED L2 cluster:     "
							"l1_index=%d, l1_entry=0x%lx\n",
							q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
							i, l1_entry);
					printf("%s Refcount block: table_index=%ld, block_index=%ld, refcount=%ld\n",
							q2s->repair >= FIX_ERRORS ? "                      " : "                  ",
							table_index, block_index, refcount);
				}
			}
		}
		l1_table[i] = cpu_to_be64(l1_table[i]);
		ret = qcow2_check_l2_copied(q2s, i, l2_offset, print);
		if (ret < 0) {
			goto out;
		}
	}
	//重建重用簇时, 不修复COPIED ERROR
	if (qcow2_is_repair_copied(q2s) && q2s->l1_copied
		&& !qcow2_rebuild_reused_active_cluster(q2s)) {
		ret = pwrite(q2s->fd, l1_table, l1_size2, l1_table_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | l1_table pwrite failed: %d\n", __LINE__, ret);
		} else {
			ret = 0;
			fsync(q2s->fd);
		}
	}

	if (print && (q2s->l1_copied || q2s->l2_copied)) {
		printf("----------------------------------------------------------------\n\n");
	}

out:
	free(l1_table);
	return ret;
}

static inline int qcow2_is_check_oflag_copied(qcow2_state* q2s)
{
	return !(q2s->refcount_table == NULL || q2s->repair == FIX_NONE);
}

#define SN_HEADER	-2

//检查引用计数与QCOW_OFLAG_COPIED标记是否冲突并修复
static int qcow2_check_oflag_copied(qcow2_state* q2s)
{
	int ret = 0;
	uint64_t size;
	QCowHeader *header = &q2s->header;
	int print = ((q2s->mode == M_CHECK && q2s->repair >= FIX_ERRORS)
				|| q2s->mode >= M_ERROR);

	if (!qcow2_is_check_oflag_copied(q2s) || !qcow2_is_check_all(q2s)) {
		return ret;
	}

	//镜像L1L2索引表已损坏时，不允许进行修复
	if (qcow2_L1L2_is_corrupt(q2s) && q2s->repair > FIX_CHECK) {
		q2s->repair = FIX_CHECK;
	}

	if (is_color_display(q2s)) {
		printf(FONT_COLOR_GREEN "COPIED OFLAG:\n" COLOR_NONE);
	} else {
		printf("COPIED OFLAG:\n");
	}
	printf("----------------------------------------------------------------\n");

	qcow2_cluster_reuse_sort(q2s);	//从小到大排序

	if (header->snapshots_offset && header->nb_snapshots) {//快照头特殊处理
		size_t offset = offsetof(QCowHeader, snapshots_offset);
		qcow2_cluster_reuse_iteration(q2s, 0, SN_HEADER,
						offset, header->snapshots_offset);
	}

	ret = qcow2_check_l1_copied(q2s, header->l1_table_offset, header->l1_size, print);
	if (ret < 0) {
		goto out;
	}

	if (q2s->mode == M_CHECK || (q2s->mode >= M_ERROR
		&& !q2s->l1_copied && !q2s->l2_copied)) {
		printf("\n");
	}

	size = q2s->active_copied * q2s->cluster_size;
	if (is_color_display(q2s)) {
		printf(FONT_COLOR_CYAN "Result:\n" COLOR_NONE);	//输出青色

		//ERROR OFLAG_COPIED data cluster: l2_entry=80000000560c0000 refcount=0
		printf("L1 Table %s OFLAG_COPIED: " FONT_COLOR_YELLOW "%ld\n" COLOR_NONE,
				q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
				q2s->l1_copied);
		printf("L2 Table %s OFLAG_COPIED: " FONT_COLOR_YELLOW "%ld\n" COLOR_NONE,
				q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
				q2s->l2_copied);

		printf("Active L2 COPIED: "
				FONT_COLOR_YELLOW "%ld [%ld / %ldM / %ldG]\n\n" COLOR_NONE,
				q2s->active_copied, size, size/ONE_M, size/ONE_G);
	} else {
		printf("Result:\n");

		//ERROR OFLAG_COPIED data cluster: l2_entry=80000000560c0000 refcount=0
		printf("L1 Table %s OFLAG_COPIED: %ld\n",
				q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
				q2s->l1_copied);
		printf("L2 Table %s OFLAG_COPIED: %ld\n",
				q2s->repair >= FIX_ERRORS ? "Repairing" : "ERROR",
				q2s->l2_copied);

		printf("Active L2 COPIED: %ld [%ld / %ldM / %ldG]\n\n",
				q2s->active_copied, size, size/ONE_M, size/ONE_G);
	}

	printf("================================================================\n\n");

out:
	return ret;
}

static inline int qcow2_has_cluster_reuse_info(qcow2_state* q2s)
{
	return q2s->cluster_reuse && q2s->cluster_reuse_cn;
}

static inline int qcow2_active_cluster_reused(qcow2_state* q2s)
{
	return q2s->cluster_reused;
}

static int qcow2_cluster_cmp(const void* a, const void* b)
{
	cluster_reuse_info* info_a = (cluster_reuse_info*)a;
	cluster_reuse_info* info_b = (cluster_reuse_info*)b;

	if (info_a->cluster_offset > info_b->cluster_offset) {
		return 1;
	}
	return -1;
}

//排序: 从小到大
static inline void qcow2_cluster_reuse_sort(qcow2_state* q2s)
{
	if (q2s->cluster_reuse_cn > 0) {
		qsort((void *)q2s->cluster_reuse, q2s->cluster_reuse_cn,
				sizeof(cluster_reuse_info), qcow2_cluster_cmp);
	}
}

//记录引用计数错误对应簇的offset
static inline void qcow2_cluster_reuse_record_offset(qcow2_state* q2s, uint64_t offset)
{
	if (q2s->cluster_reuse_cn == q2s->cluster_reuse_max - 1) {
		return ;
	}
	q2s->cluster_reuse[q2s->cluster_reuse_cn].cluster_offset = offset;
	++q2s->cluster_reuse_cn;
}

//记录引用计数错误对应簇的相关信息
static inline void qcow2_cluster_reuse_record_l1_l2(qcow2_state* q2s, int i,
							int l1_index, int l2_index, uint64_t base_offset)
{
	int n;
	uint64_t index_offset;

	++q2s->cluster_reuse[i].refcount;

	for (n = 0; n < INDEX_CN; n++) {
		if (q2s->cluster_reuse[i].l1_l2[n].l1_index == INVALID
			&& q2s->cluster_reuse[i].l1_l2[n].l2_index == INVALID) {
			q2s->cluster_reuse[i].l1_l2[n].l1_index = l1_index;
			q2s->cluster_reuse[i].l1_l2[n].l2_index = l2_index;

			if (l2_index == SN_HEADER) {//快照头
				index_offset = base_offset;
			} else if (l2_index == INVALID) {//L1表项
				index_offset = base_offset + sizeof(uint64_t) * l1_index;
			} else {//L2表项
				index_offset = base_offset + sizeof(uint64_t) * l2_index;
			}
			q2s->cluster_reuse[i].l1_l2[n].index_offset = index_offset;
			break;
		}
	}
}

//从小到大排序, 二分查找
static int qcow2_bsearch_cluster_reuse(qcow2_state* q2s, uint64_t offset)
{
	int mid;
	int low = 0;
	int high = q2s->cluster_reuse_cn - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (q2s->cluster_reuse[mid].cluster_offset > offset) {
			high = mid - 1;
		} else if (q2s->cluster_reuse[mid].cluster_offset < offset) {
			low = mid + 1;
		} else {
			return mid;
		}
	}

	return -1;
}

//查找offset是否为引用计数错误中对应的offset, 并记录, 判断是否簇重用
static void qcow2_cluster_reuse_iteration(qcow2_state* q2s, int l1_index,
					int l2_index, uint64_t base_offset, uint64_t offset)
{
	int i;

	//采用二分查找优化性能
	i = qcow2_bsearch_cluster_reuse(q2s, offset);
	if (i != -1) {
		qcow2_cluster_reuse_record_l1_l2(q2s, i, l1_index, l2_index, base_offset);
	}
}

//输出重用簇
static void qcow2_print_reused_active_cluster(qcow2_state* q2s, int i)
{
	int n;
	int print = (q2s->mode != M_CHECK);

	if (print) {
		if (is_color_display(q2s)) {
			if (q2s->cluster_reuse[i].l1_l2[0].l2_index == SN_HEADER) {//快照头
				printf(FONT_COLOR_RED "REUSED " COLOR_NONE
						"cluster offset: 0x%lx | "
						"snapshot header [offset: 0x%lx] | refcount: %d\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[0].index_offset,
						q2s->cluster_reuse[i].refcount);
			} else {//L1/L2表项
				printf(FONT_COLOR_RED "REUSED " COLOR_NONE
						"cluster offset: 0x%lx | "
						"l1_index: %4d, l2_index: %4d [offset: 0x%lx] | refcount: %d\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[0].l1_index,
						q2s->cluster_reuse[i].l1_l2[0].l2_index,
						q2s->cluster_reuse[i].l1_l2[0].index_offset,
						q2s->cluster_reuse[i].refcount);
			}

			for (n = 1; n < INDEX_CN; n++) {
				if (q2s->cluster_reuse[i].l1_l2[n].l1_index == INVALID
					&& q2s->cluster_reuse[i].l1_l2[n].l2_index == INVALID) {
					break;
				}

				printf(FONT_COLOR_RED "REUSED " COLOR_NONE
						"cluster offset: 0x%lx | "
						"l1_index: %4d, l2_index: %4d [offset: 0x%lx]\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[n].l1_index,
						q2s->cluster_reuse[i].l1_l2[n].l2_index,
						q2s->cluster_reuse[i].l1_l2[n].index_offset);
			}
		} else {
			if (q2s->cluster_reuse[i].l1_l2[0].l2_index == SN_HEADER) {//快照头
				printf("REUSED cluster offset: 0x%lx | "
						"snapshot header [offset: 0x%lx] | refcount: %d\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[0].index_offset,
						q2s->cluster_reuse[i].refcount);
			} else {//L1/L2表项
				printf("REUSED cluster offset: 0x%lx | "
						"l1_index: %4d, l2_index: %4d [offset: 0x%lx] | refcount: %d\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[0].l1_index,
						q2s->cluster_reuse[i].l1_l2[0].l2_index,
						q2s->cluster_reuse[i].l1_l2[0].index_offset,
						q2s->cluster_reuse[i].refcount);
			}

			for (n = 1; n < INDEX_CN; n++) {
				if (q2s->cluster_reuse[i].l1_l2[n].l1_index == INVALID
					&& q2s->cluster_reuse[i].l1_l2[n].l2_index == INVALID) {
					break;
				}

				printf("REUSED cluster offset: 0x%lx | "
						"l1_index: %4d, l2_index: %4d [offset: 0x%lx]\n",
						q2s->cluster_reuse[i].cluster_offset,
						q2s->cluster_reuse[i].l1_l2[n].l1_index,
						q2s->cluster_reuse[i].l1_l2[n].l2_index,
						q2s->cluster_reuse[i].l1_l2[n].index_offset);
			}
		}
	}
}

//重建重用簇(rebuild reused active cluster)
static int qcow2_do_rebuild_reused_active_cluster(qcow2_state* q2s, int i)
{
	int n;
	int ret = 0;
	uint64_t entry = 0;
	uint64_t offset = 0;
	uint64_t index_offset;
	uint64_t * buff = NULL;
	static int rebuild = 0;

	if (!qcow2_rebuild_reused_active_cluster(q2s)) {
		return 0;
	}

	buff = malloc(q2s->cluster_size);
	if (!buff) {
		ret = -ENOMEM;
		printf("Line: %d | malloc buffer failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(buff, 0, q2s->cluster_size);

	//读取重用簇中的数据
	ret = pread(q2s->fd, buff, q2s->cluster_size, q2s->cluster_reuse[i].cluster_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | reuse_active_cluster pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	//重用簇(reuse_active_cluster)的重建过程
	for (n = 1; n < INDEX_CN; n++) {
		if (q2s->cluster_reuse[i].l1_l2[n].l1_index == INVALID
			&& q2s->cluster_reuse[i].l1_l2[n].l2_index == INVALID) {
			break;
		}
		++rebuild;

#ifdef ALLOC_CLUSTER_IMRT
		//Allocates clusters using an in-memory refcount table (IMRT) in contrast to
		//the on-disk refcount structures.
		offset = qcow2_alloc_cluster_imrt(q2s);
		if (offset == 0) {
			ret = -ENOSPC;
			break;
		}
#else
		//从文件末尾处给需要重建的reused_active_cluster分配簇空间
		offset = qcow2_start_of_cluster(q2s, q2s->seek_end);	//对文件末尾偏移簇对齐
		offset += rebuild * q2s->cluster_size;	//保证是末尾后面的簇的偏移
#endif

		//对已经重建的重用簇IMRT计数
		qcow2_calculate_refcounts(q2s, offset, q2s->cluster_size);

		ret = pwrite(q2s->fd, buff, q2s->cluster_size, offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | reused_active_cluster pwrite failed: %d\n",
					__LINE__, ret);
			break;
		} else {
			fsync(q2s->fd);
		}

		entry = cpu_to_be64(offset);
		index_offset = q2s->cluster_reuse[i].l1_l2[n].index_offset;
		//重用簇中的数据写入到新分配中的簇中
		ret = pwrite(q2s->fd, &entry, sizeof(entry), index_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | reuse_entry pwrite failed: %d\n",
					__LINE__, ret);
			break;
		} else {
			fsync(q2s->fd);
		}

		if (is_color_display(q2s)) {
			printf(FONT_COLOR_RED "Rebuild " COLOR_NONE
					"reused active cluster[%4d]: "
					"l1_index: %4d, l2_index: %4d [offset: 0x%lx]\n",
					i,
					q2s->cluster_reuse[i].l1_l2[n].l1_index,
					q2s->cluster_reuse[i].l1_l2[n].l2_index,
					offset);
		} else {
			printf("Rebuild reused active cluster[%4d]: "
					"l1_index: %4d, l2_index: %4d [offset: 0x%lx]\n",
					i,
					q2s->cluster_reuse[i].l1_l2[n].l1_index,
					q2s->cluster_reuse[i].l1_l2[n].l2_index,
					offset);
		}
	}

out:
	if (buff) {
		free(buff);
	}
	return ret;
}

//检查引用计数出现错误后, 簇被重新分配的情况:
//例如: 引用计数错误，本来引用计数应该为1的，变成为0了，qemu在新分配簇时，
//发现这个簇的引用计数为0，就重新分配出去了，导致两个L2指向了同一个簇。
static int qcow2_check_active_cluster_reused(qcow2_state* q2s)
{
	int i, ret = 0;
	int print = (q2s->mode != M_CHECK);

	if (!qcow2_is_check_all(q2s) || !qcow2_has_cluster_reuse_info(q2s)
		|| !qcow2_maybe_wrong(q2s) || !qcow2_is_check_oflag_copied(q2s)) {
		return ret;
	}

	if (is_color_display(q2s)) {
		printf(FONT_COLOR_GREEN "Active Cluster:\n" COLOR_NONE);
	} else {
		printf("Active Cluster:\n");
	}
	printf("----------------------------------------------------------------\n");

	for (i = 0; i < q2s->cluster_reuse_cn; i++) {
		//active cluster是否被重用
		if (q2s->cluster_reuse[i].refcount < ACTIVE_CLUSTER_REUSED_REFCOUNT) {
			continue;
		}

		++q2s->cluster_reused;

		qcow2_print_reused_active_cluster(q2s, i);

		ret = qcow2_do_rebuild_reused_active_cluster(q2s, i);
		if (ret < 0) {
			printf("Line: %d | qcow2_do_rebuild_reused_active_cluster failed: %d\n",
					__LINE__, ret);
			break;
		}
	}

	if (print && q2s->cluster_reused) {
		printf("----------------------------------------------------------------\n");
	}

	if (is_color_display(q2s)) {	//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_CYAN "\nResult:\n" COLOR_NONE);		//输出青色
		printf("Active Cluster:	reuse: " FONT_COLOR_YELLOW "%d\n\n" COLOR_NONE,
				q2s->cluster_reused);
	} else {
		printf("\nResult:\n");
		printf("Active Cluster:	reuse: %d\n\n", q2s->cluster_reused);
	}

	printf("================================================================\n\n");

	return ret;
}

//读取qcow2镜像inactive快照信息
static int qcow2_read_snapshots(qcow2_state* q2s, QCowSnapshot **psn)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	uint32_t snapshots_size = nb_snapshots * sizeof(QCowSnapshot);
	QCowSnapshot *snapshots = NULL;

	QCowSnapshotHeader h;
	QCowSnapshotExtraData extra;
	QCowSnapshot *sn;
	int i, id_str_size, name_size;
	int64_t offset;
	uint32_t extra_data_size;
	int ret = 0;
	int sn_corrupt = 0;

	snapshots = malloc(snapshots_size);
	if (!snapshots) {
		ret = -ENOMEM;
		printf("Line: %d | malloc snapshots failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(snapshots, 0, snapshots_size);

	offset = header->snapshots_offset;
	for(i = 0; i < nb_snapshots; i++) {
		/* Read statically sized part of the snapshot header */
		offset = align_offset(offset, 8);
		memset(&h, 0, sizeof(h));
		ret = pread(q2s->fd, &h, sizeof(h), offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pread sn header failed: %d\n", __LINE__, ret);
			goto fail;
		}

		offset += sizeof(h);
		sn = snapshots + i;
		sn->l1_table_offset = be64_to_cpu(h.l1_table_offset);
		sn->l1_size = be32_to_cpu(h.l1_size);
		sn->vm_state_size = be32_to_cpu(h.vm_state_size);
		sn->date_sec = be32_to_cpu(h.date_sec);
		sn->date_nsec = be32_to_cpu(h.date_nsec);
		sn->vm_clock_nsec = be64_to_cpu(h.vm_clock_nsec);
		extra_data_size = be32_to_cpu(h.extra_data_size);

		id_str_size = be16_to_cpu(h.id_str_size);
		name_size = be16_to_cpu(h.name_size);

		/* Read extra data */
		memset(&extra, 0, sizeof(extra));
		ret = pread(q2s->fd, &extra, MIN(sizeof(extra), extra_data_size), offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pread sn extra failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += extra_data_size;

		/* Read snapshot ID */
		sn->id_str = malloc(id_str_size + 1);
		if (!sn->id_str) {
			ret = -ENOMEM;
			printf("Line: %d | malloc snapshot id_str failed: %d\n", __LINE__, ret);
			goto fail;
		}
		ret = pread(q2s->fd, sn->id_str, id_str_size, offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pread sn id failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += id_str_size;
		sn->id_str[id_str_size] = '\0';

		/* Read snapshot name */
		sn->name = malloc(name_size + 1);
		if (!sn->name) {
			ret = -ENOMEM;
			printf("Line: %d | malloc snapshot name failed: %d\n", __LINE__, ret);
			goto fail;
		}
		ret = pread(q2s->fd, sn->name, name_size, offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pread sn name failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += name_size;
		sn->name[name_size] = '\0';

        if (offset - header->snapshots_offset > QCOW_MAX_SNAPSHOTS_SIZE) {
			printf("Line: %d | offset[0x%lx] - header->snapshots_offset > QCOW_MAX_SNAPSHOTS_SIZE\n",
					__LINE__, offset);
			ret = -EFBIG;
			goto fail;
        }

		if (sn->l1_table_offset == 0 || sn->l1_size == 0) {
			sn_corrupt = 1;
		}

		if (sn->l1_table_offset > q2s->seek_end) {
			printf("Line: %d | sn[%d]: l1_table_offset[0x%lx] > seek_end[0x%lx]\n",
					__LINE__, i, sn->l1_table_offset, q2s->seek_end);
			sn_corrupt = 1;
			//ret = -EFBIG;
			//goto fail;
		}
	}
#ifdef SNAPSHOT_HEADER_CORRUPT_CORE	//与qemu代码保持一致, 快照头损坏, 此处可能出core
	assert(offset - header->snapshots_offset <= INT_MAX);
#else
	if (!(offset - header->snapshots_offset <= INT_MAX)) {	//快照头损坏, 避免出core
		q2s->image_status = S_IMAGE_SN_HEADER;
		ret = -EFBIG;
		printf("Line: %d | offset[0x%lx] - header->snapshots_offset <= INT_MAX false\n",
				__LINE__, offset);
		goto fail;
	}
#endif
	q2s->snapshots_size = offset - header->snapshots_offset;
	*psn = snapshots;

	//对快照头引用计数
	if (sn_corrupt) {
		qcow2_calculate_refcounts(q2s, header->snapshots_offset, q2s->cluster_size);
	} else {
		qcow2_calculate_refcounts(q2s, header->snapshots_offset, q2s->snapshots_size);
	}

	return 0;

fail:
	for(i = 0; i < nb_snapshots; i++) {
		free(snapshots[i].id_str);
		free(snapshots[i].name);
	}
	free(snapshots);

	return ret;
}

//写qcow2镜像inactive快照头信息
static int qcow2_write_snapshots(qcow2_state* q2s, QCowSnapshot *snapshots)
{
    int ret, i;
    QCowSnapshot *sn;
    QCowSnapshotHeader h;
    QCowSnapshotExtraData extra;
    int64_t offset, snapshots_offset = 0;
	int name_size, id_str_size, snapshots_size;
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;

    /* compute the size of the snapshots */
    offset = 0;
    for(i = 0; i < nb_snapshots; i++) {
        sn = snapshots + i;
        offset = align_offset(offset, 8);
        offset += sizeof(h);
        offset += sizeof(extra);
        offset += strlen(sn->id_str);
        offset += strlen(sn->name);

        if (offset > QCOW_MAX_SNAPSHOTS_SIZE) {
			printf("Line: %d | offset > QCOW_MAX_SNAPSHOTS_SIZE\n", __LINE__);
            ret = -EFBIG;
            goto fail;
        }
    }

	offset = header->snapshots_offset;

	/* Write all snapshots to the old list */
	for(i = 0; i < nb_snapshots; i++) {
		sn = snapshots + i;
		memset(&h, 0, sizeof(h));
		h.l1_table_offset = cpu_to_be64(sn->l1_table_offset);
		h.l1_size = cpu_to_be32(sn->l1_size);
		/* If it doesn't fit in 32 bit, older implementations should treat it
		 * as a disk-only snapshot rather than truncate the VM state */
		if (sn->vm_state_size <= 0xffffffff) {
			h.vm_state_size = cpu_to_be32(sn->vm_state_size);
		}
		h.date_sec = cpu_to_be32(sn->date_sec);
		h.date_nsec = cpu_to_be32(sn->date_nsec);
		h.vm_clock_nsec = cpu_to_be64(sn->vm_clock_nsec);
		h.extra_data_size = cpu_to_be32(sizeof(extra));

		memset(&extra, 0, sizeof(extra));
		extra.vm_state_size_large = cpu_to_be64(sn->vm_state_size);
		extra.disk_size = cpu_to_be64(sn->disk_size);

		id_str_size = strlen(sn->id_str);
		name_size = strlen(sn->name);
		assert(id_str_size <= UINT16_MAX && name_size <= UINT16_MAX);
		h.id_str_size = cpu_to_be16(id_str_size);
		h.name_size = cpu_to_be16(name_size);
		offset = align_offset(offset, 8);

		ret = pwrite(q2s->fd, &h, sizeof(h), offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pwrite sn header failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += sizeof(h);

		ret = pwrite(q2s->fd, &extra, sizeof(extra), offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pwrite sn extra failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += sizeof(extra);

		ret = pwrite(q2s->fd, sn->id_str, id_str_size, offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pwrite sn id_str failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += id_str_size;

		ret = pwrite(q2s->fd, sn->name, name_size, offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pwrite sn name failed: %d\n", __LINE__, ret);
			goto fail;
		}
		offset += name_size;
	}
	fsync(q2s->fd);

	return 0;

fail:
	return ret;
}

//公共接口, 被qcow2_inactive_snapshot/qcow2_revert_snapshot/
//qcow2_delete_snapshot/qcow2_exclude_snapshot等函数调用
static int qcow2_list_and_find_snapshot(qcow2_state* q2s, QCowSnapshot **psn, int find)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	QCowSnapshot *sn = NULL;
	int i, err, ret = -1;
	struct tm tm;
	time_t ti;
	char date_buf[128];

	ret = qcow2_read_snapshots(q2s, psn);
	if (ret < 0) {
		printf("Line: %d | qcow2_read_snapshots failed: %d\n", __LINE__, ret);
		return ret;
	}
	sn = *psn;

	if (is_color_display(q2s)) {//在终端输出彩色
		printf(FONT_COLOR_GREEN "Inactive Snapshot:\n" COLOR_NONE);	//输出绿色
	} else {
		printf("Inactive Snapshot:\n");
	}
	printf("----------------------------------------------------------------\n");

	for (i = 0; i < nb_snapshots; i++) {
		ti = sn[i].date_sec;
		localtime_r(&ti, &tm);
		strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm);

		err = qcow2_validate_table_offset(q2s, sn[i].l1_table_offset,
									sn[i].l1_size, sizeof(uint64_t));

		//list snapshots
		if (!sn[i].l1_table_offset || !sn[i].l1_size || err) {//快照头已损坏
			q2s->image_status = S_IMAGE_SN_HEADER;
			if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色
				printf(FONT_COLOR_GREEN "snapshot[%d]:	" COLOR_NONE
						"l1_table_offset: " FONT_COLOR_RED "0x%lx, " COLOR_NONE
						"l1_size: " FONT_COLOR_RED "%d\n" COLOR_NONE
						"                ID: %s, NAME: %s\n"
						"                DATE: %s, SIZE: %lu\n",
						i, sn[i].l1_table_offset, sn[i].l1_size,
						sn[i].id_str, sn[i].name, date_buf, sn[i].vm_state_size);
			} else {
				printf("snapshot[%d]:	"
						"l1_table_offset: 0x%lx, l1_size: %d\n"
						"                ID: %s, NAME: %s\n"
						"                DATE: %s, SIZE: %lu\n",
						i, sn[i].l1_table_offset, sn[i].l1_size,
						sn[i].id_str, sn[i].name, date_buf, sn[i].vm_state_size);
			}
		} else {
			if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色
				printf(FONT_COLOR_GREEN "snapshot[%d]:	" COLOR_NONE
						"l1_table_offset: " FONT_COLOR_GREEN "0x%lx, " COLOR_NONE
						"l1_size: " FONT_COLOR_GREEN "%d\n" COLOR_NONE
						"                ID: %s, NAME: %s\n"
						"                DATE: %s, SIZE: %lu\n",
						i, sn[i].l1_table_offset, sn[i].l1_size,
						sn[i].id_str, sn[i].name, date_buf, sn[i].vm_state_size);
			} else {
				printf("snapshot[%d]:	"
						"l1_table_offset: 0x%lx, l1_size: %d\n"
						"                ID: %s, NAME: %s\n"
						"                DATE: %s, SIZE: %lu\n",
						i, sn[i].l1_table_offset, sn[i].l1_size,
						sn[i].id_str, sn[i].name, date_buf, sn[i].vm_state_size);
			}
		}
		if (i != nb_snapshots - 1) {
			printf("\n");
		}

		//find snapshot
		if (find) {
			char* pos = strstr(q2s->snapshot, "snapshot[");
			if (pos) {
				int index = -1;
				sscanf(pos, "snapshot[%d]", &index);
				if (index == i) {
					ret = i;
					continue;
				}
			}
			if (!sn[i].id_str || !sn[i].name) {
				continue;
			}
			//!strcmp(sn[i].id_str, q2s->snapshot)
			if (atoi(sn[i].id_str) == atoi(q2s->snapshot)
				|| !strcmp(sn[i].name, q2s->snapshot)) {
				ret = i;
			}
		}
	}

	printf("----------------------------------------------------------------\n");

	return ret;
}

static int qcow2_info_inactive_snapshot(qcow2_state* q2s)
{
	int i, ret = 0;
	QCowSnapshot *sn = NULL;
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	const uint64_t snapshots_offset = header->snapshots_offset;

	if (snapshots_offset == 0 || nb_snapshots == 0) {
		goto out;
	}
	if (snapshots_offset > q2s->seek_end) {
		ret = -EFBIG;
		printf("Line: %d | snapshots_offset[0x%lx] > seek_end[0x%lx]\n",
				__LINE__, snapshots_offset, q2s->seek_end);
		goto out;
	}

	ret = qcow2_list_and_find_snapshot(q2s, &sn, 0);
	if (ret < 0) {
		printf("Line: %d | list and find inactive snapshot failed: %d\n",
				__LINE__, ret);
		goto out;
	}
	printf("\n================================================================\n\n");

	for (i = 0; i < nb_snapshots; i++) {
		if (sn[i].id_str) {
			free(sn[i].id_str);
		}
		if (sn[i].name) {
			free(sn[i].name);
		}
	}
	free(sn);

out:
	return ret;
}

//检测qcow2镜像inactive快照的L1表和L2表簇对齐情况
static int qcow2_inactive_snapshot(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	const uint64_t snapshots_offset = header->snapshots_offset;
	QCowSnapshot *sn = NULL;
	int i, ret = 0, print = 1, found = 0;
	int l1_unaligned, l1_invalid, l1_unused, l1_used;
	int l2_unaligned, l2_invalid, l2_unused, l2_used, l2_compressed;

	if (snapshots_offset == 0 || nb_snapshots == 0) {
		goto out;
	}
	if (snapshots_offset > q2s->seek_end) {
		ret = -EFBIG;
		printf("Line: %d | snapshots_offset[0x%lx] > seek_end[0x%lx]\n",
				__LINE__, snapshots_offset, q2s->seek_end);
		goto out;
	}

	if (!strcmp(q2s->snapshot, "active")) {
		print = 0;
	} else {
		print = (q2s->mode != M_CHECK) && (q2s->output != O_REFCOUNT);
	}

	ret = qcow2_list_and_find_snapshot(q2s, &sn, 0);
	if (ret < 0) {
		goto out;
	}

	for (i = 0; i < nb_snapshots; i++) {
		int tmp_l1_invalid = q2s->l1_invalid;
		int tmp_l1_unaligned = q2s->l1_unaligned;
		int tmp_l1_unused = q2s->l1_unused;
		int tmp_l1_used = q2s->l1_used;
		int tmp_l2_invalid = q2s->l2_invalid;
		int tmp_l2_unaligned = q2s->l2_unaligned;
		int tmp_l2_unused = q2s->l2_unused;
		int tmp_l2_used = q2s->l2_used;
		int tmp_l2_compressed = q2s->l2_compressed;

		if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色
			printf("\n" FONT_COLOR_GREEN "snapshot[%d]:	" COLOR_NONE
					"ID: %s, NAME: %s\n",
					i, sn[i].id_str, sn[i].name);
		} else {
			printf("\nsnapshot[%d]:	ID: %s, NAME: %s\n",
					i, sn[i].id_str, sn[i].name);
		}

		found = 0;
		//!strcmp(sn[i].id_str, q2s->snapshot)
		if ((sn[i].id_str && atoi(sn[i].id_str) == atoi(q2s->snapshot))
			|| (sn[i].name && !strcmp(sn[i].name, q2s->snapshot))
			|| !strcmp(q2s->snapshot, "all")
			|| !strcmp(q2s->snapshot, "inactive")) {
			found = 1;
		}

		ret = qcow2_validate_table_offset(q2s, sn[i].l1_table_offset,
									sn[i].l1_size, sizeof(uint64_t));
		if (ret < 0) {
			ret = 0;
			printf("Line: %d | snapshot[%d]: Invalid L1 table offset: 0x%lx\n",
					__LINE__, i, sn[i].l1_table_offset);
			continue;
		}

		ret = qcow2_l1_table(q2s, sn[i].l1_table_offset, sn[i].l1_size, print && found);
		if (ret < 0) {
			goto out;
		}

		l1_unaligned = q2s->l1_unaligned - tmp_l1_unaligned;
		l1_invalid = q2s->l1_invalid - tmp_l1_invalid;
		l1_unused = q2s->l1_unused - tmp_l1_unused;
		l1_used = q2s->l1_used - tmp_l1_used;
		l2_unaligned = q2s->l2_unaligned - tmp_l2_unaligned;
		l2_invalid = q2s->l2_invalid - tmp_l2_invalid;
		l2_unused = q2s->l2_unused - tmp_l2_unused;
		l2_used = q2s->l2_used - tmp_l2_used;
		l2_compressed = q2s->l2_compressed - tmp_l2_compressed;

		if (!qcow2_is_snapshot_corrupt(l1_unaligned, l1_invalid, l2_unaligned, l2_invalid)) {
			//sscanf(sn[i].id_str, "%d", &q2s->snapshot_id);
			if (sn[i].id_str) {
				//获取最后一个完好快照的id
				q2s->snapshot_id = strtoul(sn[i].id_str, NULL, 10);
			}
		} else if (q2s->corrupt_id == 0) {//获取第一个corrupt的快照
			if (sn[i].id_str) {
				q2s->corrupt_id = strtoul(sn[i].id_str, NULL, 10);
			}
		}

		qcow2_l1_result(q2s,  l1_unaligned, l1_invalid, l1_unused, l1_used,
				l2_unaligned, l2_invalid, l2_unused, l2_used, l2_compressed);

		if (i != nb_snapshots - 1) {
			printf("\n----------------------------------------------------------------\n");
		}
	}

	printf("\n================================================================\n\n");

	for (i = 0; i < nb_snapshots; i++) {
		if (sn[i].id_str) {
			free(sn[i].id_str);
		}
		if (sn[i].name) {
			free(sn[i].name);
		}
	}
	free(sn);

out:
	return ret;
}

//检测qcow2镜像active快照和inactive快照元数据簇对齐情况
static int qcow2_snapshots(qcow2_state* q2s)
{
	int ret = 0;

	ret = qcow2_active_snapshot(q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_active_snapshot failed\n", __LINE__);
		return ret;
	}

	ret = qcow2_inactive_snapshot(q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_inactive_snapshot failed\n", __LINE__);
		return ret;
	}

	return 0;
}

//用于active snapshot的L1/L2表损坏, 而inactive snapshot L1/L2表完好时, 快照回滚
//实现原理: 交换active snapshot与最新的完好的inactive snapshot的l1_table_offset和l1_size
//使用该功能后, 接着使用删除指定快照功能 & 修复引用计数泄漏功能, 最后qemu-img打current快照
//eg:
//1.	qcow2-dump -A 1 filename
//		qcow2-dump -D 1 filename
//2.	qcow2-dump -D all filename
//qcow2-dump -R all filename
//qemu-img snapshot -c current filename
static int qcow2_revert_snapshot(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	int i, ret = 0, found = -1;
	QCowSnapshot *sn = NULL;
	uint8_t data[12];

	if (nb_snapshots == 0) {
		printf("Line: %d | File: %s has no snapshot\n", __LINE__, q2s->filename);
		goto out;
	}

	found = qcow2_list_and_find_snapshot(q2s, &sn, 1);
	if (found >= 0) {//参考qemu => qcow2_grow_l1_table
		//cpu_to_be32w((uint32_t*)data, sn[found].l1_size);
		//stq_be_p(data + 4, sn[found].l1_table_offset);
		*(uint32_t*)data = cpu_to_be32(sn[found].l1_size);
		*(uint64_t*)(data + 4) = cpu_to_be64(sn[found].l1_table_offset);
		ret = pwrite(q2s->fd, &data, sizeof(data), offsetof(QCowHeader, l1_size));
		if (ret < 0) {
			ret = -errno;
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_RED "\nApply snapshot: %s failed: %d\n" COLOR_NONE,
						q2s->snapshot, ret);
			} else {
				printf("\nApply snapshot: %s failed: %d\n", q2s->snapshot, ret);
			}
		} else {
			fsync(q2s->fd);

			//交换header->l1_size与sn[found].l1_size 和
			//header->l1_table_offset与sn[found].l1_table_offset
			sn[found].l1_size = header->l1_size;
			sn[found].l1_table_offset = header->l1_table_offset;
			ret = qcow2_write_snapshots(q2s, sn);
			if (ret < 0) {
				printf("Line: %d | qcow2_write_snapshots failed: %d\n", __LINE__, ret);
			}
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_GREEN "\nApply snapshot: %s success\n" COLOR_NONE,
						q2s->snapshot);
			} else {
				printf("\nApply snapshot: %s success\n", q2s->snapshot);
			}
		}
	} else {
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nNot find snapshot: %s\n" COLOR_NONE,
					q2s->snapshot);
		} else {
			printf("\nNot find snapshot: %s\n", q2s->snapshot);
		}
	}

	printf("\n================================================================\n");

	for (i = 0; i < nb_snapshots; i++) {
		if (sn[i].id_str) {
			free(sn[i].id_str);
		}
		if (sn[i].name) {
			free(sn[i].name);
		}
	}
	free(sn);

out:
	return ret;
}

//删除快照头中指定的sn结构, 调用后最好进行引用计数泄漏修复
//主要用于修复镜像快照损坏: 删除指定快照
//使用该功能后, 接着使用修复引用计数泄漏功能
static int qcow2_delete_snapshot(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	int i, ret = 0, found = -1;
	QCowSnapshot *sn = NULL;
	uint8_t data[12];

	if (nb_snapshots == 0) {
		printf("Line: %d | File: %s has no snapshot\n", __LINE__, q2s->filename);
		goto out;
	}

	found = qcow2_list_and_find_snapshot(q2s, &sn, 1);
	if (found >= 0) {
		if (sn[found].id_str) {
			free(sn[found].id_str);		//释放被删除sn对应的id/name堆内存
			sn[found].id_str = NULL;
		}
		if (sn[found].name) {
			free(sn[found].name);
			sn[found].name = NULL;
		}

		memmove(&sn[found], &sn[found+1], (nb_snapshots-found-1) * sizeof(*sn));
		memset(&sn[nb_snapshots-1], 0, sizeof(*sn));	//最后一个sn清0
		--header->nb_snapshots;			//快照个数减1

		ret = qcow2_write_snapshots(q2s, sn);
		if (ret < 0) {
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_RED "\nDelete snapshot: %s failed: %d\n" COLOR_NONE,
						q2s->snapshot, ret);
			} else {
				printf("\nDelete snapshot: %s failed: %d\n", q2s->snapshot, ret);
			}
		} else {
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_GREEN "\nDelete snapshot: %s success\n" COLOR_NONE,
						q2s->snapshot);
			} else {
				printf("\nDelete snapshot: %s success\n", q2s->snapshot);
			}
		}
	} else if (strcmp(q2s->snapshot, "all")) {
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nNot find snapshot: %s\n" COLOR_NONE,
					q2s->snapshot);
		} else {
			printf("\nNot find snapshot: %s\n", q2s->snapshot);
		}
	} else {
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nDelete %s snapshot\n" COLOR_NONE,
					q2s->snapshot);
		} else {
			printf("\nDelete %s snapshot\n", q2s->snapshot);
		}
	}

	if ((found >= 0 && !ret) || !strcmp(q2s->snapshot, "all")) {
		//修改qcow2 header中的快照个数
		if (!header->nb_snapshots || !strcmp(q2s->snapshot, "all")) {
			*(uint32_t*)data = cpu_to_be32(0);
			*(uint64_t*)(data + 4) = cpu_to_be64(0);
		} else {
			*(uint32_t*)data = cpu_to_be32(header->nb_snapshots);
			*(uint64_t*)(data + 4) = cpu_to_be64(header->snapshots_offset);
		}
		ret = pwrite(q2s->fd, &data, sizeof(data), offsetof(QCowHeader, nb_snapshots));
		if (ret < 0) {
			ret = -errno;
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_RED "\nModify nb_snapshots failed: %d\n" COLOR_NONE, ret);
			} else {
				printf("\nModify nb_snapshots failed: %d\n", ret);
			}
		} else {
			ret = 0;
			fsync(q2s->fd);
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_GREEN "\nModify nb_snapshots success\n" COLOR_NONE);
			} else {
				printf("\nModify nb_snapshots success\n");
			}
		}
	}

	printf("\n================================================================\n");

	for (i = 0; i < nb_snapshots; i++) {
		if (sn[i].id_str) {
			free(sn[i].id_str);
		}
		if (sn[i].name) {
			free(sn[i].name);
		}
	}
	free(sn);

out:
	return ret;
}

//删除快照头中指定之外的sn结构, 调用后最好进行引用计数泄漏修复
//主要用于修复镜像快照头损坏
//使用该功能后, 接着使用修复引用计数泄漏功能
//eg:
//qcow2-dump -E 0 filename: 表示删除快照头(所有快照)
//qcow2-dump -R all filename

//快照头损坏实例: qcow2-dump -E 1 filename
//Inactive Snapshot:
//----------------------------------------------------------------
//snapshot[0]:	l1_table_offset: 0x0, l1_size: 0, id: , name:
//snapshot[1]:	l1_table_offset: 0x0, l1_size: 0, id: , name:
//snapshot[2]:	l1_table_offset: 0x0, l1_size: 0, id: , name:
//snapshot[3]:	l1_table_offset: 0xffcd10000, l1_size: 160, id: 1, name: a7faba02b
//----------------------------------------------------------------
static int qcow2_exclude_snapshot(qcow2_state* q2s)
{
	QCowHeader *header = &q2s->header;
	const uint32_t nb_snapshots = header->nb_snapshots;
	int i, ret = 0, found = -1;
	QCowSnapshot *sn = NULL;
	uint8_t data[12];

	if (nb_snapshots == 0) {
		printf("Line: %d | File: %s has no snapshot\n", __LINE__, q2s->filename);
		goto out;
	}

	found = qcow2_list_and_find_snapshot(q2s, &sn, 1);
	if (found >= 0 && nb_snapshots > 1) {
		for (i = 0; i < nb_snapshots; i++) {
			if (i != found) {
				if (sn[i].id_str) {
					free(sn[i].id_str);			//释放被删除sn对应的id/name堆内存
					sn[i].id_str = NULL;
				}
				if (sn[i].name) {
					free(sn[i].name);
					sn[i].name = NULL;
				}
			}
		}
		if (found != 0) {
			sn[0] = sn[found];
		}
		memset(&sn[1], 0, (nb_snapshots-1) * sizeof(*sn));	//sn[0]之外的所有sn清0
		header->nb_snapshots = 1;

		ret = qcow2_write_snapshots(q2s, sn);
		if (ret < 0) {
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_RED "\nExclude snapshot: %s failed: %d\n" COLOR_NONE,
						q2s->snapshot, ret);
			} else {
				printf("\nExclude snapshot: %s failed: %d\n", q2s->snapshot, ret);
			}
		} else {
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_GREEN "\nExclude snapshot: %s success\n" COLOR_NONE,
						q2s->snapshot);
			} else {
				printf("\nExclude snapshot: %s success\n", q2s->snapshot);
			}
		}
	} else if (strcmp(q2s->snapshot, "0") && !(found >= 0 && nb_snapshots == 1)) {
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nNot find snapshot: %s\n" COLOR_NONE,
					q2s->snapshot);
		} else {
			printf("\nNot find snapshot: %s\n", q2s->snapshot);
		}
	} else {
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nNot Operate: Exclude snapshot: %s\n" COLOR_NONE,
					q2s->snapshot);
		} else {
			printf("\nNot Operate: Exclude snapshot: %s\n", q2s->snapshot);
		}
	}

	if ((found >= 0 && nb_snapshots > 1 && !ret) || !strcmp(q2s->snapshot, "0")) {
		if (!strcmp(q2s->snapshot, "0")) {
			*(uint32_t*)data = cpu_to_be32(0);
			*(uint64_t*)(data + 4) = cpu_to_be64(0);
		} else if (found >= 0 && nb_snapshots > 1) {
			//修改qcow2 header中的快照个数
			*(uint32_t*)data = cpu_to_be32(1);
			*(uint64_t*)(data + 4) = cpu_to_be64(header->snapshots_offset);
		}
		ret = pwrite(q2s->fd, &data, sizeof(data), offsetof(QCowHeader, nb_snapshots));
		if (ret < 0) {
			ret = -errno;
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_RED "\nModify nb_snapshots failed: %d\n" COLOR_NONE, ret);
			} else {
				printf("\nModify nb_snapshots failed: %d\n", ret);
			}
		} else {
			ret = 0;
			fsync(q2s->fd);
			if (is_color_display(q2s)) {//在终端输出彩色
				printf(FONT_COLOR_GREEN "\nModify nb_snapshots success\n" COLOR_NONE);
			} else {
				printf("\nModify nb_snapshots success\n");
			}
		}
	}

	printf("\n================================================================\n");

	for (i = 0; i < nb_snapshots; i++) {
		if (sn[i].id_str) {
			free(sn[i].id_str);
		}
		if (sn[i].name) {
			free(sn[i].name);
		}
	}
	free(sn);

out:
	return ret;
}

//修改指定偏移的指定字节宽度(1/2/4/8字节, 默认值：8)的值(主要用于修改损坏的索引表项)
//eg:
//l1 table[  69], l2 offset: 0xac3410000
//	l2 table[ 672], data offset: 0xb920d0000 | vdisk offset: 0x8a2a00000
//	l2 table[ 673], data offset: 0xb920e0000 | vdisk offset: 0x8a2a10000
//	l2 table[ 674], data offset: 0xe00038000 [unaligned] | vdisk offset: 0x8a2a20000
//	l2 table[ 675], data offset: 0x87888ab1c4c600 [unaligned] | vdisk offset: 0x8a2a30000
//	l2 table[ 676], data offset: 0xd2000b92110000 [invalid] | vdisk offset: 0x8a2a40000
//	l2 table[ 677], data offset: 0xb92120000 | vdisk offset: 0x8a2a50000
//	l2 table[ 678], data offset: 0xb92130000 | vdisk offset: 0x8a2a60000
//损坏的L2索引表项是连续的，镜像可以完全修复，如下: (偏移: 0xac3410000 + 674 * 8 = 0xac3411510)
//	data offset: 0xe00038000       修改为 0xb920f0000	(-O 0xac3411510 -V 0xb920f0000)
//	data offset: 0x87888ab1c4c600  修改为 0xb92100000	(-O 0xac3411518 -V 0xb92100000)
//	data offset: 0xd2000b92110000  修改为 0xb92110000	(-O 0xac3411520 -V 0xb92110000)
static int qcow2_edit_modify(qcow2_state* q2s)
{
	int ret = 0;
	uint8_t  data[8];
	uint8_t  v8  = 0;
	uint16_t v16 = 0;
	uint32_t v32 = 0;
	uint64_t v64 = 0;
	uint64_t value = 0;

	if (q2s->dst_offset == INVALID || q2s->write == INVALID) {
		printf("Line: %d | offset: %lx or value: %lx invalid\n",
				__LINE__, q2s->dst_offset, q2s->write);
		return -EINVAL;
	}
	if (q2s->dst_offset >= q2s->seek_end) {//超出了镜像的范围
		printf("Line: %d | offset: %lx out of range: %lx\n",
				__LINE__, q2s->dst_offset, q2s->seek_end);
		return -EFBIG;
	}

	//1, 2, 4, 8字节宽度
	if (q2s->width != 1 && q2s->width != 2
		&& q2s->width != 4 && q2s->width != 8) {
		q2s->width = 8;
	}
	memset(&data, 0, q2s->width);
	ret = pread(q2s->fd, &data, q2s->width, q2s->dst_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | pread failed: %d\n", __LINE__, ret);
		return ret;
	}

	switch (q2s->width) {
	case 1:
		v8 = *(uint8_t*)data;
		value = v8;
		break;
	case 2:
		v16 = *(uint16_t*)data;
		be16_to_cpus(&v16);
		value = v16;
		break;
	case 4:
		v32 = *(uint32_t*)data;
		be32_to_cpus(&v32);
		value = v32;
		break;
	case 8:
	default:
		v64 = *(uint64_t*)data;
		be64_to_cpus(&v64);
		value = v64;
		break;
	}
	if (is_color_display(q2s)) {//在终端输出彩色
		printf(FONT_COLOR_GREEN "Show offset: 0x%lx, value: 0x%lx\n" COLOR_NONE,
				q2s->dst_offset, value);
	} else {
		printf("Show offset: 0x%lx, value: 0x%lx\n", q2s->dst_offset, value);
	}

	memset(&data, 0, q2s->width);
	if (q2s->write != INVALID) {
		switch (q2s->width) {
		case 1:
			*(uint8_t*)data = q2s->write;
			break;
		case 2:
			*(uint16_t*)data = cpu_to_be16(q2s->write);
			break;
		case 4:
			*(uint32_t*)data = cpu_to_be32(q2s->write);
			break;
		case 8:
		default:
			*(uint64_t*)data = cpu_to_be64(q2s->write);
			break;
		}

		ret = pwrite(q2s->fd, data, q2s->width, q2s->dst_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | pwrite failed: %d\n", __LINE__, ret);
			return ret;
		}
		fsync(q2s->fd);
		if (is_color_display(q2s)) {//在终端输出彩色
			printf(FONT_COLOR_YELLOW "\nStore offset: 0x%lx, value: 0x%lx\n" COLOR_NONE,
					q2s->dst_offset, q2s->write);
		} else {
			printf("\nStore offset: 0x%lx, value: 0x%lx\n", q2s->dst_offset, q2s->write);
		}
	}

	printf("\n================================================================\n");

	return 0;
}

//有快照的情况：当active的索引表损坏, 而inactive的索引表没有损坏, 用于拷贝修复
static int qcow2_copy_modify(qcow2_state* q2s)
{
	int ret = 0;
	int src_fd = -1;
	uint8_t* buff = NULL;

	if (q2s->dst_offset == INVALID || q2s->src_offset == INVALID) {
		printf("Line: %d | dst_offset: %lx or src_offset: %lx invalid\n",
				__LINE__, q2s->dst_offset, q2s->src_offset);
		return -EINVAL;
	}
	if (q2s->width > q2s->cluster_size) {
		printf("Line: %d | width: %x > cluster_size: %lx\n",
				__LINE__, q2s->width, q2s->cluster_size);
		return -EFBIG;
	}

	buff = malloc(q2s->width);
	if (!buff) {
		printf("Line: %d | malloc failed\n", __LINE__);
		return -ENOMEM;
	}
	memset(buff, 0, q2s->width);

	if (q2s->source_file) {
		src_fd = open(q2s->source_file, O_RDONLY);
		if (src_fd < 0) {
			ret = -errno;
			printf("Line: %d | open %s failed: %d\n", __LINE__, q2s->source_file, ret);
			goto out;
		}
	} else {
		src_fd = q2s->fd;
	}

	ret = pread(src_fd, buff, q2s->width, q2s->src_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	ret = pwrite(q2s->fd, buff, q2s->width, q2s->dst_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | pwrite failed: %d\n", __LINE__, ret);
		goto out;
	}
	ret = 0;
	fsync(q2s->fd);

	if (q2s->source_file && src_fd >= 0) {
		close(src_fd);
	}

	if (is_color_display(q2s)) {//在终端输出彩色
		printf(FONT_COLOR_GREEN "\nCOPY  data from 0x%lx to 0x%lx "
				"[length: %d] success\n" COLOR_NONE,
				q2s->src_offset, q2s->dst_offset, q2s->width);
	} else {
		printf("\nCOPY  data from 0x%lx to 0x%lx [length: %d] success\n",
				q2s->src_offset, q2s->dst_offset, q2s->width);
	}
	printf("\n================================================================\n");

out:
	free(buff);
	return ret;
}

static uint64_t qcow2_get_refcount_ro0(const void *refcount_array, uint64_t index)
{
	return (((const uint8_t *)refcount_array)[index / 8] >> (index % 8)) & 0x1;
}

static void qcow2_set_refcount_ro0(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 1));
	((uint8_t *)refcount_array)[index / 8] &= ~(0x1 << (index % 8));
	((uint8_t *)refcount_array)[index / 8] |= value << (index % 8);
}

static uint64_t qcow2_get_refcount_ro1(const void *refcount_array, uint64_t index)
{
	return (((const uint8_t *)refcount_array)[index / 4] >> (2 * (index % 4))) & 0x3;
}

static void qcow2_set_refcount_ro1(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 2));
	((uint8_t *)refcount_array)[index / 4] &= ~(0x3 << (2 * (index % 4)));
	((uint8_t *)refcount_array)[index / 4] |= value << (2 * (index % 4));
}

static uint64_t qcow2_get_refcount_ro2(const void *refcount_array, uint64_t index)
{
	return (((const uint8_t *)refcount_array)[index / 2] >> (4 * (index % 2))) & 0xf;
}

static void qcow2_set_refcount_ro2(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 4));
	((uint8_t *)refcount_array)[index / 2] &= ~(0xf << (4 * (index % 2)));
	((uint8_t *)refcount_array)[index / 2] |= value << (4 * (index % 2));
}

static uint64_t qcow2_get_refcount_ro3(const void *refcount_array, uint64_t index)
{
	return ((const uint8_t *)refcount_array)[index];
}

static void qcow2_set_refcount_ro3(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 8));
	((uint8_t *)refcount_array)[index] = value;
}

static uint64_t qcow2_get_refcount_ro4(const void *refcount_array, uint64_t index)
{
	return be16_to_cpu(((const uint16_t *)refcount_array)[index]);
}

static void qcow2_set_refcount_ro4(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 16));
	((uint16_t *)refcount_array)[index] = cpu_to_be16(value);
}

static uint64_t qcow2_get_refcount_ro5(const void *refcount_array, uint64_t index)
{
	return be32_to_cpu(((const uint32_t *)refcount_array)[index]);
}

static void qcow2_set_refcount_ro5(void *refcount_array, uint64_t index, uint64_t value)
{
	assert(!(value >> 32));
	((uint32_t *)refcount_array)[index] = cpu_to_be32(value);
}

static uint64_t qcow2_get_refcount_ro6(const void *refcount_array, uint64_t index)
{
	return be64_to_cpu(((const uint64_t *)refcount_array)[index]);
}

static void qcow2_set_refcount_ro6(void *refcount_array, uint64_t index, uint64_t value)
{
	((uint64_t *)refcount_array)[index] = cpu_to_be64(value);
}

static uint64_t qcow2_refcount_array_byte_size(qcow2_state* q2s, uint64_t entries)
{
	/* This assertion holds because there is no way we can address more than
	 * 2^(64 - 9) clusters at once (with cluster size 512 = 2^9, and because
	 * offsets have to be representable in bytes); due to every cluster
	 * corresponding to one refcount entry, we are well below that limit */
	assert(entries < (UINT64_C(1) << (64 - 9)));

	/* Thanks to the assertion this will not overflow, because
	 *  q2s->header.refcount_order < 7.
	 * (note: x << q2s->header.refcount_order == x * q2s->refcount_bits) */
	return DIV_ROUND_UP(entries << q2s->header.refcount_order, 8);
}

//计算qcow2镜像所有簇的引用计数
static void qcow2_calculate_refcounts(qcow2_state* q2s, uint64_t offset, uint64_t size)
{
	QCowHeader *header = &q2s->header;
	uint64_t start, last, cluster_offset, k, refcount;

    if (0 == size) {
        return;
    }
	if (!qcow2_is_check_all(q2s)) {//只有检查所有元数据时, 才检查引用计数
		return;
	}
	if (qcow2_out_of_range(q2s, offset)) {
		printf("Line: %d | out_of_range, offset: 0x%lx, seek_end: 0x%lx\n",
				__LINE__, offset, q2s->seek_end);
		exit(-EFBIG);	/* File too large */
	}
	if (qcow2_out_of_range(q2s, offset+size)) {
		printf("Line: %d | out_of_range, offset+size: 0x%lx, seek_end: 0x%lx\n",
				__LINE__, offset+size, q2s->seek_end);
		exit(-EFBIG);	/* File too large */
	}

	start = qcow2_start_of_cluster(q2s, offset);
	last = qcow2_start_of_cluster(q2s, offset + size - 1);
	for(cluster_offset = start; cluster_offset <= last;
		cluster_offset += q2s->cluster_size) {
		k = cluster_offset >> header->cluster_bits;

		if (k >= q2s->nb_clusters) {
			uint64_t old_refcount_array_size = qcow2_refcount_array_byte_size(q2s, q2s->nb_clusters);
			uint64_t nb_clusters = align_offset(k + SECTOR_ALIGN, SECTOR_ALIGN);  //!预留一部分空间
			uint64_t new_refcount_array_size = qcow2_refcount_array_byte_size(q2s, nb_clusters);
			void *new_refcount_table = realloc(q2s->refcount_table, new_refcount_array_size);
			if (!new_refcount_table) {
				printf("Line: %d | realloc failed\n", __LINE__);
				exit(-ENOMEM);
			}
			memset((char *)new_refcount_table + old_refcount_array_size, 0,
					new_refcount_array_size - old_refcount_array_size);
			q2s->nb_clusters = nb_clusters;
			q2s->refcount_table = new_refcount_table;
		}

		refcount = q2s->get_refcount(q2s->refcount_table, k);
		if (refcount == q2s->refcount_max) {
			printf("ERROR: overflow cluster offset=0x%lx, refcount_table[%ld], refcount: %ld\n",
					cluster_offset, k, refcount);
			++q2s->corruptions;
		}
		q2s->set_refcount(q2s->refcount_table, k, refcount + 1);
	}
}

static int qcow2_refcount_entry_miss(qcow2_state* q2s, int i,
					int64_t refcount_block_offset, int print)
{
	int n, first = 1;
	uint64_t offset;
	uint64_t reference;
	int corruptions = 0, unused = 0, entry_miss = 0;
	QCowHeader *header = &q2s->header;
	uint64_t refcount_table_index = i * (q2s->cluster_size >> (header->refcount_order - 3));
	char entry_buff[64] = {0};

	sprintf(entry_buff, "refcount table[%4d], offset: 0x%lx\n", i, refcount_block_offset);
	for(n = 0; n < q2s->refcount_block_size; n++) {
		if (refcount_table_index + n >= q2s->nb_clusters) {//超出范围
			break;
		}
		//refcount_table: 镜像内的全部引用计数
		reference = q2s->get_refcount(q2s->refcount_table, refcount_table_index + n);
		if (reference > 0) {
			entry_miss = 1;
			offset = (refcount_table_index + n) * q2s->cluster_size;
			qcow2_cluster_reuse_record_offset(q2s, offset);
			if (print) {//-m error/dump时输出
				qcow2_entry_print(entry_buff, &first);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf("	refcount block[%5d], refcount: 0 | reference: %ld "
								FONT_COLOR_RED "[error][rebuild] " COLOR_NONE
								"| cluster offset: 0x%lx\n",
								n, reference, offset);
				} else {
					printf("	refcount block[%5d], refcount: 0 | reference: %ld "
								"[error][rebuild] | cluster offset: 0x%lx\n",
								n, reference, offset);
				}
			}
			++corruptions;
			++q2s->corruptions;
			++q2s->rebuild;		//统计refcount table miss和refcount block error中的refcount = 0
		} else {
			if (print && !first) {
				printf("	refcount block[%5d], refcount: 0 | reference: %ld\n",
							n, reference);
			}
			++unused;
		}
	}
	//refcount_block的引用计数在自己上, 参考qemu => in_same_refcount_block
	//因此refcount_block的引用计数漏算, 需要+1：
	//Refcount: error: 32767, leak: 0, unused: 1, used: 0
	if (entry_miss) {
		--unused;
		++corruptions;
		++q2s->corruptions;	//统计refcount block error
		++q2s->entry_miss;	//统计refcount table miss
		++q2s->rebuild;		//统计refcount table miss和refcount block error中的refcount = 0
	}
	if (print && !first) {
		if (entry_miss) {
			printf("\n	Refcount: error: %d, leak: %d, unused: %d, "
						"used: %d, miss index: %d",
						corruptions, 0, unused, 0, i);
		} else {
			printf("\n	Refcount: error: %d, leak: %d, unused: %d, used: %d",
						corruptions, 0, unused, 0);
		}
		printf("\n	--------------------------------------------------------\n\n");
	}

	return entry_miss;
}

//Allocates clusters using an in-memory refcount table (IMRT) in contrast to
//the on-disk refcount structures.
static uint64_t qcow2_alloc_cluster_imrt(qcow2_state* q2s)
{
	int i = 0;
	uint64_t offset = 0;
	uint64_t refcount = 0;

	for (i = 0; i < q2s->nb_clusters; i++) {
		refcount = q2s->get_refcount(q2s->refcount_table, i);
		if (refcount == 0) {
			offset = i * q2s->cluster_size;
			break;
		}
	}

	return offset;
}

//重建损坏的refcount table, 重新分配refcount block
static int qcow2_rebuild_refcount_structure(qcow2_state* q2s,
						uint64_t *refcount_table, int index)
{
	int ret = 0;
	int rebuild = 0;
	uint64_t offset = 0;
	uint64_t *refcount_block = NULL;

#ifdef ALLOC_CLUSTER_IMRT
	//Allocates clusters using an in-memory refcount table (IMRT) in contrast to
	//the on-disk refcount structures.
	offset = qcow2_alloc_cluster_imrt(q2s);
	if (offset == 0) {
		return -ENOSPC;
	}
#else
	//从文件末尾处给需要重建的refcount block分配簇空间
	offset = qcow2_start_of_cluster(q2s, q2s->seek_end);	//对文件末尾偏移簇对齐
	rebuild = q2s->entry_miss + q2s->ref_unaligned + q2s->ref_invalid;
	offset += rebuild * q2s->cluster_size;	//保证是末尾后面的簇的偏移
#endif
	//修复引用计数时, 已经对重建refcount block的簇IMRT计数了
	//qcow2_calculate_refcounts(q2s, offset, q2s->cluster_size);

	assert(refcount_table && index < q2s->refcount_table_size);
	refcount_table[index] = cpu_to_be64(offset);	//注意字节顺

	refcount_block = malloc(q2s->cluster_size);
	if (!refcount_block) {
		ret = -ENOMEM;
		printf("Line: %d | malloc refcount_block failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(refcount_block, 0, q2s->cluster_size);

	ret = pwrite(q2s->fd, refcount_block, q2s->cluster_size, offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | rebuild refcount block[offset: 0x%lx] failed: %d\n",
				__LINE__, offset, ret);
		goto out;
	} else {
		ret = 0;
		fsync(q2s->fd);
	}

out:
	if (refcount_block) {
		free(refcount_block);
	}
	return ret;
}

//检测qcow2镜像引用计数
static int qcow2_refcount_block(qcow2_state* q2s, int index,
								int64_t refcount_block_offset,
								char* entry_buff, int print)
{
	int i, ret = 0, first = 1;
	uint64_t offset;
	uint64_t *refcount_block;
	uint64_t refcount, reference;
	QCowHeader *header = &q2s->header;
	int corruptions = 0, leaks = 0, unused = 0, used = 0;
	uint64_t refcount_table_index = index * (q2s->cluster_size >> (header->refcount_order - 3));

	refcount_block = malloc(q2s->cluster_size);
	if (!refcount_block) {
		ret = -ENOMEM;
		printf("Line: %d | malloc refcount_block failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(refcount_block, 0, q2s->cluster_size);

	ret = pread(q2s->fd, refcount_block, q2s->cluster_size, refcount_block_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | refcount_block pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	//对refcount block引用计数
	qcow2_calculate_refcounts(q2s, refcount_block_offset, q2s->cluster_size);

	for(i = 0; i < q2s->refcount_block_size; i++) {
		if (refcount_table_index + i >= q2s->nb_clusters) {//超出范围
			break;
		}
		offset = (refcount_table_index + i) * q2s->cluster_size;

		//refcount_block: 一簇范围内的引用计数
		refcount = q2s->get_refcount(refcount_block, i);
		//refcount_table: 镜像内的全部引用计数
		reference = q2s->get_refcount(q2s->refcount_table, refcount_table_index + i);
		if (refcount > reference) {
			++leaks;
			++q2s->leaks;
			if (print) {//-m error/dump时输出
				qcow2_entry_print(entry_buff, &first);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf("	refcount block[%5d], refcount: %ld | reference: %ld "
								FONT_COLOR_YELLOW "[leak] " COLOR_NONE
								"| cluster[%lu] offset: 0x%lx\n",
								i, refcount, reference,
								refcount_table_index + i, offset);
				} else {
					printf("	refcount block[%5d], refcount: %ld | reference: %ld "
								"[leak] | cluster[%lu] offset: 0x%lx\n",
								i, refcount, reference,
								refcount_table_index + i, offset);
				}
			}
			if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_leaks(q2s)
				&& !qcow2_rebuild_reused_active_cluster(q2s)) {
				q2s->set_refcount(refcount_block, i, reference);
				if (is_color_display(q2s)) {
					printf(FONT_COLOR_RED "Repairing " COLOR_NONE
							"cluster: %lu, refcount: %ld | reference: %ld "
							FONT_COLOR_YELLOW "[leak] " COLOR_NONE
							"| cluster offset: 0x%lx\n",
							refcount_table_index + i,
							refcount, reference, offset);
				} else {
					printf("Repairing cluster: %lu, refcount: %ld | reference: %ld "
							"[leak] | cluster offset: 0x%lx\n",
							refcount_table_index + i,
							refcount, reference, offset);
				}
			}
		} else if (refcount < reference) {
			++corruptions;
			++q2s->corruptions;
			if (refcount == 0) {//与qemu-img: compare_refcounts保持一致.
				++q2s->rebuild;
			}

			qcow2_cluster_reuse_record_offset(q2s, offset);
			if (print) {//-m error/dump时输出
				qcow2_entry_print(entry_buff, &first);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf("	refcount block[%5d], refcount: %ld | reference: %ld "
								FONT_COLOR_RED "[error]%s " COLOR_NONE
								"| cluster[%lu] offset: 0x%lx\n",
								i, refcount, reference,
								refcount == 0 ? "[rebuild]" : "",
								refcount_table_index + i, offset);
				} else {
					printf("	refcount block[%5d], refcount: %ld | reference: %ld "
								"[error]%s | cluster[%lu] offset: 0x%lx\n",
								i, refcount, reference,
								refcount == 0 ? "[rebuild]" : "",
								refcount_table_index + i, offset);
				}
			}
			if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_errors(q2s)
				&& !qcow2_rebuild_reused_active_cluster(q2s)) {
				q2s->set_refcount(refcount_block, i, reference);
				if (is_color_display(q2s)) {
					printf(FONT_COLOR_RED "Repairing " COLOR_NONE
							"cluster: %lu, refcount: %ld | reference: %ld "
							FONT_COLOR_RED "[error] " COLOR_NONE
							"| cluster offset: 0x%lx\n",
							refcount_table_index + i,
							refcount, reference, offset);
				} else {
					printf("Repairing cluster: %lu, refcount: %ld | reference: %ld "
							"[error] | cluster offset: 0x%lx\n",
							refcount_table_index + i,
							refcount, reference, offset);
				}
			}
		} else {
			if (refcount) {
				++used;
				++q2s->used;
			} else {
				++unused;
				++q2s->unused;
			}
			if (print && q2s->mode == M_DUMP) {
				qcow2_entry_print(entry_buff, &first);
				printf("	refcount block[%5d], refcount: %ld | reference: %ld\n",
							i, refcount, reference);
			}
		}
	}
	//重建重用簇时, 不修复引用计数泄漏/错误
	if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_mode(q2s)
		&& (leaks || corruptions) && !qcow2_rebuild_reused_active_cluster(q2s)) {
		ret = pwrite(q2s->fd, refcount_block, q2s->cluster_size, refcount_block_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | refcount_block pwrite failed: %d\n", __LINE__, ret);
		} else {
			ret = 0;
			fsync(q2s->fd);
		}
	}
	if (print && !first) {
		printf("\n	Refcount: error: %d, leak: %d, unused: %d, used: %d",
					corruptions, leaks, unused, used);
		printf("\n	--------------------------------------------------------\n\n");
	}

out:
	if (qcow2_is_check_oflag_copied(q2s)) {
		q2s->refcount_array[index] = refcount_block;
	} else {
		free(refcount_block);
	}

	return ret;
}

//输出检测qcow2镜像引用计数统计情况
static void qcow2_refcount_result(qcow2_state* q2s)
{
	if (is_color_display(q2s)) {	//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_CYAN "Result:\n" COLOR_NONE);		//输出青色

		if (q2s->entry_miss) {
			printf("Refcount Table:	unaligned: "
					FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"invalid: "
					FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"miss: "
					FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid,
					q2s->entry_miss,
					q2s->ref_unused, q2s->ref_used);
		} else {
			printf("Refcount Table:	unaligned: "
					FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"invalid: "
					FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid,
					q2s->ref_unused, q2s->ref_used);
		}

		if (qcow2_is_check_all(q2s)) {
			printf("Refcount:       error: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"leak: " FONT_COLOR_YELLOW "%ld, " COLOR_NONE
					"unused: %ld, used: %ld\n",
					q2s->corruptions, q2s->leaks,
					q2s->unused, q2s->used);
		}
	} else {
		printf("Result:\n");

		if (q2s->entry_miss) {
			printf("Refcount Table:	unaligned: %ld, invalid: %ld, "
					"miss: %ld, unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid,
					q2s->entry_miss,
					q2s->ref_unused, q2s->ref_used);
		} else {
			printf("Refcount Table:	unaligned: %ld, "
					"invalid: %ld, unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid,
					q2s->ref_unused, q2s->ref_used);
		}

		if (qcow2_is_check_all(q2s)) {
			printf("Refcount:       error: %ld, "
					"leak: %ld, unused: %ld, used: %ld\n",
					q2s->corruptions, q2s->leaks,
					q2s->unused, q2s->used);
		}
	}
}

//检测qcow2镜像引用计数表
static int qcow2_refcount_table(qcow2_state* q2s)
{
	int entry_rebuild = 0;
	int i, ret = 0, print = 0;
	int64_t refcount_block_offset;
	uint64_t *refcount_table = NULL;
	QCowHeader *header = &q2s->header;
	int cluster_bits = header->cluster_bits;
	const uint32_t refcount_table_size = q2s->refcount_table_size;
	uint64_t refcount_table_offset = header->refcount_table_offset;
	uint32_t refcount_table_size2 = refcount_table_size * sizeof(uint64_t);
	char entry_buff[64] = {0};

	if (q2s->output == O_REFCOUNT || q2s->output == O_ALL) {
		print = (q2s->mode != M_CHECK);
	}

	refcount_table = malloc(refcount_table_size2);
	if (!refcount_table) {
		ret = -ENOMEM;
		printf("Line: %d | malloc refcount_table failed: %d\n", __LINE__, ret);
		return ret;
	}
	memset(refcount_table, 0, refcount_table_size2);

	ret = pread(q2s->fd, refcount_table, refcount_table_size2, refcount_table_offset);
	if (ret < 0) {
		ret = -errno;
		printf("Line: %d | refcount_table pread failed: %d\n", __LINE__, ret);
		goto out;
	}

	//对refcount table引用计数
	qcow2_calculate_refcounts(q2s, refcount_table_offset, refcount_table_size2);

	if (is_color_display(q2s)) {//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_GREEN "Refcount Table:\n" COLOR_NONE);	//输出绿色
	} else {
		printf("Refcount Table:\n");
	}
	printf("----------------------------------------------------------------\n");

	printf("Refcount Table:	[offset: 0x%lx, len: %u]\n\n",
			refcount_table_offset, refcount_table_size);

	for(i = 0; i < refcount_table_size; i++) {
		be64_to_cpus(&refcount_table[i]);
		refcount_block_offset = refcount_table[i] & REFT_OFFSET_MASK;

		if (!refcount_block_offset) {//解决: 此处的引用计数检查/修复可能漏掉了.
			ret = qcow2_refcount_entry_miss(q2s, i, refcount_block_offset, print);
			if (ret) {
				if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_errors(q2s)) {
					++entry_rebuild;

					ret = qcow2_rebuild_refcount_structure(q2s, refcount_table, i);
					if (ret < 0) {
						printf("Line: %d | rebuild_refcount_structure failed: %d\n",
								__LINE__, ret);
						goto out;
					}
					--i;	//refcount block已经重建, 接下来修复引用计数错误
				}
			} else {
				++q2s->ref_unused;
			}
			continue;
		}
		if (offset_into_cluster(q2s->cluster_size, refcount_block_offset)) {
			++q2s->ref_unaligned;
			if (print) {//-m error/dump时输出
				printf("refcount table[%4d], offset: 0x%lx", i, refcount_block_offset);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf(FONT_COLOR_RED " [unaligned]\n\n" COLOR_NONE);
				} else {
					printf(" [unaligned]\n\n");
				}
			}

			if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_errors(q2s)) {
				++entry_rebuild;

				ret = qcow2_rebuild_refcount_structure(q2s, refcount_table, i);
				if (ret < 0) {
					printf("Line: %d | rebuild_refcount_structure failed: %d\n",
							__LINE__, ret);
					goto out;
				}
				--i;	//refcount block已经重建, 接下来修复引用计数错误
			}
		} else if (qcow2_out_of_range(q2s, refcount_block_offset)) {
			++q2s->ref_invalid;
			if (print) {//-m error/dump时输出
				printf("refcount table[%4d], offset: 0x%lx", i, refcount_block_offset);
				if (is_color_display(q2s)) {//-m error时输出彩色
					printf(FONT_COLOR_RED " [invalid]\n\n" COLOR_NONE);
				} else {
					printf(" [invalid]\n\n");
				}
			}

			if (!qcow2_L1L2_is_corrupt(q2s) && qcow2_is_repair_errors(q2s)) {
				++entry_rebuild;

				ret = qcow2_rebuild_refcount_structure(q2s, refcount_table, i);
				if (ret < 0) {
					printf("Line: %d | rebuild_refcount_structure failed: %d\n",
							__LINE__, ret);
					goto out;
				}
				--i;	//refcount block已经重建, 接下来修复引用计数错误
			}
		} else {
			++q2s->ref_used;
			if (qcow2_is_check_all(q2s)) {
				if (print) {//sprintf到buffer中, 作为参数传到qcow2_refcount_block.
					memset(entry_buff, 0, sizeof(entry_buff));
					sprintf(entry_buff, "refcount table[%4d], offset: 0x%lx\n",
							i, refcount_block_offset);
				}
				ret = qcow2_refcount_block(q2s, i, refcount_block_offset, entry_buff, print);
				if (ret < 0) {
					goto out;
				}
			}
		}
	}
	//重建丢失/损坏的refcount block
	if (entry_rebuild && qcow2_is_repair_errors(q2s)) {
		for(i = 0; i < refcount_table_size; i++) {
			refcount_table[i] = cpu_to_be64(refcount_table[i]);
		}
		ret = pwrite(q2s->fd, refcount_table, refcount_table_size2, refcount_table_offset);
		if (ret < 0) {
			ret = -errno;
			printf("Line: %d | refcount_table pwrite failed: %d\n", __LINE__, ret);
		} else {
			ret = 0;
			fsync(q2s->fd);
			q2s->ref_used -= entry_rebuild;	//重新计算used的refcount table项
		}
	}

	qcow2_refcount_result(q2s);

	printf("\n================================================================\n\n");

out:
	if (refcount_table) {
		free(refcount_table);
	}

	return ret;
}

//输出镜像分配模式
static void qcow2_preallocation_mode(qcow2_state* q2s)
{
	if (q2s->l1_unused || q2s->l2_not_prealloc || q2s->l2_unused > q2s->l2_size) {
		if (is_color_display(q2s)) {
			printf("preallocation:  " FONT_COLOR_GREEN "off\n" COLOR_NONE);
		} else {
			printf("preallocation:  off\n");
		}
	} else {
		if (q2s->disk_size/ONE_M < q2s->header.size/ONE_M) {
			if (is_color_display(q2s)) {
				printf("preallocation:  " FONT_COLOR_GREEN "metadata\n" COLOR_NONE);
			} else {
				printf("preallocation:  metadata\n");
			}
		} else {
			if (is_color_display(q2s)) {
				printf("preallocation:  " FONT_COLOR_GREEN "full\n" COLOR_NONE);
			} else {
				printf("preallocation:  full\n");
			}
		}
	}
}

//输出镜像元数据统计情况
static void qcow2_metadata_stats(qcow2_state* q2s)
{
	if (is_color_display(q2s)) {
		if (q2s->cluster_reused) {
			printf("Active Cluster:	" FONT_COLOR_RED "reuse: %d\n" COLOR_NONE,
					q2s->cluster_reused);
		}

		printf("Refcount Table:	");
		if (q2s->ref_unaligned) {
			printf(FONT_COLOR_YELLOW "unaligned: " COLOR_NONE
					FONT_COLOR_RED "%ld, " COLOR_NONE,
					q2s->ref_unaligned);
		} else {
			printf(FONT_COLOR_YELLOW "unaligned: %ld, " COLOR_NONE,
					q2s->ref_unaligned);
		}
		if (q2s->ref_invalid) {
			printf(FONT_COLOR_YELLOW "invalid: " COLOR_NONE
					FONT_COLOR_RED "%ld, " COLOR_NONE,
					q2s->ref_invalid);
		} else {
			printf(FONT_COLOR_YELLOW "invalid: %ld, " COLOR_NONE,
					q2s->ref_invalid);
		}
		if (q2s->entry_miss) {
			printf(FONT_COLOR_RED "miss: %ld, " COLOR_NONE,
					q2s->entry_miss);
		}
		printf("unused: %ld, used: %ld\n",
				q2s->ref_unused, q2s->ref_used);

		if (qcow2_is_check_all(q2s)) {
			printf("Refcount:       ");
			printf(FONT_COLOR_YELLOW "error: " COLOR_NONE);
			if (q2s->corruptions) {
				printf(FONT_COLOR_RED "%ld, " COLOR_NONE,
						q2s->corruptions);
			} else {
				printf(FONT_COLOR_YELLOW "%ld, " COLOR_NONE,
						q2s->corruptions);
			}
			printf(FONT_COLOR_YELLOW "leak: %ld, " COLOR_NONE,
					q2s->leaks);
			if (q2s->rebuild) {
				printf(FONT_COLOR_RED "rebuild: %ld, " COLOR_NONE,
						q2s->rebuild);
			}
			printf("unused: %ld, used: %ld\n",
					q2s->unused, q2s->used);
		}

		if (qcow2_is_check_all(q2s)) {
			printf("L1 Table:       ");
			if (q2s->l1_unaligned) {
				printf(FONT_COLOR_YELLOW "unaligned: " COLOR_NONE
						FONT_COLOR_RED "%ld, " COLOR_NONE,
						q2s->l1_unaligned);
			} else {
				printf(FONT_COLOR_YELLOW "unaligned: %ld, " COLOR_NONE,
						q2s->l1_unaligned);
			}
			if (q2s->l1_invalid) {
				printf(FONT_COLOR_YELLOW "invalid: " COLOR_NONE
						FONT_COLOR_RED "%ld, " COLOR_NONE,
						q2s->l1_invalid);
			} else {
				printf(FONT_COLOR_YELLOW "invalid: %ld, " COLOR_NONE,
						q2s->l1_invalid);
			}
			printf("unused: %ld, used: %ld\n", q2s->l1_unused, q2s->l1_used);
			if (q2s->l1_copied) {
				printf(FONT_COLOR_YELLOW "                oflag copied: " COLOR_NONE
						FONT_COLOR_RED "%ld\n" COLOR_NONE,
						q2s->l1_copied);
			}

			printf("L2 Table:       ");
			if (q2s->l2_unaligned) {
				printf(FONT_COLOR_YELLOW "unaligned: " COLOR_NONE
						FONT_COLOR_RED "%ld, " COLOR_NONE,
						q2s->l2_unaligned);
			} else {
				printf(FONT_COLOR_YELLOW "unaligned: %ld, " COLOR_NONE,
						q2s->l2_unaligned);
			}
			if (q2s->l2_invalid) {
				printf(FONT_COLOR_YELLOW "invalid: " COLOR_NONE
						FONT_COLOR_RED "%ld, " COLOR_NONE,
						q2s->l2_invalid);
			} else {
				printf(FONT_COLOR_YELLOW "invalid: %ld, " COLOR_NONE, q2s->l2_invalid);
			}
			printf("unused: %ld, used: %ld\n", q2s->l2_unused, q2s->l2_used);
			if (q2s->l2_compressed) {
				printf(FONT_COLOR_YELLOW "                compressed: %ld\n" COLOR_NONE,
						q2s->l2_compressed);
			}
			if (q2s->l2_copied) {
				printf(FONT_COLOR_YELLOW "                oflag copied: " COLOR_NONE
						FONT_COLOR_RED "%ld\n" COLOR_NONE,
						q2s->l2_copied);
			}
		}
	} else {
		if (q2s->cluster_reused) {
			printf("Active Cluster:	reuse: %d\n", q2s->cluster_reused);
		}

		if (q2s->entry_miss) {
			printf("Refcount Table:	unaligned: %ld, invalid: %ld, "
					"miss: %ld, unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid, q2s->entry_miss,
					q2s->ref_unused, q2s->ref_used);
		} else {
			printf("Refcount Table:	unaligned: %ld, invalid: %ld, "
					"unused: %ld, used: %ld\n",
					q2s->ref_unaligned, q2s->ref_invalid,
					q2s->ref_unused, q2s->ref_used);
		}

		if (qcow2_is_check_all(q2s)) {
			if (q2s->rebuild) {
				printf("Refcount:       error: %ld, leak: %ld, "
						"rebuild: %ld, unused: %ld, used: %ld\n",
						q2s->corruptions, q2s->leaks, q2s->rebuild,
						q2s->unused, q2s->used);
			} else {
				printf("Refcount:       error: %ld, leak: %ld, "
						"unused: %ld, used: %ld\n",
						q2s->corruptions, q2s->leaks,
						q2s->unused, q2s->used);
			}
		}

		if (qcow2_is_check_all(q2s)) {
			printf("L1 Table:       unaligned: %ld, invalid: %ld, "
					"unused: %ld, used: %ld\n",
					q2s->l1_unaligned, q2s->l1_invalid,
					q2s->l1_unused, q2s->l1_used);
			if (q2s->l1_copied) {
				printf("                oflag copied: %ld\n", q2s->l1_copied);
			}

			printf("L2 Table:       unaligned: %ld, invalid: %ld, "
					"unused: %ld, used: %ld\n",
					q2s->l2_unaligned, q2s->l2_invalid,
					q2s->l2_unused, q2s->l2_used);
			if (q2s->l2_compressed) {
				printf("                compressed: %ld\n", q2s->l2_compressed);
			}
			if (q2s->l2_copied) {
				printf("                oflag copied: %ld\n", q2s->l2_copied);
			}
		}
	}
	printf("\n");
}

//输出qcow2镜像状态
static void qcow2_image_status(qcow2_state* q2s, int status)
{//可以用switch...case...简化代码
	if (status == S_IMAGE_FLAG) {
		q2s->image_status = S_IMAGE_FLAG;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_YELLOW "         qcow2 header is set corrupt flag!  (X_X)" COLOR_NONE "         ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###         qcow2 header is set corrupt flag!  (X_X)         ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_CORRUPT) {
		q2s->image_status = S_IMAGE_CORRUPT;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_RED "           Sorry: qcow2 image is corrupt!  (T_T)" COLOR_NONE "          ###\n");
			if (!qcow2_L1L2_is_corrupt(q2s)) {
				printf("###" BLINK_COLOR_GREEN "   Recommendation: -R all to rebuild refcount structure!" COLOR_NONE "  ###\n");
			}
			if (!qcow2_is_refcount_corrupt(q2s) && qcow2_is_active_corrupt(q2s)
				&& qcow2_has_good_snapshot(q2s)) {
				printf("###" BLINK_COLOR_GREEN "      qcow2 can be repaired, revert snapshot id: %03d"  COLOR_NONE "      ###\n",
						q2s->snapshot_id);
			}
			if (!qcow2_is_refcount_corrupt(q2s) && !qcow2_is_active_corrupt(q2s)) {
				printf("###" BLINK_COLOR_RED "              First corrupt snapshot id: %03d" COLOR_NONE "              ###\n",
						q2s->corrupt_id);
			}
			printf("###" FONT_COLOR_YELLOW "     Please backup this image and contact the author!" COLOR_NONE "     ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###           Sorry: qcow2 image is corrupt!  (T_T)          ###\n");
			if (!qcow2_L1L2_is_corrupt(q2s)) {
				printf("###   Recommendation: -R all to rebuild refcount structure!  ###\n");
			}
			if (!qcow2_is_refcount_corrupt(q2s) && qcow2_is_active_corrupt(q2s)
				&& qcow2_has_good_snapshot(q2s)) {
				printf("###      qcow2 can be repaired, revert snapshot id: %03d      ###\n", q2s->snapshot_id);
			}
			if (!qcow2_is_refcount_corrupt(q2s) && !qcow2_is_active_corrupt(q2s)) {
				printf("###              First corrupt snapshot id: %03d              ###\n", q2s->corrupt_id);
			}
			printf("###     Please backup this image and contact the author!     ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_WRONG) {
		q2s->image_status = S_IMAGE_WRONG;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_RED "         qcow2 image has refcount errors!   (=_=#)" COLOR_NONE "        ###\n");
			if (qcow2_maybe_copied(q2s)) {
				printf("###" BLINK_COLOR_RED "        and qcow2 image has copied errors!  (o_0)?" COLOR_NONE "        ###\n");
			}
			if (qcow2_active_cluster_reused(q2s)) {
				printf("###" BLINK_COLOR_RED "  Sadly: refcount error cause active cluster reused! Orz" COLOR_NONE "  ###\n");
			} else {
				printf("###" BLINK_COLOR_GREEN "       Recommendation: -R all to repair the image!" COLOR_NONE "        ###\n");
				if (qcow2_need_rebuild_refcount(q2s)) {
					printf("###" BLINK_COLOR_GREEN "       OR use qemu-img rebuild refcount structure!" COLOR_NONE "        ###\n");
				}
			}
			printf("###" FONT_COLOR_YELLOW "     Please backup this image and contact the author!" COLOR_NONE "     ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###         qcow2 image has refcount errors!   (=_=#)        ###\n");
			if (qcow2_maybe_copied(q2s)) {
				printf("###        and qcow2 image has copied errors!  (o_0)?        ###\n");
			}
			if (qcow2_active_cluster_reused(q2s)) {
				printf("###  Sadly: refcount error cause active cluster reused! Orz  ###\n");
			} else {
				printf("###       Recommendation: -R all to repair the image!        ###\n");
				if (qcow2_need_rebuild_refcount(q2s)) {
					printf("###       OR use qemu-img rebuild refcount structure!        ###\n");
				}
			}
			printf("###     Please backup this image and contact the author!     ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_COPIED) {
		q2s->image_status = S_IMAGE_COPIED;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_YELLOW "          qcow2 image has copied errors!  (o_0)?" COLOR_NONE "          ###\n");
			printf("###" BLINK_COLOR_GREEN "       Recommendation: -R all to repair the image!" COLOR_NONE "        ###\n");
			printf("###" FONT_COLOR_YELLOW "     Please backup this image and contact the author!" COLOR_NONE "     ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###          qcow2 image has copied errors!  (o_0)?          ###\n");
			printf("###       Recommendation: -R all to repair the image!        ###\n");
			printf("###     Please backup this image and contact the author!     ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_LEAK) {
		q2s->image_status = S_IMAGE_LEAK;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_YELLOW "          qcow2 image has refcount leaks!  (+_+)?" COLOR_NONE "         ###\n");
			printf("###" BLINK_COLOR_GREEN "       Recommendation: -R leak to repair the image!" COLOR_NONE "       ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###          qcow2 image has refcount leaks!  (+_+)?         ###\n");
			printf("###       Recommendation: -R leak to repair the image!       ###\n");
			printf("################################################################\n\n");
		}
	} else  if (status == S_IMAGE_GOOD) {
		q2s->image_status = S_IMAGE_GOOD;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_GREEN "          Haha:  qcow2 image is good!  Y(^_^)Y" COLOR_NONE "            ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###          Haha:  qcow2 image is good!  Y(^_^)Y            ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_METADATA) {
		q2s->image_status = S_IMAGE_METADATA;
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_YELLOW "         qcow2 header is set corrupt flag!  (X_X)" COLOR_NONE "         ###\n");
			printf("###" BLINK_COLOR_GREEN "         qcow2 metadata has nothing wrong!  (@_@)" COLOR_NONE "         ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###         qcow2 header is set corrupt flag!  (X_X)         ###\n");
			printf("###         qcow2 metadata has nothing wrong!  (@_@)         ###\n");
			printf("################################################################\n\n");
		}
	} else if (status == S_IMAGE_SN_HEADER) {
		if (is_color_display(q2s)) {
			printf("################################################################\n");
			printf("###" BLINK_COLOR_RED "         qcow2 snapshot header is corrupt!  (#_#)" COLOR_NONE "         ###\n");
			printf("###" BLINK_COLOR_GREEN "        Recommendation: -D/-E to repair the image!" COLOR_NONE "        ###\n");
			printf("###" BLINK_COLOR_GREEN "      OR try to modify nb_snapshots of qcow2 header!" COLOR_NONE "      ###\n");
			printf("###" FONT_COLOR_YELLOW "     Please backup this image and contact the author!" COLOR_NONE "     ###\n");
			printf("################################################################\n\n");
		} else {
			printf("################################################################\n");
			printf("###         qcow2 snapshot header is corrupt!  (#_#)         ###\n");
			printf("###        Recommendation: -D/-E to repair the image!        ###\n");
			printf("###      OR try to modify nb_snapshots of qcow2 header!      ###\n");
			printf("###     Please backup this image and contact the author!     ###\n");
			printf("################################################################\n\n");
		}
	} else {
		q2s->image_status = S_IMAGE_OTHER;
	}
}

//qcow2镜像检测结果
static inline int qcow2_check_result(qcow2_state* q2s)
{
	QCowHeader* header = &q2s->header;

	if (is_color_display(q2s)) {	//只有-m check/error在终端输出彩色
		printf(FONT_COLOR_RED "Summary:\n" COLOR_NONE);		//输出红色
	} else {
		printf("Summary:\n");
	}

	qcow2_preallocation_mode(q2s);	//输出镜像分配模式

	qcow2_metadata_stats(q2s);		//输出元数据检查统计结果

	if (qcow2_is_sn_header_corrupt(q2s)) {
		qcow2_image_status(q2s, S_IMAGE_SN_HEADER);
	} else if (qcow2_is_corrupt(q2s)) {
		qcow2_image_status(q2s, S_IMAGE_CORRUPT);
	} else if (qcow2_maybe_wrong(q2s)) {
		qcow2_image_status(q2s, S_IMAGE_WRONG);
	} else if (qcow2_maybe_copied(q2s)) {
		qcow2_image_status(q2s, S_IMAGE_COPIED);
	} else if (qcow2_maybe_leak(q2s)) {
		qcow2_image_status(q2s, S_IMAGE_LEAK);
	} else if (qcow2_is_check_all(q2s)) {
		if (header->incompatible_features & QCOW2_INCOMPAT_CORRUPT) {
			//qcow2头部有置corrupt标记, 但元数据完好.
			qcow2_image_status(q2s, S_IMAGE_METADATA);
		} else {
			//qcow2头部没置corrupt标记, 且元数据完好.
			qcow2_image_status(q2s, S_IMAGE_GOOD);
		}
	}

	printf("================================================================\n");

	return q2s->image_status;
}

//释放资源
static void qcow2_release(qcow2_state* q2s)
{
	if (qcow2_open_mutex(q2s)) {
		int ret = flock(q2s->fd, LOCK_UN);
		if (ret) {
			ret = -errno;
			printf("Line: %d | flock LOCK_UN failed: %d\n", __LINE__, ret);
		}
	}
	if (q2s->fd >= 0) {
		close(q2s->fd);
		q2s->fd = -1;
	}
	if (q2s->refcount_table) {
		free(q2s->refcount_table);
		q2s->refcount_table = NULL;
		q2s->nb_clusters = 0;
	}
	if (q2s->cluster_reuse) {
		free(q2s->cluster_reuse);
		q2s->cluster_reuse = NULL;
		q2s->cluster_reuse_cn = 0;
		q2s->cluster_reused = 0;
	}
	if (q2s->refcount_array) {
		int i;

		for (i = 0; i < q2s->refcount_array_size; i++) {
			if (q2s->refcount_array[i]) {
				free(q2s->refcount_array[i]);
				q2s->refcount_array[i] = NULL;
			}
		}
		free(q2s->refcount_array);
		q2s->refcount_array = NULL;
	}
}

static void set_rlimit_core(void)
{
	struct rlimit limit;
	int ret;

	/* Set the file size resource limit. */
	limit.rlim_cur = 4294967296;
	limit.rlim_max = 4294967296;
	if (setrlimit(RLIMIT_CORE, &limit) != 0) {
		ret = errno;
		printf("Line: %d | setrlimit failed: %s\n", __LINE__, strerror(-ret));
		exit(-ret);
	}

	/* Get the file size resource limit. */
	if (getrlimit(RLIMIT_CORE, &limit) != 0) {
		ret = errno;
		printf("Line: %d | getrlimit failed: %s\n", __LINE__, strerror(-ret));
		exit(-ret);
	}
}

int main(int argc, char **argv)
{
	int ret = 0;
	qcow2_state q2s;
	memset(&q2s, 0, sizeof(q2s));

	parse_args(&q2s, argc, argv);

	set_rlimit_core();

	ret = qcow2_open(&q2s);
	if (ret < 0) {
		printf("Line: %d | open file: %s failed: %s\n",
				__LINE__, q2s.filename, strerror(-ret));
		goto out;
	}

	qcow2_header(&q2s);

	if (qcow2_is_info_mode(&q2s)) {
		ret = qcow2_info_inactive_snapshot(&q2s);
		if (ret < 0) {
			printf("Line: %d | info inactive snapshot failed: %s\n",
					__LINE__, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_mark_mode(&q2s)) {
		ret = qcow2_mark_flags(&q2s);
		if (ret < 0) {
			printf("Line: %d | qcow2_mark_flags failed: %s\n",
					__LINE__, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_clean_mode(&q2s)) {
		ret = qcow2_clean_flags(&q2s);
		if (ret < 0) {
			printf("Line: %d | qcow2_clean_flags failed: %s\n",
					__LINE__, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_edit_mode(&q2s)) {
		ret = qcow2_edit_modify(&q2s);
		if (ret < 0) {
			printf("Line: %d | qcow2_edit_modify failed: %s\n",
					__LINE__, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_copy_mode(&q2s)) {
		ret = qcow2_copy_modify(&q2s);
		if (ret < 0) {
			printf("Line: %d | qcow2_copy_modify failed: %s\n",
					__LINE__, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_revert_mode(&q2s)) {
		ret = qcow2_revert_snapshot(&q2s);
		if (ret) {
			printf("Line: %d | Revert snapshot: %s failed: %s\n",
					__LINE__, q2s.snapshot, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_delete_mode(&q2s)) {
		ret = qcow2_delete_snapshot(&q2s);
		if (ret) {
			printf("Line: %d | delete snapshot: %s failed: %s\n",
					__LINE__, q2s.snapshot, strerror(-ret));
		}
		goto out;
	}

	if (qcow2_is_exclude_mode(&q2s)) {
		ret = qcow2_exclude_snapshot(&q2s);
		if (ret) {
			printf("Line: %d | exclude snapshot: %s failed: %s\n",
					__LINE__, q2s.snapshot, strerror(-ret));
		}
		goto out;
	}

	ret = qcow2_snapshots(&q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_snapshots failed: %s\n",
				__LINE__, strerror(-ret));
		goto out;
	}

	ret = qcow2_refcount_table(&q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_refcount_table failed: %s\n",
				__LINE__, strerror(-ret));
		goto out;
	}

	ret = qcow2_check_oflag_copied(&q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_check_oflag_copied failed: %s\n",
				__LINE__, strerror(-ret));
		goto out;
	}

	ret = qcow2_check_active_cluster_reused(&q2s);
	if (ret < 0) {
		printf("Line: %d | qcow2_check_active_cluster_reused failed: %s\n",
				__LINE__, strerror(-ret));
		goto out;
	}

	//根据返回值可以判断镜像状态, 用于shell脚本调用对返回值$?的判断
	ret = qcow2_check_result(&q2s);

out:
	qcow2_release(&q2s);

	return ret;
}
