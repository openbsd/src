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
 *     Title: Parameter negotiation structures and utilities
 */

#ifndef angel_params_h
#define angel_params_h

#include "angel.h"
#include "adp.h"

#ifndef TARGET
# include "host.h"
#endif

/* A single parameter, with tag */
typedef struct Parameter {
      ADP_Parameter     type;
      unsigned int      value;
} Parameter;

/* A list of parameter values, with tag */
typedef struct ParameterList {
      ADP_Parameter     type;
      unsigned int      num_options;
      unsigned int     *option; /* points to array of values */
} ParameterList;

/* A configuration of one or more parameters */
typedef struct ParameterConfig {
      unsigned int      num_parameters;
      Parameter        *param;  /* pointer to array of Parameters */
} ParameterConfig;

/* A set of parameter options */
typedef struct ParameterOptions {
      unsigned int      num_param_lists;
      ParameterList    *param_list; /* pointer to array of ParamLists */
} ParameterOptions;

/*
 * Function: Angel_MatchParams
 *  Purpose: find a configuration from the requested options which is
 *           the best match from the supported options.
 *
 *   Params:
 *              Input: requested      The offered set of parameters.
 *                     supported      The supported set of parameters.
 *
 *            Returns: ptr to config  A match has been made, ptr to result
 *                                      will remain valid until next call to
 *                                      this function.
 *                     NULL           Match not possible
 */
const ParameterConfig *Angel_MatchParams( const ParameterOptions *requested,
                                          const ParameterOptions *supported );

/*
 * Function: Angel_FindParam
 *  Purpose: find the value of a given parameter from a config.
 *
 *   Params:
 *              Input: type     parameter type to find
 *                     config   config to search
 *             Output: value    parameter value if found
 *
 *            Returns: TRUE     parameter found
 *                     FALSE    parameter not found
 */
bool Angel_FindParam( ADP_Parameter          type,
                      const ParameterConfig *config,
                      unsigned int          *value );

/*
 * Function: Angel_StoreParam
 *  Purpose: store the value of a given parameter in a config.
 *
 *   Params:
 *             In/Out: config   config to store in
 *              Input: type     parameter type to store
 *                     value    parameter value if found
 *
 *            Returns: TRUE     parameter found and new value stored
 *                     FALSE    parameter not found
 */
bool Angel_StoreParam( ParameterConfig *config,
                       ADP_Parameter    type,
                       unsigned int     value );

/*
 * Function: Angel_FindParamList
 *  Purpose: find the parameter list of a given parameter from an options.
 *
 *   Params:
 *              Input: type     parameter type to find
 *                     options  options block to search
 *
 *            Returns: pointer to list
 *                     NULL     parameter not found
 */
ParameterList *Angel_FindParamList( const ParameterOptions *options,
                                    ADP_Parameter           type );

/*
 * Function: Angel_BuildParamConfigMessage
 *  Purpose: write a parameter config to a buffer in ADP format.
 *
 *   Params:
 *              Input: buffer   where to write to
 *                     config   the parameter config to write
 *
 *            Returns: number of characters written to buffer
 */
unsigned int Angel_BuildParamConfigMessage( unsigned char         *buffer,
                                            const ParameterConfig *config );

/*
 * Function: Angel_BuildParamOptionsMessage
 *  Purpose: write a parameter Options to a buffer in ADP format.
 *
 *   Params:
 *              Input: buffer   where to write to
 *                     options  the options block to write
 *
 *            Returns: number of characters written to buffer
 */
unsigned int Angel_BuildParamOptionsMessage( unsigned char          *buffer,
                                             const ParameterOptions *options );

/*
 * Function: Angel_ReadParamConfigMessage
 *  Purpose: read a parameter config from a buffer where it is in ADP format.
 *
 *   Params:
 *              Input: buffer   where to read from
 *             In/Out: config   the parameter config to read to, which must
 *                              be set up on entry with a valid array, and
 *                              the size of the array in num_parameters.
 *
 *            Returns: TRUE     okay
 *                     FALSE    not enough space in config
 */
bool Angel_ReadParamConfigMessage( const unsigned char *buffer,
                                   ParameterConfig     *config );

/*
 * Function: Angel_ReadParamOptionsMessage
 *  Purpose: read a parameter options from a buffer
 *             where it is in ADP format.
 *
 *   Params:
 *              Input: buffer   where to read from
 *             In/Out: options  the parameter options block to read to, 
 *                                which must be set up on entry with a valid
 *                                array, and the size of the array in
 *                                num_parameters.  Each param_list must
 *                                also be set up in the same way.
 *
 *            Returns: TRUE     okay
 *                     FALSE    not enough space in options
 */
bool Angel_ReadParamOptionsMessage( const unsigned char *buffer,
                                    ParameterOptions    *options );

#endif /* ndef angel_params_h */

/* EOF params.h */
