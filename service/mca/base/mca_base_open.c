/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2008 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2011      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "service/platform/ccs_config.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "service/util/service_environ.h"
#include "service/util/output.h"
#include "service/util/printf.h"
#include "service/mca/mca.h"
#include "service/mca/base/base.h"
#include "service/mca/base/mca_base_component_repository.h"
#include "service/platform/service_constants.h"


/*
 * Public variables
 */
int ccs_mca_base_param_component_path = -1;
bool ccs_mca_base_opened = false;
char *ccs_mca_base_system_default_path=NULL;
char *ccs_mca_base_user_default_path=NULL;

/*
 * Private functions
 */
static void set_defaults(service_output_stream_t *lds);
static void parse_verbose(char *e, service_output_stream_t *lds);
ccs_mca_service_install_dirs_t service_install_dirs = {"","",""};

/*
 * Main MCA initialization.  
 */
int ccs_mca_base_open(ccs_mca_service_install_dirs_t install_dirs)
{
  char *value;
  service_output_stream_t lds;
  char hostname[64];

  if (!ccs_mca_base_opened) {
    ccs_mca_base_opened = true;
  } else {
    return CCS_SUCCESS;
  }

  service_install_dirs = install_dirs;
    /* define the system and user default paths */
#if CCS_WANT_HOME_CONFIG_FILES
    ccs_mca_base_system_default_path = strdup(service_install_dirs.pkglibdir);
    asprintf(&ccs_mca_base_user_default_path, "%s"CCS_PATH_SEP".llc"CCS_PATH_SEP"components", service_home_directory());
#else
# if defined(__WINDOWS__) && defined(_DEBUG)
    asprintf(&ccs_mca_base_system_default_path, "%s/debug", ccs_install_dirs.pkglibdir); 
# else
    asprintf(&ccs_mca_base_system_default_path, "%s", ccs_install_dirs.pkglibdir); 
# endif
#endif

    /* see if the user wants to override the defaults */
    if (NULL == ccs_mca_base_user_default_path) {
        value = strdup(ccs_mca_base_system_default_path);
    } else {
        asprintf(&value, "%s%c%s", ccs_mca_base_system_default_path,
                 CCS_ENV_SEP, ccs_mca_base_user_default_path);
    }
  ccs_mca_base_param_component_path = 
    ccs_mca_base_param_reg_string_name("mca", "component_path",
                                   "Path where to look for Open MPI and ORTE components", 
                                   false, false, value, NULL);
  free(value);
  
  ccs_mca_base_param_reg_int_name("mca", "component_show_load_errors", 
                              "Whether to show errors for components that failed to load or not", 
                              false, false, 1, NULL);

  ccs_mca_base_param_reg_int_name("mca", "component_disable_dlopen",
                              "Whether to attempt to disable opening dynamic components or not",
                              false, false, 0, NULL);

  /* What verbosity level do we want for the default 0 stream? */

  ccs_mca_base_param_reg_string_name("mca", "verbose", 
                                 "Specifies where the default error output stream goes (this is separate from distinct help messages).  Accepts a comma-delimited list of: stderr, stdout, syslog, syslogpri:<notice|info|debug>, syslogid:<str> (where str is the prefix string for all syslog notices), file[:filename] (if filename is not specified, a default filename is used), fileappend (if not specified, the file is opened for truncation), level[:N] (if specified, integer verbose level; otherwise, 0 is implied)",
                                 false, false, "stderr", &value);
  memset(&lds, 0, sizeof(lds));
  if (NULL != value) {
    parse_verbose(value, &lds);
    free(value);
  } else {
    set_defaults(&lds);
  }
  gethostname(hostname, 64);
  asprintf(&lds.lds_prefix, "[%s:%05d] ", hostname, getpid());
  service_output_reopen(0, &lds);
  service_output_verbose(5, 0, "mca: base: opening components");
  free(lds.lds_prefix);

  /* Open up the component repository */

  return ccs_mca_base_component_repository_init();
}


/*
 * Set sane default values for the lds
 */
static void set_defaults(service_output_stream_t *lds)
{

    /* Load up defaults */

    OBJ_CONSTRUCT(lds, service_output_stream_t);
#ifndef __WINDOWS__
    lds->lds_syslog_priority = LOG_INFO;
#endif
    lds->lds_syslog_ident = "ompi";
    lds->lds_want_stderr = true;
}


/*
 * Parse the value of an environment variable describing verbosity
 */
static void parse_verbose(char *e, service_output_stream_t *lds)
{
  char *edup;
  char *ptr, *next;
  bool have_output = false;

  if (NULL == e) {
    return;
  }

  edup = strdup(e);
  ptr = edup;

  /* Now parse the environment variable */

  while (NULL != ptr && strlen(ptr) > 0) {
    next = strchr(ptr, ',');
    if (NULL != next) {
      *next = '\0';
    }

    if (0 == strcasecmp(ptr, "syslog")) {
#ifndef __WINDOWS__  /* there is no syslog */
		lds->lds_want_syslog = true;
      have_output = true;
    }
    else if (strncasecmp(ptr, "syslogpri:", 10) == 0) {
      lds->lds_want_syslog = true;
      have_output = true;
      if (strcasecmp(ptr + 10, "notice") == 0)
	lds->lds_syslog_priority = LOG_NOTICE;
      else if (strcasecmp(ptr + 10, "INFO") == 0)
	lds->lds_syslog_priority = LOG_INFO;
      else if (strcasecmp(ptr + 10, "DEBUG") == 0)
	lds->lds_syslog_priority = LOG_DEBUG;
    } else if (strncasecmp(ptr, "syslogid:", 9) == 0) {
      lds->lds_want_syslog = true;
      lds->lds_syslog_ident = ptr + 9;
#endif
    }

    else if (strcasecmp(ptr, "stdout") == 0) {
      lds->lds_want_stdout = true;
      have_output = true;
    } else if (strcasecmp(ptr, "stderr") == 0) {
      lds->lds_want_stderr = true;
      have_output = true;
    }

    else if (strcasecmp(ptr, "file") == 0) {
      lds->lds_want_file = true;
      have_output = true;
    } else if (strncasecmp(ptr, "file:", 5) == 0) {
      lds->lds_want_file = true;
      lds->lds_file_suffix = ptr + 5;
      have_output = true;
    } else if (strcasecmp(ptr, "fileappend") == 0) {
      lds->lds_want_file = true;
      lds->lds_want_file_append = 1;
      have_output = true;
    } 

    else if (strncasecmp(ptr, "level", 5) == 0) {
      lds->lds_verbose_level = 0;
      if (ptr[5] == CCS_ENV_SEP)
        lds->lds_verbose_level = atoi(ptr + 6);
    }

    if (NULL == next) {
      break;
    }
    ptr = next + 1;
  }

  /* If we didn't get an output, default to stderr */

  if (!have_output) {
    lds->lds_want_stderr = true;
  }

  /* All done */

  free(edup);
}
