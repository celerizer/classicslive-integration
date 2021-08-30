#ifndef CL_SEARCH_H
#define CL_SEARCH_H

#include "cl_types.h"

#define CL_POINTER_MAX_PASSES 4

typedef struct cl_search_params_t
{
   uint8_t compare_type;
   uint8_t size;
   uint8_t value_type;
} cl_search_params_t;

typedef struct cl_searchbank_t
{
   cl_membank_t *bank;
   uint8_t *backup;
   uint8_t *valid;
   bool any_valid;
} cl_searchbank_t;

typedef struct cl_search_t
{
   cl_searchbank_t    *searchbanks;
   cl_search_params_t  params;
   uint8_t             searchbank_count;
   uint32_t            matches;
} cl_search_t;

typedef struct cl_pointerresult_t
{
   cl_addr_t address_initial;
   cl_addr_t address_final;
   uint32_t  value_current;
   uint32_t  value_previous;
   int32_t   offsets[CL_POINTER_MAX_PASSES];
} cl_pointerresult_t;

typedef struct cl_pointersearch_t
{
   cl_search_params_t  params;
   uint8_t             passes;
   uint32_t            range; 
   cl_pointerresult_t *results;
   uint32_t            result_count;
} cl_pointersearch_t;

bool cl_read_search (uint32_t *value, cl_search_t *search, 
   cl_searchbank_t *bank, cl_addr_t address);

uint32_t cl_search_ascii (cl_search_t *search, const char *needle, uint8_t length);
bool     cl_search_free  (cl_search_t *search);
bool     cl_search_init  (cl_search_t *search);

/*
   Unsets the validity of a specific address.
   Returns TRUE if it succeeds.
*/
bool cl_search_remove(cl_search_t *search, cl_addr_t address);

/*
   Sets the validity of all addresses to true.
   Returns TRUE if it succeeds.
*/
bool cl_search_reset(cl_search_t *search);

/*
   Unsets the validity of all addresses that no longer meet the given conditions.
   Returns the number of valid addresses afterwards.
*/
uint32_t cl_search_step(cl_search_t *search, void *value);

bool cl_pointersearch_free(cl_pointersearch_t *search);

bool cl_pointersearch_init(cl_pointersearch_t *search, cl_addr_t address, 
   uint8_t size, uint8_t passes, uint32_t range, uint32_t max_results);

uint32_t cl_pointersearch_step(cl_pointersearch_t *search, void *value);

/*
   Updates the "value_current" variable in every result in a pointersearch. 
*/
void cl_pointersearch_update(cl_pointersearch_t *search);

#endif
