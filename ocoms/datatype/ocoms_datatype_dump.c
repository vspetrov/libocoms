/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2009 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2006 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2009      Oak Ridge National Labs.  All rights reserved.
 * Copyright (c) 2011-2013 UT-Battelle, LLC. All rights reserved.
 * Copyright (C) 2013      Mellanox Technologies Ltd. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ocoms/platform/ocoms_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "ocoms/platform/ocoms_constants.h"
#include "ocoms/util/output.h"
#include "ocoms/datatype/ocoms_datatype.h"
#include "ocoms/datatype/ocoms_datatype_internal.h"

/********************************************************
 * Data dumping functions
 ********************************************************/

int ocoms_datatype_contain_basic_datatypes( const ocoms_datatype_t* pData, char* ptr, size_t length )
{
    int i;
    int32_t index = 0;
    uint64_t mask = 1;

    if( pData->flags & OCOMS_DATATYPE_FLAG_USER_LB ) index += snprintf( ptr, length - index, "lb " );
    if( pData->flags & OCOMS_DATATYPE_FLAG_USER_UB ) index += snprintf( ptr + index, length - index, "ub " );
    for( i = 0; i < OCOMS_DATATYPE_MAX_PREDEFINED; i++ ) {
        if( pData->bdt_used & mask )
            index += snprintf( ptr + index, length - index, "%s ", ocoms_datatype_basicDatatypes[i]->name );
        mask <<= 1;
        if( length <= (size_t)index ) break;
    }
    return index;
}

int ocoms_datatype_dump_data_flags( unsigned short usflags, char* ptr, size_t length )
{
    int index = 0;
    if( length < 22 ) return 0;
    index = snprintf( ptr, 22, "-----------[---][---]" );  /* set everything to - */
    if( usflags & OCOMS_DATATYPE_FLAG_COMMITED )   ptr[1]  = 'c';
    if( usflags & OCOMS_DATATYPE_FLAG_CONTIGUOUS ) ptr[2]  = 'C';
    if( usflags & OCOMS_DATATYPE_FLAG_OVERLAP )    ptr[3]  = 'o';
    if( usflags & OCOMS_DATATYPE_FLAG_USER_LB )    ptr[4]  = 'l';
    if( usflags & OCOMS_DATATYPE_FLAG_USER_UB )    ptr[5]  = 'u';
    if( usflags & OCOMS_DATATYPE_FLAG_PREDEFINED ) ptr[6]  = 'P';
    if( !(usflags & OCOMS_DATATYPE_FLAG_NO_GAPS) ) ptr[7]  = 'G';
    if( usflags & OCOMS_DATATYPE_FLAG_DATA )       ptr[8]  = 'D';
    if( (usflags & OCOMS_DATATYPE_FLAG_BASIC) == OCOMS_DATATYPE_FLAG_BASIC ) ptr[9]  = 'B';
    /* We know nothing about the upper level language! This is part of _ompi_dump_data_flags */
    /* ... */
    return index;
}


int ocoms_datatype_dump_data_desc( dt_elem_desc_t* pDesc, int nbElems, char* ptr, size_t length )
{
    int i;
    int32_t index = 0;

    for( i = 0; i < nbElems; i++ ) {
        index += ocoms_datatype_dump_data_flags( pDesc->elem.common.flags, ptr + index, length );
        if( length <= (size_t)index ) break;
        index += snprintf( ptr + index, length - index, "%15s ", ocoms_datatype_basicDatatypes[pDesc->elem.common.type]->name );
        if( length <= (size_t)index ) break;
        if( OCOMS_DATATYPE_LOOP == pDesc->elem.common.type )
            index += snprintf( ptr + index, length - index, "%d times the next %d elements extent %d\n",
                               (int)pDesc->loop.loops, (int)pDesc->loop.items,
                               (int)pDesc->loop.extent );
        else if( OCOMS_DATATYPE_END_LOOP == pDesc->elem.common.type )
            index += snprintf( ptr + index, length - index, "prev %d elements first elem displacement %ld size of data %d\n",
                           (int)pDesc->end_loop.items, (long)pDesc->end_loop.first_elem_disp,
                           (int)pDesc->end_loop.size );
        else
            index += snprintf( ptr + index, length - index, "count %d disp 0x%lx (%ld) extent %d (size %ld)\n",
                               (int)pDesc->elem.count, (long)pDesc->elem.disp, (long)pDesc->elem.disp,
                               (int)pDesc->elem.extent, (long)(pDesc->elem.count * ocoms_datatype_basicDatatypes[pDesc->elem.common.type]->size) );
        pDesc++;

        if( length <= (size_t)index ) break;
    }
    return index;
}


void ocoms_datatype_dump_v2( const ocoms_datatype_t* pData, bool dump_to_stderr)
{
    size_t length;
    int index = 0;
    char* buffer;

    length = pData->opt_desc.used + pData->desc.used;
    length = length * 100 + 500;
    buffer = (char*)malloc( length );
    index += snprintf( buffer, length - index, "Datatype %p[%s] size %ld align %d id %d length %d used %d\n"
                                               "true_lb %ld true_ub %ld (true_extent %ld) lb %ld ub %ld (extent %ld)\n"
                                               "nbElems %d loops %d flags %X (",
                     (void*)pData, pData->name, (long)pData->size, (int)pData->align, pData->id, (int)pData->desc.length, (int)pData->desc.used,
                     (long)pData->true_lb, (long)pData->true_ub, (long)(pData->true_ub - pData->true_lb),
                     (long)pData->lb, (long)pData->ub, (long)(pData->ub - pData->lb),
                     (int)pData->nbElems, (int)pData->btypes[OCOMS_DATATYPE_LOOP], (int)pData->flags );
    /* dump the flags */
    if( pData->flags == OCOMS_DATATYPE_FLAG_PREDEFINED )
        index += snprintf( buffer + index, length - index, "predefined " );
    else {
        if( pData->flags & OCOMS_DATATYPE_FLAG_COMMITED ) index += snprintf( buffer + index, length - index, "commited " );
        if( pData->flags & OCOMS_DATATYPE_FLAG_CONTIGUOUS) index += snprintf( buffer + index, length - index, "contiguous " );
    }
    index += snprintf( buffer + index, length - index, ")" );
    index += ocoms_datatype_dump_data_flags( pData->flags, buffer + index, length - index );
    {
        index += snprintf( buffer + index, length - index, "\n   contain " );
        index += ocoms_datatype_contain_basic_datatypes( pData, buffer + index, length - index );
        index += snprintf( buffer + index, length - index, "\n" );
    }
    if( (pData->opt_desc.desc != pData->desc.desc) && (NULL != pData->opt_desc.desc) ) {
        /* If the data is already committed print everything including the last
         * fake OCOMS_DATATYPE_END_LOOP entry.
         */
        index += ocoms_datatype_dump_data_desc( pData->desc.desc, pData->desc.used + 1, buffer + index, length - index );
        index += snprintf( buffer + index, length - index, "Optimized description \n" );
        index += ocoms_datatype_dump_data_desc( pData->opt_desc.desc, pData->opt_desc.used + 1, buffer + index, length - index );
    } else {
        index += ocoms_datatype_dump_data_desc( pData->desc.desc, pData->desc.used, buffer + index, length - index );
        index += snprintf( buffer + index, length - index, "No optimized description\n" );
    }
    buffer[index] = '\0';  /* make sure we end the string with 0 */
    if (dump_to_stderr) {
        fprintf(stderr, "%s\n", buffer);
    } else {
        DUMP("%s\n", buffer );
    }

    free(buffer);
}

void ocoms_datatype_dump( const ocoms_datatype_t* pData) {
    return ocoms_datatype_dump_v2(pData, false);
}
