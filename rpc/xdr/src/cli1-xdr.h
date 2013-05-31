/*
  Copyright (c) 2007-2012 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#include "xdr-common.h"
#include "compat.h"

#if defined(__GNUC__)
#if __GNUC__ >= 4
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _CLI1_XDR_H_RPCGEN
#define _CLI1_XDR_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif


enum gf_cli_defrag_type {
	GF_DEFRAG_CMD_START = 1,
	GF_DEFRAG_CMD_STOP = 1 + 1,
	GF_DEFRAG_CMD_STATUS = 1 + 2,
	GF_DEFRAG_CMD_START_LAYOUT_FIX = 1 + 3,
	GF_DEFRAG_CMD_START_FORCE = 1 + 4,
};
typedef enum gf_cli_defrag_type gf_cli_defrag_type;

enum gf_defrag_status_t {
	GF_DEFRAG_STATUS_NOT_STARTED = 0,
	GF_DEFRAG_STATUS_STARTED = 1,
	GF_DEFRAG_STATUS_STOPPED = 2,
	GF_DEFRAG_STATUS_COMPLETE = 3,
	GF_DEFRAG_STATUS_FAILED = 4,
};
typedef enum gf_defrag_status_t gf_defrag_status_t;

enum gf1_cluster_type {
	GF_CLUSTER_TYPE_NONE = 0,
	GF_CLUSTER_TYPE_STRIPE = 0 + 1,
	GF_CLUSTER_TYPE_REPLICATE = 0 + 2,
	GF_CLUSTER_TYPE_STRIPE_REPLICATE = 0 + 3,
};
typedef enum gf1_cluster_type gf1_cluster_type;

enum gf1_cli_replace_op {
	GF_REPLACE_OP_NONE = 0,
	GF_REPLACE_OP_START = 0 + 1,
	GF_REPLACE_OP_COMMIT = 0 + 2,
	GF_REPLACE_OP_PAUSE = 0 + 3,
	GF_REPLACE_OP_ABORT = 0 + 4,
	GF_REPLACE_OP_STATUS = 0 + 5,
	GF_REPLACE_OP_COMMIT_FORCE = 0 + 6,
};
typedef enum gf1_cli_replace_op gf1_cli_replace_op;

enum gf1_op_commands {
	GF_OP_CMD_NONE = 0,
	GF_OP_CMD_START = 0 + 1,
	GF_OP_CMD_COMMIT = 0 + 2,
	GF_OP_CMD_STOP = 0 + 3,
	GF_OP_CMD_STATUS = 0 + 4,
	GF_OP_CMD_COMMIT_FORCE = 0 + 5,
};
typedef enum gf1_op_commands gf1_op_commands;

enum gf_quota_type {
	GF_QUOTA_OPTION_TYPE_NONE = 0,
	GF_QUOTA_OPTION_TYPE_ENABLE = 0 + 1,
	GF_QUOTA_OPTION_TYPE_DISABLE = 0 + 2,
	GF_QUOTA_OPTION_TYPE_LIMIT_USAGE = 0 + 3,
	GF_QUOTA_OPTION_TYPE_REMOVE = 0 + 4,
	GF_QUOTA_OPTION_TYPE_LIST = 0 + 5,
	GF_QUOTA_OPTION_TYPE_VERSION = 0 + 6,
	GF_QUOTA_OPTION_TYPE_SOFT_LIMIT = 0 + 7,
	GF_QUOTA_OPTION_TYPE_ALERT_TIME = 0 + 8,
	GF_QUOTA_OPTION_TYPE_SOFT_TIMEOUT = 0 + 9,
	GF_QUOTA_OPTION_TYPE_HARD_TIMEOUT = 0 + 10,
};
typedef enum gf_quota_type gf_quota_type;

enum gf1_cli_friends_list {
	GF_CLI_LIST_PEERS = 1,
	GF_CLI_LIST_POOL_NODES = 2,
};
typedef enum gf1_cli_friends_list gf1_cli_friends_list;

enum gf1_cli_get_volume {
	GF_CLI_GET_VOLUME_ALL = 1,
	GF_CLI_GET_VOLUME = 1 + 1,
	GF_CLI_GET_NEXT_VOLUME = 1 + 2,
};
typedef enum gf1_cli_get_volume gf1_cli_get_volume;

enum gf1_cli_sync_volume {
	GF_CLI_SYNC_ALL = 1,
};
typedef enum gf1_cli_sync_volume gf1_cli_sync_volume;

enum gf1_cli_op_flags {
	GF_CLI_FLAG_OP_FORCE = 1,
};
typedef enum gf1_cli_op_flags gf1_cli_op_flags;

enum gf1_cli_gsync_set {
	GF_GSYNC_OPTION_TYPE_NONE = 0,
	GF_GSYNC_OPTION_TYPE_START = 1,
	GF_GSYNC_OPTION_TYPE_STOP = 2,
	GF_GSYNC_OPTION_TYPE_CONFIG = 3,
	GF_GSYNC_OPTION_TYPE_STATUS = 4,
	GF_GSYNC_OPTION_TYPE_ROTATE = 5,
};
typedef enum gf1_cli_gsync_set gf1_cli_gsync_set;

enum gf1_cli_stats_op {
	GF_CLI_STATS_NONE = 0,
	GF_CLI_STATS_START = 1,
	GF_CLI_STATS_STOP = 2,
	GF_CLI_STATS_INFO = 3,
	GF_CLI_STATS_TOP = 4,
};
typedef enum gf1_cli_stats_op gf1_cli_stats_op;

enum gf1_cli_top_op {
	GF_CLI_TOP_NONE = 0,
	GF_CLI_TOP_OPEN = 0 + 1,
	GF_CLI_TOP_READ = 0 + 2,
	GF_CLI_TOP_WRITE = 0 + 3,
	GF_CLI_TOP_OPENDIR = 0 + 4,
	GF_CLI_TOP_READDIR = 0 + 5,
	GF_CLI_TOP_READ_PERF = 0 + 6,
	GF_CLI_TOP_WRITE_PERF = 0 + 7,
};
typedef enum gf1_cli_top_op gf1_cli_top_op;

enum gf_cli_status_type {
	GF_CLI_STATUS_NONE = 0x0000,
	GF_CLI_STATUS_MEM = 0x0001,
	GF_CLI_STATUS_CLIENTS = 0x0002,
	GF_CLI_STATUS_INODE = 0x0004,
	GF_CLI_STATUS_FD = 0x0008,
	GF_CLI_STATUS_CALLPOOL = 0x0010,
	GF_CLI_STATUS_DETAIL = 0x0020,
	GF_CLI_STATUS_MASK = 0x00FF,
	GF_CLI_STATUS_VOL = 0x0100,
	GF_CLI_STATUS_ALL = 0x0200,
	GF_CLI_STATUS_BRICK = 0x0400,
	GF_CLI_STATUS_NFS = 0x0800,
	GF_CLI_STATUS_SHD = 0x1000,
};
typedef enum gf_cli_status_type gf_cli_status_type;

struct gf_cli_req {
	struct {
		u_int dict_len;
		char *dict_val;
	} dict;
};
typedef struct gf_cli_req gf_cli_req;

struct gf_cli_rsp {
	int op_ret;
	int op_errno;
	char *op_errstr;
	struct {
		u_int dict_len;
		char *dict_val;
	} dict;
};
typedef struct gf_cli_rsp gf_cli_rsp;

struct gf1_cli_probe_req {
	char *hostname;
	int port;
};
typedef struct gf1_cli_probe_req gf1_cli_probe_req;

struct gf1_cli_probe_rsp {
	int op_ret;
	int op_errno;
	int port;
	char *hostname;
	char *op_errstr;
};
typedef struct gf1_cli_probe_rsp gf1_cli_probe_rsp;

struct gf1_cli_deprobe_req {
	char *hostname;
	int port;
	int flags;
};
typedef struct gf1_cli_deprobe_req gf1_cli_deprobe_req;

struct gf1_cli_deprobe_rsp {
	int op_ret;
	int op_errno;
	char *hostname;
	char *op_errstr;
};
typedef struct gf1_cli_deprobe_rsp gf1_cli_deprobe_rsp;

struct gf1_cli_peer_list_req {
	int flags;
	struct {
		u_int dict_len;
		char *dict_val;
	} dict;
};
typedef struct gf1_cli_peer_list_req gf1_cli_peer_list_req;

struct gf1_cli_peer_list_rsp {
	int op_ret;
	int op_errno;
	struct {
		u_int friends_len;
		char *friends_val;
	} friends;
};
typedef struct gf1_cli_peer_list_rsp gf1_cli_peer_list_rsp;

struct gf1_cli_fsm_log_req {
	char *name;
};
typedef struct gf1_cli_fsm_log_req gf1_cli_fsm_log_req;

struct gf1_cli_fsm_log_rsp {
	int op_ret;
	int op_errno;
	char *op_errstr;
	struct {
		u_int fsm_log_len;
		char *fsm_log_val;
	} fsm_log;
};
typedef struct gf1_cli_fsm_log_rsp gf1_cli_fsm_log_rsp;

struct gf1_cli_getwd_req {
	int unused;
};
typedef struct gf1_cli_getwd_req gf1_cli_getwd_req;

struct gf1_cli_getwd_rsp {
	int op_ret;
	int op_errno;
	char *wd;
};
typedef struct gf1_cli_getwd_rsp gf1_cli_getwd_rsp;

struct gf1_cli_mount_req {
	char *label;
	struct {
		u_int dict_len;
		char *dict_val;
	} dict;
};
typedef struct gf1_cli_mount_req gf1_cli_mount_req;

struct gf1_cli_mount_rsp {
	int op_ret;
	int op_errno;
	char *path;
};
typedef struct gf1_cli_mount_rsp gf1_cli_mount_rsp;

struct gf1_cli_umount_req {
	int lazy;
	char *path;
};
typedef struct gf1_cli_umount_req gf1_cli_umount_req;

struct gf1_cli_umount_rsp {
	int op_ret;
	int op_errno;
};
typedef struct gf1_cli_umount_rsp gf1_cli_umount_rsp;

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_gf_cli_defrag_type (XDR *, gf_cli_defrag_type*);
extern  bool_t xdr_gf_defrag_status_t (XDR *, gf_defrag_status_t*);
extern  bool_t xdr_gf1_cluster_type (XDR *, gf1_cluster_type*);
extern  bool_t xdr_gf1_cli_replace_op (XDR *, gf1_cli_replace_op*);
extern  bool_t xdr_gf1_op_commands (XDR *, gf1_op_commands*);
extern  bool_t xdr_gf_quota_type (XDR *, gf_quota_type*);
extern  bool_t xdr_gf1_cli_friends_list (XDR *, gf1_cli_friends_list*);
extern  bool_t xdr_gf1_cli_get_volume (XDR *, gf1_cli_get_volume*);
extern  bool_t xdr_gf1_cli_sync_volume (XDR *, gf1_cli_sync_volume*);
extern  bool_t xdr_gf1_cli_op_flags (XDR *, gf1_cli_op_flags*);
extern  bool_t xdr_gf1_cli_gsync_set (XDR *, gf1_cli_gsync_set*);
extern  bool_t xdr_gf1_cli_stats_op (XDR *, gf1_cli_stats_op*);
extern  bool_t xdr_gf1_cli_top_op (XDR *, gf1_cli_top_op*);
extern  bool_t xdr_gf_cli_status_type (XDR *, gf_cli_status_type*);
extern  bool_t xdr_gf_cli_req (XDR *, gf_cli_req*);
extern  bool_t xdr_gf_cli_rsp (XDR *, gf_cli_rsp*);
extern  bool_t xdr_gf1_cli_probe_req (XDR *, gf1_cli_probe_req*);
extern  bool_t xdr_gf1_cli_probe_rsp (XDR *, gf1_cli_probe_rsp*);
extern  bool_t xdr_gf1_cli_deprobe_req (XDR *, gf1_cli_deprobe_req*);
extern  bool_t xdr_gf1_cli_deprobe_rsp (XDR *, gf1_cli_deprobe_rsp*);
extern  bool_t xdr_gf1_cli_peer_list_req (XDR *, gf1_cli_peer_list_req*);
extern  bool_t xdr_gf1_cli_peer_list_rsp (XDR *, gf1_cli_peer_list_rsp*);
extern  bool_t xdr_gf1_cli_fsm_log_req (XDR *, gf1_cli_fsm_log_req*);
extern  bool_t xdr_gf1_cli_fsm_log_rsp (XDR *, gf1_cli_fsm_log_rsp*);
extern  bool_t xdr_gf1_cli_getwd_req (XDR *, gf1_cli_getwd_req*);
extern  bool_t xdr_gf1_cli_getwd_rsp (XDR *, gf1_cli_getwd_rsp*);
extern  bool_t xdr_gf1_cli_mount_req (XDR *, gf1_cli_mount_req*);
extern  bool_t xdr_gf1_cli_mount_rsp (XDR *, gf1_cli_mount_rsp*);
extern  bool_t xdr_gf1_cli_umount_req (XDR *, gf1_cli_umount_req*);
extern  bool_t xdr_gf1_cli_umount_rsp (XDR *, gf1_cli_umount_rsp*);

#else /* K&R C */
extern bool_t xdr_gf_cli_defrag_type ();
extern bool_t xdr_gf_defrag_status_t ();
extern bool_t xdr_gf1_cluster_type ();
extern bool_t xdr_gf1_cli_replace_op ();
extern bool_t xdr_gf1_op_commands ();
extern bool_t xdr_gf_quota_type ();
extern bool_t xdr_gf1_cli_friends_list ();
extern bool_t xdr_gf1_cli_get_volume ();
extern bool_t xdr_gf1_cli_sync_volume ();
extern bool_t xdr_gf1_cli_op_flags ();
extern bool_t xdr_gf1_cli_gsync_set ();
extern bool_t xdr_gf1_cli_stats_op ();
extern bool_t xdr_gf1_cli_top_op ();
extern bool_t xdr_gf_cli_status_type ();
extern bool_t xdr_gf_cli_req ();
extern bool_t xdr_gf_cli_rsp ();
extern bool_t xdr_gf1_cli_probe_req ();
extern bool_t xdr_gf1_cli_probe_rsp ();
extern bool_t xdr_gf1_cli_deprobe_req ();
extern bool_t xdr_gf1_cli_deprobe_rsp ();
extern bool_t xdr_gf1_cli_peer_list_req ();
extern bool_t xdr_gf1_cli_peer_list_rsp ();
extern bool_t xdr_gf1_cli_fsm_log_req ();
extern bool_t xdr_gf1_cli_fsm_log_rsp ();
extern bool_t xdr_gf1_cli_getwd_req ();
extern bool_t xdr_gf1_cli_getwd_rsp ();
extern bool_t xdr_gf1_cli_mount_req ();
extern bool_t xdr_gf1_cli_mount_rsp ();
extern bool_t xdr_gf1_cli_umount_req ();
extern bool_t xdr_gf1_cli_umount_rsp ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_CLI1_XDR_H_RPCGEN */
