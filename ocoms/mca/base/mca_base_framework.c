/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ocoms/platform/ocoms_config.h"

#include "ocoms/platform/ocoms_constants.h"
#include "ocoms/util/output.h"

#include "mca_base_framework.h"
#include "mca_base_var.h"
#include "ocoms/mca/base/base.h"

static bool framework_is_registered (struct ocoms_mca_base_framework_t *framework)
{
    return !!(framework->framework_flags & MCA_BASE_FRAMEWORK_FLAG_REGISTERED);
}

static void framework_open_output (struct ocoms_mca_base_framework_t *framework)
{
    if (0 < framework->framework_verbose) {
        if (-1 == framework->framework_output) {
            framework->framework_output = ocoms_output_open (NULL);
        }
        ocoms_output_set_verbosity(framework->framework_output,
                                  framework->framework_verbose);
    } else if (-1 != framework->framework_output) {
        ocoms_output_close (framework->framework_output);
        framework->framework_output = -1;
    }
}

static void framework_close_output (struct ocoms_mca_base_framework_t *framework)
{
    if (-1 != framework->framework_output) {
        ocoms_output_close (framework->framework_output);
        framework->framework_output = -1;
    }
}

int ocoms_mca_base_framework_register (struct ocoms_mca_base_framework_t *framework,
                                 ocoms_mca_base_register_flag_t flags)
{
    char *desc;
    int ret;

    assert (NULL != framework);

    if (framework_is_registered (framework)) {
        return OCOMS_SUCCESS;
    }

    if (!(MCA_BASE_FRAMEWORK_FLAG_NOREGISTER & framework->framework_flags)) {
        /* register this framework with the MCA variable system */
        ret = ocoms_mca_base_var_group_register (framework->framework_project,
                                           framework->framework_name,
                                           NULL, framework->framework_description);
        if (0 > ret) {
            return ret;
        }

        asprintf (&desc, "Default selection set of components for the %s framework (<none>"
                  " means use all components that can be found)", framework->framework_name);
        ret = ocoms_mca_base_var_register (framework->framework_project, framework->framework_name,
                                     NULL, NULL, desc, MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                     MCA_BASE_VAR_FLAG_SETTABLE, OCOMS_INFO_LVL_2,
                                     MCA_BASE_VAR_SCOPE_ALL_EQ, &framework->framework_selection);
        free (desc);
        if (0 > ret) {
            return ret;
        }

        /* register a verbosity variable for this framework */
        asprintf (&desc, "Verbosity level for the %s framework (0 = no verbosity)",
                  framework->framework_name);
        ret = ocoms_mca_base_framework_var_register (framework, "verbose", desc,
                                               MCA_BASE_VAR_TYPE_INT, NULL, 0,
                                               MCA_BASE_VAR_FLAG_SETTABLE,
                                               OCOMS_INFO_LVL_8,
                                               MCA_BASE_VAR_SCOPE_LOCAL,
                                               &framework->framework_verbose);                                           
        free(desc);
        if (0 > ret) {
            return ret;
        }

        /* check the initial verbosity and open the output if necessary. we
           will recheck this on open */
        framework_open_output (framework);

        /* register framework variables */
        if (NULL != framework->framework_register) {
            ret = framework->framework_register (flags);
            if (OCOMS_SUCCESS != ret) {
                return ret;
            }
        }

        /* register components variables */
        ret = ocoms_mca_base_framework_components_register (framework, flags);
        if (OCOMS_SUCCESS != ret) {
            return ret;
        }
    }

    framework->framework_flags |= MCA_BASE_FRAMEWORK_FLAG_REGISTERED;

    /* framework did not provide a register function */
    return OCOMS_SUCCESS;
}

int ocoms_mca_base_framework_open (struct ocoms_mca_base_framework_t *framework,
                             ocoms_mca_base_open_flag_t flags) {
    int ret;

    assert (NULL != framework);

    /* check if this framework is already open */
    if (framework->framework_refcnt++) {
        return OCOMS_SUCCESS;
    }

    /* register this framework before opening it */
    ret = ocoms_mca_base_framework_register (framework, MCA_BASE_REGISTER_DEFAULT);
    if (OCOMS_SUCCESS != ret) {
        return ret;
    }

    if (MCA_BASE_FRAMEWORK_FLAG_NOREGISTER & framework->framework_flags) {
        flags |= MCA_BASE_OPEN_FIND_COMPONENTS;
    }

    /* lock all of this frameworks's variables */
    ret = ocoms_mca_base_var_group_find (framework->framework_project,
                                   framework->framework_name,
                                   NULL);
    ocoms_mca_base_var_group_set_var_flag (ret, MCA_BASE_VAR_FLAG_SETTABLE, false);

    /* check the verbosity level and open (or close) the output */
    framework_open_output (framework);

    if (NULL != framework->framework_open) {
        ret = framework->framework_open (flags);
    } else {
        ret = ocoms_mca_base_framework_components_open (framework, flags);
    }

    if (OCOMS_SUCCESS != ret) {
        framework->framework_refcnt = 0;
    }

    return ret;
}

int ocoms_mca_base_framework_close (struct ocoms_mca_base_framework_t *framework) {
    bool is_open = !!framework->framework_refcnt;
    int ret, group_id;

    assert (NULL != framework);

    if (!framework_is_registered (framework) && 0 == framework->framework_refcnt) {
        return OCOMS_SUCCESS;
    }

    if (framework->framework_refcnt && --framework->framework_refcnt) {
        return OCOMS_SUCCESS;
    }

    /* find and deregister all component groups and variables */
    group_id = ocoms_mca_base_var_group_find (framework->framework_project,
                                        framework->framework_name, NULL);
    if (0 <= group_id) {
        ret = ocoms_mca_base_var_group_deregister (group_id);
        framework->framework_flags &= ~MCA_BASE_FRAMEWORK_FLAG_REGISTERED;
    }

    /* close the framework and all of its components */
    if (is_open) {
        if (NULL != framework->framework_close) {
            ret = framework->framework_close ();
        } else {
            ret = ocoms_mca_base_framework_components_close (framework, NULL);
        }

        if (OCOMS_SUCCESS != ret) {
            return ret;
        }
    } else {
        ocoms_list_item_t *item;
        while (NULL != (item = ocoms_list_remove_first (&framework->framework_components))) {
            ocoms_mca_base_component_unload ((ocoms_mca_base_component_t *) item, framework->framework_output);
            OBJ_RELEASE(item);
        }
        ret = OCOMS_SUCCESS;
    }

    framework->framework_flags &= ~MCA_BASE_FRAMEWORK_FLAG_REGISTERED;

    framework_close_output (framework);

    return ret;
}
