#ifndef CL_SEARCH_NEW_H
#define CL_SEARCH_NEW_H

#include "cl_memory.h"
#include "cl_types.h"

#define CL_SEARCH_PAGE_SIZE 4096

typedef struct cl_search_page_t cl_search_page_t;
struct cl_search_page_t
{
  cl_addr_t start;

  unsigned matches;

  /** The first byte in the page that is flagged in the validity bitfield */
  unsigned first_match;

  /** The last byte in the page that is flagged in the validity bitfield */
  unsigned last_match;

  /**
   * A copy of the memory state at the time of the last search step
   */
  uint8_t *data;

  /**
   * Should the the size of data / 8 / search value size
   */
  uint8_t *validity;

  /** Pointer to the region this page holds search data for */
  const cl_memory_region_t *region;

  struct cl_search_page_t *prev;
  struct cl_search_page_t *next;
};

typedef struct
{
  unsigned value_size;

  /** The number of bytes searched in each page */
  unsigned page_size;

  /** The number of values in this page that passed the last step */
  unsigned matches;

  /** The number of times a search step has completed */
  unsigned steps;

  /** The number of pages that are currently allocated */
  unsigned page_count;

  /** The type of comparison being done this step */
  cl_comparison comparison;

  /**
   * The source of the "right" value in the comparison
   * Should be either an immediate or previous memory value
   */
  cl_src_t source;

  cl_arg_t value;

  cl_search_page_t *begin;
  cl_search_page_t *end;
} cl_search_t;

cl_search_t *cl_search_init(void);

void cl_search_free(cl_search_t *search);

unsigned cl_search_step(cl_search_t *search);

#endif
