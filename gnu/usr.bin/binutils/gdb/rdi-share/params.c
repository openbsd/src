/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 *
 *   Project: ANGEL
 *
 *     Title: Parameter negotiation utility functions
 */

#include "params.h"

#include "angel_endian.h"
#include "logging.h"


/*
 * Function: Angel_FindParam
 *  Purpose: find the value of a given parameter from a config.
 *
 * see params.h for details
 */
bool Angel_FindParam( ADP_Parameter          type,
                      const ParameterConfig *config,
                      unsigned int          *value )
{
    unsigned int i;

    for ( i = 0; i < config->num_parameters; ++i )
       if ( config->param[i].type == type )
       {
           *value = config->param[i].value;
           return TRUE;
       }

    return FALSE;
}


#if !defined(TARGET) || !defined(MINIMAL_ANGEL) || MINIMAL_ANGEL == 0

ParameterList *Angel_FindParamList( const ParameterOptions *options,
                                    ADP_Parameter           type )
{
    unsigned int i;

    for ( i = 0; i < options->num_param_lists; ++i )
       if ( options->param_list[i].type == type )
          return &options->param_list[i];

    return NULL;
}


#if defined(TARGET) || defined(TEST_PARAMS)
/*
 * Function: Angel_MatchParams
 *  Purpose: find a configuration from the requested options which is
 *           the best match from the supported options.
 *
 * see params.h for details
 */
const ParameterConfig *Angel_MatchParams( const ParameterOptions *requested,
                                          const ParameterOptions *supported )
{
    static Parameter        chosen_params[AP_NUM_PARAMS];
    static ParameterConfig  chosen_config = {
        AP_NUM_PARAMS,
        chosen_params
    };
    unsigned int i;

    ASSERT( requested != NULL, "requested is NULL" );
    ASSERT( requested != NULL, "requested is NULL" );
    ASSERT( supported->num_param_lists <= AP_NUM_PARAMS, "supp_num too big" );

    if ( requested->num_param_lists > supported->num_param_lists )
    {
        WARN( "req_num exceeds supp_num" );
        return NULL;
    }

    for ( i = 0; i < requested->num_param_lists; ++i )
    {
        bool           match;
        unsigned int   j;

        const ParameterList *req_list = &requested->param_list[i];
        ADP_Parameter        req_type = req_list->type;
        const ParameterList *sup_list = Angel_FindParamList(
                                            supported, req_type );

        if ( sup_list == NULL )
        {
#ifdef ASSERTIONS_ENABLED
            __rt_warning( "option %x not supported\n", req_type );
#endif
            return NULL;
        }

        for ( j = 0, match = FALSE;
              (j < req_list->num_options) && !match;
              ++j
            )
        {
            unsigned int k;

            for ( k = 0;
                  (k < sup_list->num_options) && !match;
                  ++k
                )
            {
                if ( req_list->option[j] == sup_list->option[k] )
                {
#ifdef DEBUG
                    __rt_info( "chose value %d for option %x\n",
                               req_list->option[j], req_type );
#endif
                    match = TRUE;
                    chosen_config.param[i].type = req_type;
                    chosen_config.param[i].value = req_list->option[j];
                }
            }
        }

        if ( !match )
        {
#ifdef ASSERTIONS_ENABLED
            __rt_warning( "no match found for option %x\n", req_type );
#endif
            return NULL;
        }
    }

    chosen_config.num_parameters = i;
    INFO( "match succeeded" );
    return &chosen_config;
}
#endif /* defined(TARGET) || defined(TEST_PARAMS) */


#if !defined(TARGET) || defined(TEST_PARAMS)
/*
 * Function: Angel_StoreParam
 *  Purpose: store the value of a given parameter to a config.
 *
 * see params.h for details
 */
bool Angel_StoreParam( ParameterConfig *config,
                       ADP_Parameter    type,
                       unsigned int     value )
{
    unsigned int i;

    for ( i = 0; i < config->num_parameters; ++i )
       if ( config->param[i].type == type )
       {
           config->param[i].value = value;
           return TRUE;
       }

    return FALSE;
}
#endif /* !defined(TARGET) || defined(TEST_PARAMS) */


#if defined(TARGET) || defined(LINK_RECOVERY) || defined(TEST_PARAMS)
/*
 * Function: Angel_BuildParamConfigMessage
 *  Purpose: write a parameter config to a buffer in ADP format.
 *
 * see params.h for details
 */
unsigned int Angel_BuildParamConfigMessage( unsigned char         *buffer,
                                            const ParameterConfig *config )
{
    unsigned char *start = buffer;
    unsigned int i;

    PUT32LE( buffer, config->num_parameters );
    buffer += sizeof( word );

    for ( i = 0; i < config->num_parameters; ++i )
    {
        PUT32LE( buffer, config->param[i].type );
        buffer += sizeof( word );
        PUT32LE( buffer, config->param[i].value );
        buffer += sizeof( word );
    }

    return (buffer - start);
}
#endif /* defined(TARGET) || defined(LINK_RECOVERY) || defined(TEST_PARAMS) */


#if !defined(TARGET) || defined(TEST_PARAMS)
/*
 * Function: Angel_BuildParamOptionsMessage
 *  Purpose: write a parameter Options to a buffer in ADP format.
 *
 * see params.h for details
 */
unsigned int Angel_BuildParamOptionsMessage( unsigned char          *buffer,
                                             const ParameterOptions *options )
{
    unsigned char *start = buffer;
    unsigned int i, j;

    PUT32LE( buffer, options->num_param_lists );
    buffer += sizeof( word );

    for ( i = 0; i < options->num_param_lists; ++i )
    {
        PUT32LE( buffer, options->param_list[i].type );
        buffer += sizeof( word );
        PUT32LE( buffer, options->param_list[i].num_options );
        buffer += sizeof( word );
        for ( j = 0; j < options->param_list[i].num_options; ++j )
        {
            PUT32LE( buffer, options->param_list[i].option[j] );
            buffer += sizeof( word );
        }
    }

    return (buffer - start);
}
#endif /* !defined(TARGET) || defined(TEST_PARAMS) */


#if !defined(TARGET) || defined(LINK_RECOVERY) || defined(TEST_PARAMS)
/*
 * Function: Angel_ReadParamConfigMessage
 *  Purpose: read a parameter config from a buffer where it is in ADP format.
 *
 * see params.h for details
 */
bool Angel_ReadParamConfigMessage( const unsigned char *buffer,
                                   ParameterConfig     *config )
{
    unsigned int word;
    unsigned int i;

    word = GET32LE( buffer );
    buffer += sizeof( word );
    if ( word > config->num_parameters )
    {
        WARN( "not enough space" );
        return FALSE;
    }
    config->num_parameters = word;

    for ( i = 0; i < config->num_parameters; ++i )
    {
        config->param[i].type = (ADP_Parameter)GET32LE( buffer );
        buffer += sizeof( word );
        config->param[i].value = GET32LE( buffer );
        buffer += sizeof( word );
    }

    return TRUE;
}
#endif /* !defined(TARGET) || defined(LINK_RECOVERY) || defined(TEST_PARAMS) */


#if defined(TARGET) || defined(TEST_PARAMS)
/*
 * Function: Angel_ReadParamOptionsMessage
 *  Purpose: read a parameter options block from a buffer
 *             where it is in ADP format.
 *
 * see params.h for details
 */
bool Angel_ReadParamOptionsMessage( const unsigned char *buffer,
                                    ParameterOptions    *options )
{
    unsigned int word;
    unsigned int i, j;

    word = GET32LE( buffer );
    buffer += sizeof( word );
    if ( word > options->num_param_lists )
    {
        WARN( "not enough space" );
        return FALSE;
    }
    options->num_param_lists = word;

    for ( i = 0; i < options->num_param_lists; ++i )
    {
        ParameterList *list = &options->param_list[i];

        list->type = (ADP_Parameter)GET32LE( buffer );
        buffer += sizeof( word );
        word = GET32LE( buffer );
        buffer += sizeof( word );
        if ( word > list->num_options )
        {
            WARN( "not enough list space" );
            return FALSE;
        }
        list->num_options = word;

        for ( j = 0; j < list->num_options; ++j )
        {
            list->option[j] = GET32LE( buffer );
            buffer += sizeof( word );
        }
    }

    return TRUE;
}
#endif /* defined(TARGET) || defined(TEST_PARAMS) */

#endif /* !define(TARGET) || !defined(MINIMAL_ANGEL) || MINIMAL_ANGEL == 0 */

/* EOF params.c */
