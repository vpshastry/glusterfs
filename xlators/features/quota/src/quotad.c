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
quota_timeout (struct timeval *tv, time_t timeout)
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
qd_resolve_root (xlator_t *this, xlator_t *subvol, loc_t *root_loc,
                 struct iatt *iatt, dict_t **dict_rsp)
{
        int     ret             = -1;
        dict_t  *dict_req       = NULL;

        dict_req = dict_new ();
        if (!dict_req) {
                gf_log (this->name, GF_LOG_WARNING, "Couldn't create dict");
                goto out;
        }

        ret = dict_set_uint64 (dict_req, QUOTA_SIZE_KEY, 0);
        if (ret)
                gf_log (this->name, GF_LOG_ERROR, "Couldn't set dict");

        ret = syncop_lookup (subvol, root_loc, dict_req, iatt, dict_rsp, NULL);
        if (-1 == ret) {
                gf_log (this->name, GF_LOG_ERROR, "Received %s for lookup on /"
                        " (%s)", strerror (errno), subvol->name);
        }
out:
        if (dict_req)
                dict_unref (dict_req);
        return ret;
}

int
qd_build_root_loc (xlator_t *this, xlator_t *subvol, inode_t *inode, loc_t *loc)
{
        struct iatt      buf    = {0,};

        loc->path = gf_strdup ("/");
        loc->inode = inode;
        memset (loc->gfid, 0, 16);
        loc->gfid[15] = 1;

        return qd_resolve_root (this, subvol, loc, &buf, NULL);
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
 * <limit-on-single-dir> = <abs-path-from-volume-root>:<hard-limit>[:<soft-limit-%>]
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

                        soft_l = -1;
                        if (str_val) {
                                ret = gf_string2percent (str_val, &soft_l);
                                if (ret)
                                        gf_log (this->name, GF_LOG_WARNING,
                                                "Failed to convert str to "
                                                "percent. Using default soft "
                                                "limit");
                        }
                        if (-1 == soft_l)
                                soft_l = this_vol->default_soft_lim;

                        quota_lim->soft_lim = (int64_t) quota_lim->hard_lim * (soft_l / 100);

                        quota_lim->path = gf_strdup (path);

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


int
qd_build_loc (loc_t *loc, inode_t *par, char *compnt, int *reval, uuid_t gfid)
{
        int     ret     = 0;

	loc->name = compnt;

	loc->parent = inode_ref (par);
	uuid_copy (loc->pargfid, par->gfid);

        loc->inode = inode_grep (par->table, par, compnt);

	if (loc->inode) {
		uuid_copy (loc->gfid, loc->inode->gfid);
		*reval = 1;
	} else {
		uuid_generate (gfid);
		loc->inode = inode_new (par->table);
	}

	if (!loc->inode)
                ret = -1;
        return ret;
}

int
qd_loc_touchup (loc_t *loc)
{
	char *path = NULL;
	int   ret = -1;
	char *bn = NULL;

	if (loc->parent)
		ret = inode_path (loc->parent, loc->name, &path);
	else
		ret = inode_path (loc->inode, 0, &path);

	if (ret < 0 || !path) {
		ret = -1;
		errno = ENOMEM;
		goto out;
	}

	loc->path = path;

	bn = strrchr (path, '/');
	if (bn)
		bn++;
	loc->name = bn;
	ret = 0;
out:
	return ret;
}

inode_t *
qd_resolve_component (xlator_t *this,xlator_t *subvol, inode_t *par,
                      char *component, struct iatt *iatt, dict_t *xattr_req,
                      dict_t **dict_rsp, int force_lookup)
{
	loc_t        loc        = {0, };
	inode_t     *inode      = NULL;
	int          reval      = 0;
	int          ret        = -1;
	struct iatt  ciatt      = {0, };


	loc.name = component;

	loc.parent = inode_ref (par);
	uuid_copy (loc.pargfid, par->gfid);

        loc.inode = inode_grep (par->table, par, component);

	if (loc.inode) {
		uuid_copy (loc.gfid, loc.inode->gfid);
		reval = 1;

                if (!force_lookup) {
                        inode = inode_ref (loc.inode);
                        ciatt.ia_type = inode->ia_type;
                        goto found;
                }
	} else {
		loc.inode = inode_new (par->table);
	}

	if (!loc.inode)
                goto out;

	ret = qd_loc_touchup (&loc);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = syncop_lookup (subvol, &loc, xattr_req, &ciatt, dict_rsp, NULL);
	if (ret && reval) {
                switch (errno) {
                case ENOENT:
                        inode_unlink (loc.inode, par, component);
                case ENOTCONN:
                        goto out;
                }

		inode_unlink (loc.inode, par, component);

		loc.inode = inode_new (par->table);
		if (!loc.inode) {
                        errno = ENOMEM;
			goto out;
                }

                if (*dict_rsp) {
                        ret = dict_reset (*dict_rsp);
                        if (ret) {
                                gf_log (this->name, GF_LOG_DEBUG,
                                        "Couldn't reset dict");
                                goto out;
                        }
                }

		ret = syncop_lookup (subvol, &loc, xattr_req, &ciatt,
				     dict_rsp, NULL);
	}
	if (ret)
		goto out;

	inode = inode_link (loc.inode, loc.parent, component, &ciatt);
found:
	if (inode)
		inode_lookup (inode);
	if (iatt)
		*iatt = ciatt;
out:
        if (xattr_req)
                dict_unref (xattr_req);

	loc_wipe (&loc);

	return inode;
}

int
qd_resolve_path (xlator_t *this, xlator_t *subvol, qd_vols_conf_t *this_vol,
                 limits_t *entry, loc_t *root_loc, loc_t *entry_loc,
                 dict_t *dict_req, dict_t **dict_rsp, int reval)
{
        char                    *component      = NULL;
        char                    *next_component = NULL;
        char                    *saveptr        = NULL;
        char                    *path           = NULL;
        int                      ret            = -1;
        struct iatt              piatt          = {0,};
        inode_t                 *parent         = NULL;
        inode_t                 *inode          = NULL;
        int                      ret_val        = -1;

        inode = inode_ref (root_loc->inode);

        /* Lookup on / is sent while building root_loc */
        if (0 == strcmp (entry->path, "/")) {
                ret_val = loc_copy (entry_loc, root_loc);
                if (ret_val) {
                        gf_log (this->name, GF_LOG_ERROR, "Failed to copy loc");
                        inode_unref (inode);
                        goto out;
                }

                ret_val = qd_resolve_root (this, subvol, entry_loc, &piatt, dict_rsp);

                inode_unref (inode);
                goto out;
        }

        path = gf_strdup (entry->path);
	for (component = strtok_r (path, "/", &saveptr);
	     component; component = next_component) {

		next_component = strtok_r (NULL, "/", &saveptr);

		if (parent)
			inode_unref (parent);

		parent = inode;

                /* Get the xattrs in lookup for the last component */
                if (!next_component) {
                        ret = dict_set_uint64 (dict_req, QUOTA_SIZE_KEY, 0);
                        if (ret)
                                gf_log (this->name, GF_LOG_ERROR,
                                        "Couldn't set dict");
                }

                /* Hold a ref on the dict. Because in qd_resolve_component,
                   it sends a lookup and after the lookup the dictinary is
                   unrefed, which means the dict will get destroyed.
                */
                dict_ref (dict_req);
		inode = qd_resolve_component (this, subvol, parent, component,
                                              &piatt, dict_req, dict_rsp,
                                              (reval || !next_component));
		if (!inode)
			break;

		if (!next_component)
			break;

		if (!IA_ISDIR (piatt.ia_type)) {
			/* next_component exists and this component is
			   not a directory
			*/
			inode_unref (inode);
			inode = NULL;
			ret_val = -1;
			errno = ENOTDIR;
			break;
		}

                ret = dict_reset (dict_req);
                if (ret) {
                        gf_log (this->name, GF_LOG_WARNING, "dict reset failed");
                        ret_val = -1;
                        errno = EINVAL; //Is this errno ok?
                        goto out;
                }
	}

	if (parent && next_component)
		goto out;

	entry_loc->parent = parent;
	if (parent)
		uuid_copy (entry_loc->pargfid, parent->gfid);

	entry_loc->inode = inode;
	if (inode) {
		uuid_copy (entry_loc->gfid, inode->gfid);
		ret_val = 0;
	}

	qd_loc_touchup (entry_loc);
out:
	GF_FREE (path);

	return ret_val;
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

        if (DID_CROSS_SOFT_LIMIT (entry->soft_lim, entry->prev_size, cur_size)) {
                entry->prev_log_tv = cur_time;
                gf_log (this->name, GF_LOG_ALERT, "Usage crossed soft limit:"
                        " %ld for %s", entry->soft_lim, entry->path);
        }
        if (cur_size > entry->soft_lim &&
                   quota_timeout (&entry->prev_log_tv, this_vol->log_timeout)) {
                entry->prev_log_tv = cur_time;
                gf_log (this->name, GF_LOG_ALERT, "Usage %ld %s limit for %s",
                        cur_size,
                        (cur_size > entry->hard_lim)? "has reached hard": "is above soft",
                        entry->path);
        }
}


int
qd_resolve_entry (xlator_t *this, limits_t *entry, loc_t *root_loc,
                  xlator_t *subvol, qd_vols_conf_t *this_vol, dict_t *dict_req,
                  dict_t **dict, loc_t *entry_loc, int reval)
{
        int32_t          ret            = -1;

        /* reval = 0: fresh lookup
           reval = 1: revalidate lookup
           reval > 1: revalidate done.
        */
        if (reval > 1)
                return ret;

        ret = qd_resolve_path (this, subvol, this_vol,
                               entry, root_loc, entry_loc, dict_req, dict,
                               reval);
        if (-1 == ret) {
                reval++;
                gf_log (this->name, GF_LOG_WARNING,
                        "Received %s error for %s (%s)",
                        strerror (errno), entry->path, this_vol->name);
                ret = qd_resolve_entry (this, entry, root_loc, subvol,
                                        this_vol, dict_req, dict, entry_loc,
                                        reval);
        }

        return ret;
}


int
qd_updatexattr (xlator_t *this, struct limits_level *list, loc_t *root_loc,
                xlator_t *subvol)
{
        limits_t        *entry          = NULL;
        limits_t        *next           = NULL;
        int32_t          ret            = -1;
        int              reval          = 0;
        loc_t            entry_loc      = {0,};
        qd_vols_conf_t  *this_vol       = NULL;
        dict_t          *dict           = NULL;
        int64_t         *size           = NULL;
        int64_t          cur_size       = 0;
        int64_t          prev_size      = 0;
        dict_t          *setxattr_dict  = NULL;
        dict_t          *dict_req       = NULL;

        this_vol = GET_THIS_VOL (list);

        dict_req = dict_new ();
        if (!dict_req) {
                ret = -1;
                goto out;
        }

        setxattr_dict = dict_new ();
        if (!setxattr_dict) {
                ret = -1;
                goto out;
        }

        list_for_each_entry_safe (entry, next, &list->limit_head, limit_list) {
                if (dict) {
                        dict_unref (dict);
                        dict = NULL;
                }
                loc_wipe (&entry_loc);
                reval = 0;
                ret = qd_resolve_entry (this, entry, root_loc, subvol, this_vol,
                                        dict_req, &dict, &entry_loc, reval);
                if (ret) {
                        if (errno == ENOENT || errno == ENOTDIR)
                                entry->prev_size = entry->hard_lim;
                        gf_log (this->name, GF_LOG_ERROR, "resolving the entry "
                                "%s failed (%s)", entry->path, strerror (errno));
                        continue;
                }

                ret = dict_get_bin (dict, QUOTA_SIZE_KEY, (void **)&size);
                if (0 != ret) {
                        gf_log (this->name, GF_LOG_WARNING, "Couldn't get size"
                                " from the dict (%s)", entry->path);
                        continue;
                }

                cur_size = ntoh64 (*size);

                qd_log_usage (this, this_vol, entry, cur_size);

                /* if size hasn't changed, just skip the updation */
                if (entry->prev_size == cur_size)
                        continue;

                prev_size = entry->prev_size;

                QUOTA_ALLOC_OR_GOTO (size, int64_t, out);
                *size = hton64 (cur_size);

                ret = dict_set_bin (setxattr_dict, QUOTA_UPDATE_USAGE_KEY, size,
                                    sizeof (int64_t));
                if (-1 == ret) {
                        gf_log (this->name, GF_LOG_WARNING,
                                "Couldn't set dict");
                        continue;
                }


                /* There is a possibility that, after a setxattr is done,
                   a rename might happen and the resolve will fail again.
                */
                ret = syncop_setxattr (subvol, &entry_loc, setxattr_dict, 0);
                if (-1 == ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "Received ERROR:%s in updating quota value %s "
                                " (vol:%s). Quota enforcement may not be"
                                " accurate", strerror (errno), entry->path,
                                this_vol->name);
                        continue;
                }

                /* Move the limit-node to the corresponding list,
                 * based on the usage */
                LOCK (&this_vol->lock);
                {
                        /* usage > soft_limit? */
                        if (prev_size < entry->soft_lim &&
                            cur_size >= entry->soft_lim)
                                list_move (&entry->limit_list,
                                           &(this_vol->above_soft.limit_head));
                        /* usage < soft_limit? */
                        else if (prev_size >= entry->soft_lim &&
                                 cur_size < entry->soft_lim)
                                list_move (&entry->limit_list,
                                           &(this_vol->below_soft.limit_head));

                        entry->prev_size = cur_size;
                }
                UNLOCK (&this_vol->lock);
        }

out:
        if (setxattr_dict)
                dict_unref (setxattr_dict);
        if (dict_req)
                dict_unref (dict_req);
        if (dict)
                dict_unref (dict);
        loc_wipe (&entry_loc);
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
        loc_t                    root_loc       = {0,};

        this = THIS;
        list = args;

        subvol = qd_get_subvol (this, GET_THIS_VOL (list));
        if (!subvol) {
                gf_log (this->name, GF_LOG_ERROR, "No subvol found");
                return -1;
        }

        root_inode = qd_build_root_inode (this, GET_THIS_VOL (list));
        if (!root_inode) {
                gf_log (this->name, GF_LOG_ERROR, "New itable creation failed");
                return -1;
        }

        ret = qd_build_root_loc (this, subvol, root_inode, &root_loc);
        if (ret) {
                gf_log (this->name, GF_LOG_ERROR, "Failed to build root_loc "
                        "for %s", GET_THIS_VOL (list)->name);
                goto out;
        }

        while (1) {
                if (!list_empty (&list->limit_head)) {
                        ret = qd_updatexattr (this, list, &root_loc, subvol);
                        if (-1 == ret)
                                gf_log (this->name, GF_LOG_WARNING,
                                        "Couldn't update the usage, frequent "
                                        "log may lead usage to cross beyond "
                                        "limit");
                }

                sleep ((unsigned int) (list->time_out));
        }

out:
        loc_wipe (&root_loc);
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
                        (GET_THIS_VOL ((struct limits_level *)args))->name);

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
        char            *alert_time_str = NULL;

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

        priv->qd_vols_conf = GF_CALLOC (sizeof (qd_vols_conf_t *),
                                        subvol_cnt, gf_quota_mt_qd_vols_conf_t);
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

                ret = gf_asprintf (&option_str, "%s.default-soft-limit", this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->default_soft_lim,
                                percent, err);
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.alert-time", this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, alert_time_str, str, err);
                /* Don't free up alert_time_str */
                ret = gf_string2time (alert_time_str,
                                      (uint32_t *)&this_vol->log_timeout);
                if (ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "Couldn't convert time in str to int");
                        goto err;
                }
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

                ret = gf_asprintf (&option_str, "%s.soft-timeout", this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->below_soft.time_out,
                                uint64, err);
                GF_FREE (option_str);

                ret = gf_asprintf (&option_str, "%s.hard-timeout", this_vol->name);
                if (0 > ret) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "gf_asprintf failed");
                        goto err;
                }
                GF_OPTION_INIT (option_str, this_vol->above_soft.time_out,
                                uint64, err);
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
                if (0 == strcmp (priv->qd_vols_conf[i]->name, subvol_rec->name))
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
                                gf_log (this->name, GF_LOG_ERROR,
                                        "Couldn't start the threads for volumes");
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
         .type = GF_OPTION_TYPE_SIZET,
         .min = 1,
         .max = LONG_MAX,
         .default_value = "10",
         .description = ""
        },
        {.key = {"*.hard-timeout"},
         .type = GF_OPTION_TYPE_SIZET,
         .min = 0,
         .max = LONG_MAX,
         .default_value = "2",
         .description = ""
        },
        {.key = {"*.alert-time"},
         .type = GF_OPTION_TYPE_SIZET,
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
