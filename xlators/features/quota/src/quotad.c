/*
   Copyright (c) 2013 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/
#include <fnmatch.h>

#include "quota.h"
#include "common-utils.h"
#include "defaults.h"
#include "syncop.h"
#include "libgen.h"


inline uint64_t
quota_time_elapsed (struct timeval *now, struct timeval *then)
{
        return (now->tv_sec - then->tv_sec);
}


int32_t
quota_timeout (struct timeval *tv, uint64_t timeout)
{
        struct timeval now       = {0,};
        int32_t        timed_out = 0;

        gettimeofday (&now, NULL);

        if (quota_time_elapsed (&now, tv) >= timeout) {
                timed_out = 1;
        }

        return timed_out;
}

/* Returns itable->root, also creates itable if not present */
inode_t *
qd_build_root_inode (xlator_t *this, qd_vols_conf_t *this_vol)
{
        if (!this_vol->itable) {
                this_vol->itable = inode_table_new (0, this);
                if (!this_vol->itable)
                        return NULL;
        }

        return inode_ref (this_vol->itable->root);
}

int
qd_resolve_root (xlator_t *subvol, loc_t *root_loc,
                 struct iatt *iatt, dict_t *dict_req, dict_t **dict_rsp)
{
        int     ret             = -1;

        ret = syncop_lookup (subvol, root_loc, dict_req, iatt, dict_rsp, NULL);
        if (-1 == ret) {
                gf_log (subvol->name, GF_LOG_ERROR, "Received %s for lookup "
                         "on / (vol:%s)", strerror (errno), subvol->name);
        }
        return ret;
}

int
qd_build_root_loc (xlator_t *this, xlator_t *subvol, inode_t *inode, loc_t *loc)
{

        loc->path = gf_strdup ("/");
        loc->inode = inode;
        uuid_clear (loc->gfid);
        loc->gfid[15] = 1;

        return qd_resolve_root (subvol, loc, NULL, NULL, NULL);
}


int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        if (!this)
                return ret;

        ret = xlator_mem_acct_init (this, gf_quota_mt_end + 1);

        if (0 != ret) {
                gf_log (this->name, GF_LOG_WARNING, "Memory accounting "
                        "init failed");
                return ret;
        }

        return ret;
}

/**
 * Takes the limit string, parse it, fill limits_t struct and insert into
 * above-soft list.
 *
 * Format for limit string
 * <limit-string> = <limit-on-single-dir>[,<limit-on-single-dir>]*
 * <limit-on-single-dir> =
 *                    <abs-path-from-volume-root>:<hard-limit>[:<soft-limit-%>]
 */
int
qd_parse_limits (quota_priv_t *priv, xlator_t *this, char *limit_str,
                    struct list_head *old_list, qd_vols_conf_t *this_vol)
{
        int32_t       ret       = -1;
        char         *str_val   = NULL;
        char         *path      = NULL, *saveptr = NULL;
        uint64_t      value     = 0;
        limits_t     *quota_lim = NULL;
        char         *str       = NULL;
        double        soft_l    = -1;
        char         *limit_on_dir      = NULL;
        char         *saveptr_dir       = NULL;

        str = gf_strdup (limit_str);

        if (str) {
                limit_on_dir = strtok_r (str, ",", &saveptr);

                while (limit_on_dir) {
                        QUOTA_ALLOC_OR_GOTO (quota_lim, limits_t, err);
                        gettimeofday (&quota_lim->prev_log_tv, NULL);

                        saveptr_dir = NULL;

                        path = strtok_r (limit_on_dir, ":", &saveptr_dir);

                        str_val = strtok_r (NULL, ":", &saveptr_dir);

                        ret = gf_string2bytesize (str_val, &value);
                        if (0 != ret)
                                goto err;

                        quota_lim->hard_lim = value;

                        str_val = strtok_r (NULL, ",", &saveptr_dir);

                        soft_l = this_vol->default_soft_lim;
                        if (str_val) {
                                ret = gf_string2percent (str_val, &soft_l);
                                if (ret)
                                        gf_log (this->name, GF_LOG_WARNING,
                                                "Failed to convert str to "
                                                "percent. Using default soft "
                                                "limit");
                        }

                        quota_lim->soft_lim = (int64_t) quota_lim->hard_lim *
                                                                (soft_l / 100);

                        quota_lim->path = gf_strdup (path);
                        if (!quota_lim->path) {
                                gf_log (this->name, GF_LOG_ERROR, "ENOMEM "
                                        "Received copying the path");
                                goto err;
                        }

                        quota_lim->prev_size = quota_lim->hard_lim;

                        gf_log (this->name, GF_LOG_INFO, "%s:%"PRId64":%"PRId32,
                                quota_lim->path, quota_lim->hard_lim,
                                (int)quota_lim->soft_lim);

                        /* This is used in the reconfigure path, so not used
                         * by quotad as of now.
                        if (NULL != old_list) {
                                list_for_each_entry (old, old_list,
                                                     limit_list) {
                                        if (0 ==
                                            strcmp (old->path, quota_lim->path)) {
                                                uuid_copy (quota_lim->gfid,
                                                           old->gfid);
                                                break;
                                        }
                                }
                        } */

                        LOCK (&this_vol->lock);
                        {
                                list_add_tail (&quota_lim->limit_list,
                                               &this_vol->above_soft.limit_head);
                        }
                        UNLOCK (&this_vol->lock);

                        limit_on_dir = strtok_r (NULL, ",", &saveptr);
                }
        } else {
                gf_log (this->name, GF_LOG_INFO,
                        "no \"limit-set\" option provided");
        }

        ret = 0;
err:
        GF_FREE (str);
        return ret;
}


xlator_t *
qd_get_subvol (xlator_t *this, qd_vols_conf_t *this_vol)
{
        xlator_list_t   *subvol        = NULL;

        for (subvol = this->children; subvol; subvol = subvol->next)
                if (0 == strcmp (subvol->xlator->name, this_vol->name))
                        return subvol->xlator;

        return NULL;
}

/* Logs if
 *  i.   Usage crossed soft limit
 *  ii.  Usage above soft limit and alert-time timed out
 */
void
qd_log_usage (xlator_t *this, qd_vols_conf_t *this_vol, limits_t *entry,
              int64_t cur_size)
{
        struct timeval           cur_time       = {0,};

        gettimeofday (&cur_time, NULL);

        if (DID_CROSS_SOFT_LIMIT (entry->soft_lim,
                                  entry->prev_size, cur_size)) {
                entry->prev_log_tv = cur_time;
                gf_log (this->name, GF_LOG_ALERT, "Usage crossed soft limit:"
                        " %ld for %s", entry->soft_lim, entry->path);
        }
        if (cur_size > entry->soft_lim &&
                  quota_timeout (&entry->prev_log_tv, this_vol->log_timeout)) {
                entry->prev_log_tv = cur_time;
                gf_log (this->name, GF_LOG_ALERT, "Usage %ld %s limit for %s",
                        cur_size, (cur_size > entry->hard_lim)?
                        "has reached hard": "is above soft", entry->path);
        }
}

int
qd_build_child_loc (loc_t *child, loc_t *parent, char *name)
{
        if (!child) {
                goto err;
        }

        if (strcmp (parent->path, "/") == 0)
                gf_asprintf ((char **)&child->path, "/%s", name);
        else
                gf_asprintf ((char **)&child->path, "%s/%s", parent->path,
                             name);

        if (!child->path) {
                goto err;
        }

        child->name = strrchr (child->path, '/');
        if (child->name)
                child->name++;

        child->parent = inode_ref (parent->inode);
        if (!child->inode)
                child->inode = inode_new (parent->inode->table);

        if (!child->inode) {
                goto err;
        }
        if (!uuid_is_null(parent->gfid))
                uuid_copy (child->pargfid, parent->gfid);
        else
                uuid_copy (child->pargfid, parent->inode->gfid);

        return 0;
err:
        loc_wipe (child);
        return -1;
}

int
qd_check_enforce (qd_vols_conf_t *conf, dict_t *dict, limits_t *entry,
                  loc_t *entry_loc, xlator_t *subvol)
{
        xlator_t        *this = THIS;
        int64_t         *size = NULL;
        int              ret  = -1;
        int64_t          cur_size       = 0;
        int64_t          prev_size      = 0;
        dict_t          *setxattr_dict  = NULL;

        ret = dict_get_bin (dict, QUOTA_SIZE_KEY, (void **)&size);
        if (ret) {
                gf_log (this->name, GF_LOG_WARNING, "Couldn't get size"
                        " from the dict (%s)", entry->path);
                goto out;
        }

        cur_size = ntoh64 (*size);

        qd_log_usage (this, conf, entry, cur_size);

        /* if size hasn't changed, just skip the updation */
        if (entry->prev_size == cur_size)
                goto out;

        prev_size = entry->prev_size;

        QUOTA_ALLOC_OR_GOTO (size, int64_t, out);
        *size = hton64 (cur_size);

        setxattr_dict = dict_new();
        ret = dict_set_bin (setxattr_dict, QUOTA_UPDATE_USAGE_KEY, size,
                           sizeof (int64_t));
        if (-1 == ret) {
                gf_log (this->name, GF_LOG_WARNING,
                        "Couldn't set dict");
                goto out;
        }


        /* There is a possibility that, after a setxattr is done,
           a rename might happen and the resolve will fail again.
        */
        ret = syncop_setxattr (subvol, entry_loc, setxattr_dict, 0);
        if (ret) {
		if (errno == (ESTALE|ENOENT))
			inode_forget (entry_loc->inode, 0);
                gf_log (this->name, GF_LOG_ERROR,
                        "Received ERROR:%s in updating quota value %s "
                        " (vol:%s). Quota enforcement may not be"
                        " accurate", strerror (errno), entry->path,
                         subvol->name);
                goto out;
        }

        /* Move the limit-node to the corresponding list,
         * based on the usage */
        LOCK (&conf->lock);
        {
                /* usage > soft_limit? */
                if (prev_size < entry->soft_lim &&
                        cur_size >= entry->soft_lim) {
                        list_move (&entry->limit_list,
                                   &(conf->above_soft.limit_head));
                 /* usage < soft_limit? */
                } else if (prev_size >= entry->soft_lim &&
                        cur_size < entry->soft_lim) {
                        list_move (&entry->limit_list,
                                   &(conf->below_soft.limit_head));
                }

                entry->prev_size = cur_size;
        }
        UNLOCK (&conf->lock);

out:
        if (setxattr_dict)
                dict_unref (setxattr_dict);

        return ret;
}

int
qd_resolve_handle_error (loc_t *comp_loc, inode_t *inode,
                         int op_ret, int op_errno)
{

        if (op_ret) {
                switch (op_errno) {
                case ESTALE:
                case ENOENT:
                       inode_forget (inode, 0);
                       break;
                default:
                        gf_log ("", GF_LOG_ERROR, "lookup on %s returned %s",
                                comp_loc->path, strerror(op_errno));
                }
        }

        return 0;
}

int
qd_resolve_entry (qd_vols_conf_t *conf, xlator_t *subvol, limits_t *entry,
                   dict_t *dict_req, loc_t *loc, dict_t **dict_rsp,
                   int force)
{
        char            *component      = NULL;
        char            *next_comp      = NULL;
        char            *saveptr        = NULL;
        char            *path           = NULL;
        int              ret            = -1;
        struct iatt      iatt           = {0,};
        inode_t         *inode          = NULL;
        dict_t          *tmp_dict       = NULL;
        loc_t            comp_loc       = {0,};
        loc_t            par_loc        = {0,};
        int              need_lookup    = 0;

        path = gf_strdup (entry->path);

        loc_copy (&par_loc, &conf->root_loc);
        for (component = strtok_r (path, "/", &saveptr);
	     component; component = next_comp) {

		next_comp = strtok_r (NULL, "/", &saveptr);

                inode = inode_grep (conf->itable,
                                    par_loc.inode,
                                    component);
                /* if inode is found, and force lookup, forget inode */
                if (inode && force) {
                        inode_forget (inode, 0);
                        inode = NULL;
                }

                /* if inode is found, then skip lookup unless last component */
                if (inode) {
                        comp_loc.inode = inode;
                        need_lookup = 0;
                } else {
                        need_lookup = 1;
                }

                qd_build_child_loc (&comp_loc, &par_loc, component);
                /* Get the xattrs in lookup for the last component */
                if (!next_comp) {
                        tmp_dict = dict_req;
                        need_lookup = 1;
                }

                if (need_lookup || force) {
                        ret = syncop_lookup (subvol, &comp_loc, tmp_dict, &iatt,
                                             dict_rsp, NULL);
                        if (ret) {
                                /* invalidate inode got from inode_grep if
                                 * ESTALE/ENOENT */
                                qd_resolve_handle_error (&comp_loc, inode,
                                                         ret, errno);
                                goto out;
                        }

                        if (!IA_ISDIR (iatt.ia_type)) {
			        gf_log (subvol->name, GF_LOG_ERROR,
                                        "%s is not a directory",
                                        comp_loc.path);
			        goto out;
		        }

                        /* if inode not in itable, link it */
                        if (!inode) {
                                inode_link (comp_loc.inode, par_loc.inode,
                                            component, &iatt);
                                inode_lookup (comp_loc.inode);
                                inode_unref (comp_loc.inode);
                        }
                }
                inode = NULL;
                loc_wipe (&par_loc);
                loc_copy (&par_loc, &comp_loc);
                loc_wipe (&comp_loc);
	}
        ret = 0;
        loc_copy (loc, &par_loc);

out:
        loc_wipe (&par_loc);
        loc_wipe (&comp_loc);
        return ret;
}

int
qd_handle_entry (qd_vols_conf_t *conf, xlator_t *subvol, limits_t *entry,
                 dict_t *dict_req, int revalidate)
{
        dict_t          *dict_rsp  = NULL;
        loc_t            entry_loc = {0,};
        int              ret       = -1;

        if (!strcmp (entry->path, "/")) {
                ret = qd_resolve_root (subvol, &conf->root_loc, NULL, dict_req,
                                       &dict_rsp);
                if (ret) {
                        gf_log (subvol->name, GF_LOG_ERROR, "lookup on / "
                                "(%s)", strerror(errno));
                        goto err;
                }
                loc_copy (&entry_loc, &conf->root_loc);

        } else {
                ret = qd_resolve_entry (conf, subvol, entry, dict_req,
                                        &entry_loc, &dict_rsp, revalidate);
                /* if resolve failed, force resolve from "/" once */
                if (ret) {
                        gf_log (subvol->name, GF_LOG_ERROR, "Quota check on %s"
                                " failed", entry_loc.path);
                        if (!revalidate) {
                                ret = qd_handle_entry (conf, subvol, entry,
                                                       dict_req, 1);
                                if (ret)
                                        goto err;
                        }
                        goto err;
                }
        }
        ret = qd_check_enforce (conf, dict_rsp, entry, &entry_loc, subvol);
        if (ret)
                gf_log (subvol->name, GF_LOG_ERROR,
                        "Failed to enforce quota on %s", entry_loc.path);
err:
        loc_wipe (&entry_loc);
        if (dict_rsp)
                dict_unref (dict_rsp);
        return 0;
}

int
qd_iterator (qd_vols_conf_t  *conf, xlator_t *subvol,
            struct limits_level *list)
{
        limits_t        *entry          = NULL;
        limits_t        *next           = NULL;
        int32_t          ret            = -1;
        dict_t          *dict_req       = NULL;

        GF_VALIDATE_OR_GOTO ("qd-iterator", conf, out);
        GF_VALIDATE_OR_GOTO ("qd-iterator", list, out);
        GF_VALIDATE_OR_GOTO ("qd-iterator", subvol, out);

        dict_req = dict_new ();
        GF_VALIDATE_OR_GOTO ("qd-iterator", dict_req, out);
        ret = dict_set_uint64 (dict_req, QUOTA_SIZE_KEY, 0);
        if (ret) {
                gf_log ("qd-iterator", GF_LOG_ERROR, "dict set failed for "
                        "QUOTA SIZE key");
                goto out;
        }
        list_for_each_entry_safe (entry, next, &list->limit_head, limit_list) {
                ret = qd_handle_entry (conf, subvol, entry, dict_req, 0);
                if (ret) {
                        gf_log ("qd-iterator", GF_LOG_ERROR, "Failed to check "
                                "quota limit on %s", entry->path);
                }
        }
        ret = 0;
out:
        if (dict_req)
                dict_unref (dict_req);
        return ret;
}

int
qd_trigger_periodically (void *args)
{
        int                      ret            = -1;
        struct limits_level     *list           = NULL;
        xlator_t                *this           = NULL;
        xlator_t                *subvol         = NULL;
        inode_t                 *root_inode     = NULL;
        qd_vols_conf_t          *conf           = NULL;

        this = THIS;
        list = args;

        conf = GET_CONF (list);
        if (!conf)
                goto out;
        subvol = qd_get_subvol (this, conf);
        if (!subvol) {
                gf_log (this->name, GF_LOG_ERROR, "No subvol found");
                return -1;
        }

        root_inode = qd_build_root_inode (this, conf);
        if (!root_inode) {
                gf_log (this->name, GF_LOG_ERROR,
                        "New itable creation failed");
                return -1;
        }

        ret = qd_build_root_loc (this, subvol, root_inode, &conf->root_loc);
        if (ret) {
                gf_log (this->name, GF_LOG_ERROR, "Failed to build root_loc "
                        "for %s", GET_CONF (list)->name);
                goto out;
        }

        while (1) {
                if (!list_empty (&list->limit_head)) {
                        ret = qd_iterator (conf, subvol, list);
                        if (ret)
                                gf_log (this->name, GF_LOG_WARNING,
                                        "Couldn't update the usage, frequent "
                                        "log may lead usage to cross beyond "
                                        "limit");
                }

                sleep ((unsigned int) (list->time_out));
        }

out:
        return ret;
}

int
qd_trigger_periodically_try_again (int ret, call_frame_t *frame, void *args)
{
        if (ret) {
                gf_log ("quotad", GF_LOG_ERROR,
                        "Synctask stopped unexpectedly, trying to restart");
        }

        ret = synctask_new (THIS->ctx->env,
                            qd_trigger_periodically,
                            qd_trigger_periodically_try_again,
                            frame, args);
        if (-1 == ret)
                gf_log ("quotad", GF_LOG_ERROR, "Synctask creation "
                        "failed for %s",
                        (GET_CONF ((struct limits_level *)args))->name);

        return ret;
}


int
qd_start_threads (xlator_t *this, int subvol_idx)
{
        quota_priv_t            *priv           = NULL;
        int                      ret            = 0;
        qd_vols_conf_t          *this_vol       = NULL;

        priv = this->private;

        this_vol = priv->qd_vols_conf[subvol_idx];

        if (list_empty (&this_vol->above_soft.limit_head)) {
                gf_log (this->name, GF_LOG_DEBUG, "No limit is set on "
                        "volume %s", this_vol->name);
                /* Dafault ret is 0 */
                goto err;
        }

        /* Create 2 threads for soft and hard limits */
        ret = synctask_new (this->ctx->env,
                            qd_trigger_periodically,
                            qd_trigger_periodically_try_again,
                            NULL, (void *)&this_vol->below_soft);
        if (-1 == ret) {
                gf_log (this->name, GF_LOG_ERROR, "Synctask creation "
                        "failed for %s", this_vol->name);
                goto err;
        }

        ret = synctask_new (this->ctx->env,
                            qd_trigger_periodically,
                            qd_trigger_periodically_try_again,
                            NULL, (void *)&this_vol->above_soft);
        if (-1 == ret) {
                gf_log (this->name, GF_LOG_ERROR, "Synctask creation "
                        "failed for %s", this_vol->name);
                goto err;
        }
err:
        return ret;
}

int
qd_reconfigure (xlator_t *this, dict_t *options)
{
        /* As of now quotad is restarted upon alteration of volfile */
        return 0;
}

void
qd_fini (xlator_t *this)
{
        return;
}

int32_t
qd_init (xlator_t *this)
{
        int32_t          ret            = -1;
        quota_priv_t    *priv           = NULL;
        int              i              = 0;
        char            *option_str     = NULL;
        xlator_list_t   *subvol         = NULL;
        char            *limits         = NULL;
        int              subvol_cnt     = 0;
        qd_vols_conf_t  *this_vol       = NULL;

        if (NULL == this->children) {
                gf_log (this->name, GF_LOG_ERROR,
                        "FATAL: quota (%s) not configured for min of 1 child",
                        this->name);
                ret = -1;
                goto err;
        }

        QUOTA_ALLOC_OR_GOTO (priv, quota_priv_t, err);

        LOCK_INIT (&priv->lock);

        this->private = priv;

        for (subvol_cnt = 0, subvol = this->children;
             subvol;
             subvol_cnt++, subvol = subvol->next);

        priv->qd_vols_conf = GF_CALLOC (sizeof (qd_vols_conf_t *), subvol_cnt,
                                        gf_quota_mt_qd_vols_conf_t);
        if (!priv->qd_vols_conf) {
                gf_log (this->name, GF_LOG_ERROR, "Failed to allocate memory");
                goto err;
        }

        for (i = 0, subvol = this->children;
             subvol;
             subvol = subvol->next, i++) {

                QUOTA_ALLOC_OR_GOTO (priv->qd_vols_conf[i],
                                     qd_vols_conf_t, err);

                this_vol = priv->qd_vols_conf[i];

                LOCK_INIT (&this_vol->lock);
                INIT_LIST_HEAD (&this_vol->above_soft.limit_head);
                INIT_LIST_HEAD (&this_vol->below_soft.limit_head);

                this_vol->name = subvol->xlator->name;

                this_vol->below_soft.my_vol =
                this_vol->above_soft.my_vol = this_vol;

                ret = gf_asprintf (&option_str, "%s.default-soft-limit",
                                   this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->default_soft_lim, percent,
                                err);
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.alert-time",
                                   this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->log_timeout, time, err);
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.limit-set", this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                ret = dict_get_str (this->options, option_str, &limits);
                if (ret < 0) {
                        gf_log (this->name, GF_LOG_ERROR, "dict get failed or "
                                "no limits set");
                        continue;
                }

                ret = qd_parse_limits (priv, this, limits, NULL, this_vol);
                if (ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "Couldn't parse limits for %s", this_vol->name);
                        goto err;
                }
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.soft-timeout",
                                   this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->below_soft.time_out,
                                time, err);
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.hard-timeout",
                                   this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->above_soft.time_out,
                                time, err);
                GF_FREE (option_str);
        }

        this->local_pool = mem_pool_new (quota_local_t, 64);
        if (!this->local_pool) {
                ret = -1;
                gf_log (this->name, GF_LOG_ERROR,
                        "failed to create local_t's memory pool");
                goto err;
        }

        ret = 0;
err:
        /* Free all allocated variables */
        if (ret) {
                /* can reach here from GF_OPTION_INIT, so cleaning opt_str */
                GF_FREE (option_str);

                for (i = 0; i < subvol_cnt; i++)
                        GF_FREE (priv->qd_vols_conf[i]);
                GF_FREE (priv->qd_vols_conf);

                GF_FREE (priv);
        }
        return ret;
}

int
qd_notify (xlator_t *this, int event, void *data, ...)
{
        xlator_list_t   *subvol                 = NULL;
        xlator_t        *subvol_rec             = NULL;
        quota_priv_t    *priv                   = NULL;
        int              i                      = 0;
        int              ret                    = 0;

        subvol_rec = data;
        priv = this->private;

        for (i=0, subvol = this->children; subvol; i++, subvol = subvol->next) {
                if (! strcmp (priv->qd_vols_conf[i]->name, subvol_rec->name))
                        break;
        }
        if (!subvol) {
                default_notify (this, event, data);
                goto out;
        }

        switch (event) {
        case GF_EVENT_CHILD_UP:
        {
                /* handle spurious CHILD_UP and DOWN events */
                if (!priv->qd_vols_conf[i]->threads_status) {
                        priv->qd_vols_conf[i]->threads_status = _gf_true;

                        ret = qd_start_threads (this, i);
                        if (-1 == ret) {
                                gf_log (this->name, GF_LOG_ERROR, "Couldn't "
                                        "start the threads for volumes");
                                goto out;
                        }
                }
                break;
        }
        case GF_EVENT_CHILD_DOWN:
        {
                gf_log (this->name, GF_LOG_ERROR, "vol %s is down.",
                        priv->qd_vols_conf [i]->name);
                break;
        }
        default:
                default_notify (this, event, data);
        }/* end switch */



out:
        return ret;
}

class_methods_t class_methods = {
        .init           = qd_init,
        .fini           = qd_fini,
        .reconfigure    = qd_reconfigure,
        .notify         = qd_notify,
};

struct xlator_fops fops = {
};

struct xlator_cbks cbks = {
};

struct volume_options options[] = {
        {.key = {"*.limit-set"}},
        {.key = {"*.soft-timeout"},
         .type = GF_OPTION_TYPE_TIME,
         .min = 1,
         .max = LONG_MAX,
         .default_value = "10",
         .description = ""
        },
        {.key = {"*.hard-timeout"},
         .type = GF_OPTION_TYPE_TIME,
         .min = 0,
         .max = LONG_MAX,
         .default_value = "2",
         .description = ""
        },
        {.key = {"*.alert-time"},
         .type = GF_OPTION_TYPE_TIME,
         .min = 0,
         .max = LONG_MAX,
         /* default weekly (7 * 24 * 60 *60) */
         .default_value = "604800",
         .description = ""
        },
        {.key = {"*.default-soft-limit"},
         .type = GF_OPTION_TYPE_PERCENT,
         .min = 0,
         .max = 100,
         .default_value = "90%",
         .description = "Takes this if individual paths are not configured "
                        "with soft limits."
        },
        {.key = {NULL}}
};
