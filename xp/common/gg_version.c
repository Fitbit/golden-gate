/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Michael Hoyt
 *
 * @date 2017-11-30
 *
 * @details
 *
 * Implementation of goldengate library version utility.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_version.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_version_info.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

static const char * const gg_build_date = __DATE__;
static const char * const gg_build_time = __TIME__;

static const char * const gg_git_commit_hash  = GG_GIT_COMMIT_HASH;
static const char * const gg_git_branch_name  = GG_GIT_BRANCH;
static uint32_t           gg_git_commit_count = GG_GIT_COMMIT_COUNT;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_Version(uint16_t*    maj,
           uint16_t*    min,
           uint16_t*    patch,
           uint32_t*    commit_count,
           const char** commit_hash,
           const char** branch_name,
           const char** build_date,
           const char** build_time)
{
    GG_ASSERT(maj);
    GG_ASSERT(min);
    GG_ASSERT(patch);
    GG_ASSERT(commit_hash);
    GG_ASSERT(branch_name);
    GG_ASSERT(build_date);
    GG_ASSERT(build_time);
    GG_ASSERT(commit_count);

    *maj          = GG_MAJOR_VERSION;
    *min          = GG_MINOR_VERSION;
    *patch        = GG_PATCH_VERSION;
    *commit_count = gg_git_commit_count;
    *commit_hash  = gg_git_commit_hash;
    *branch_name  = gg_git_branch_name;
    *build_date   = gg_build_date;
    *build_time   = gg_build_time;
}
