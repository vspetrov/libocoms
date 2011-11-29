%{ /* -*- C -*- */
/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ccs_config.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "service/util/keyval/keyval_lex.h"

/*
 * local functions
 */

BEGIN_C_DECLS

static int finish_parsing(void);
int service_util_keyval_yywrap(void);

END_C_DECLS

/*
 * global variables
 */
int service_util_keyval_yynewlines = 1;
bool service_util_keyval_parse_done = false;
char *service_util_keyval_string = NULL;

#define yyterminate() \
  return finish_parsing()

%}

WHITE       [\f\t\v ]
CHAR        [A-Za-z0-9_\-\.]

%x VALUE
%x comment

%%

{WHITE}*\n          { service_util_keyval_yynewlines++; return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }
#.*\n               { service_util_keyval_yynewlines++; return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }
"//".*\n            { service_util_keyval_yynewlines++; return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }

"/*"                { BEGIN(comment);
                      return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }
<comment>[^*\n]*       ; /* Eat up non '*'s */
<comment>"*"+[^*/\n]*  ; /* Eat '*'s not followed by a '/' */
<comment>\n         { service_util_keyval_yynewlines++;
                      return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; } 
<comment>"*"+"/"    { BEGIN(INITIAL); /* Done with Block Comment */
                      return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }

{WHITE}*"="{WHITE}* { BEGIN(VALUE); return SERVICE_UTIL_KEYVAL_PARSE_EQUAL; }
{WHITE}+            ; /* whitespace */
{CHAR}+             { return SERVICE_UTIL_KEYVAL_PARSE_SINGLE_WORD; }

<VALUE>{WHITE}*\n   { BEGIN(INITIAL); return SERVICE_UTIL_KEYVAL_PARSE_NEWLINE; }
<VALUE>[^\n]*[^\t \n]/[\t ]*      { return SERVICE_UTIL_KEYVAL_PARSE_VALUE; }

.	            { return SERVICE_UTIL_KEYVAL_PARSE_ERROR; }

%%


/*
 * This cleans up at the end of the parse (since, in this case, we
 * always parse the entire file) and prevents a memory leak.
 */
static int finish_parsing(void) 
{
    if (NULL != YY_CURRENT_BUFFER) {
        yy_delete_buffer(YY_CURRENT_BUFFER); 
#if defined(YY_CURRENT_BUFFER_LVALUE)
        YY_CURRENT_BUFFER_LVALUE = NULL;
#else
        YY_CURRENT_BUFFER = NULL;
#endif  /* YY_CURRENT_BUFFER_LVALUE */
    }
    return YY_NULL;
}

int service_util_keyval_yywrap(void)
{
    service_util_keyval_parse_done = true;
    return 1;
}

/*
 * Ensure that we have a valid yybuffer to use.  Specifically, if this
 * scanner is invoked a second time, finish_parsing() (above) will
 * have been executed, and the current buffer will have been freed.
 * Flex doesn't recognize this fact because as far as it's concerned,
 * its internal state was already initialized, so it thinks it should
 * have a valid buffer.  Hence, here we ensure to give it a valid
 * buffer.
 */
int service_util_keyval_init_buffer(FILE *file)
{
    YY_BUFFER_STATE buf = yy_create_buffer(file, YY_BUF_SIZE);
    yy_switch_to_buffer(buf);

    return 0;
}