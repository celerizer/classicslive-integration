#include <string.h>

#include "cl_search_new.h"

/**
 * - Setup search parameters
 * - Create pages while performing initial search
 * - Free invalid pages on continued search steps
 *
 * @todo
 * - Make the validity bitfield small as expected
 * - Speedup by including a pointer to the relevant memory region in each page
 * - Implement a low memory mode that does not deep copy into the "data" buffer
 *   but allows only comparisons to immediates
 * - Multithreading
 * - Include maximum result/memory usage params
 */

/** @todo use instead a pointer to function returning bool and inline this */
static bool cl_search_value_matches(const cl_counter_t *left,
                                    const cl_counter_t *right,
                                    cl_comparison comparison)
{
  switch (comparison)
  {
  case CL_COMPARE_EQUAL:
    return cl_ctr_equal(left, right);
  case CL_COMPARE_GREATER:
    return cl_ctr_greater(left, right);
  case CL_COMPARE_LESS:
    return cl_ctr_lesser(left, right);
  default:
    return false;
  }

  return false;
}

/**
 * Attempts to create a search page by checking the specified region of memory.
 * Returns a new page if there are any matches, returns NULL otherwise.
 */
static cl_search_page_t *cl_search_page_init(const cl_search_t *search,
                                             const cl_addr_t address,
                                             uint8_t *buffer, uint8_t *validity,
                                             const cl_memory_region_t *region)
{
  if (search)
  {
    cl_search_page_t *page = calloc(1, sizeof(cl_search_page_t));

    cl_read_memory(buffer, region, address, search->page_size);
  }
  return NULL;
}

/**
 * Frees a search page from memory.
 * Returns the next page in sequence, or NULL if it is the final page.
 */
static cl_search_page_t *cl_search_page_free(cl_search_page_t *page)
{
  if (page)
  {
    cl_search_page_t *next = page->next;

    /* Update double-linked list */
    if (page->prev)
      page->prev->next = page->next;
    if (page->next)
      page->next->prev = page->prev;

    /* Free all the memory */
    free(page->data);
    free(page->validity);
    free(page);

    return next;
  }
  else
    return NULL;
}

static unsigned cl_search_step_first(cl_search_t *search)
{
  if (search)
  {
    uint8_t *buffer = malloc(search->page_size);
    uint8_t *validity = malloc(search->page_size);
    unsigned total_matches = 0;
    unsigned page_count = 0;
    unsigned i;

    for (i = 0; i < memory.region_count; i++)
    {
      const cl_memory_region_t *region = &memory.regions[i];
      cl_addr_t j;

      for (j = region->base_guest; j < region->base_guest + region->size;
           j += search->value_size)
      {
        cl_search_page_t *page = cl_search_page_init(search, j, buffer,
                                                     validity, region);

        if (page)
        {
          /* Set this as the starting page if it is our first */
          if (page_count == 0)
            search->begin = page;
          else
            search->end->next = page;
          page->prev = search->end;
          page->next = NULL;
          search->end = page;

          total_matches += page->matches;
          page_count++;

          /* Re-allocate the buffers for the next valid page */
          buffer = malloc(search->page_size);
          validity = malloc(search->page_size);
        }
      }
    }
    /* Cleanup our buffers since we allocated one more than needed */
    free(buffer);
    free(validity);

    return total_matches;
  }
  else
    return 0;
}

unsigned cl_search_step(cl_search_t *search)
{
  if (search && search->begin)
  {
    cl_search_page_t *page = search->begin;
    uint8_t *buffer = malloc(search->page_size);
    int64_t value_buffer;
    unsigned total_matches = 0;
    unsigned page_count = 0;
    cl_counter_t left, right;

    if (search->source == CL_SRCTYPE_IMMEDIATE_INT)
      cl_ctr_store_int(&right, search->value.intval);
    else if (search->source == CL_SRCTYPE_IMMEDIATE_FLOAT)
      cl_ctr_store_float(&right, search->value.floatval);

    while (page)
    {
      unsigned first_match = 0;
      unsigned last_match = 0;
      unsigned page_matches = 0;
      unsigned i;

      /* Attempt to read the new memory state of this page */
      if (!cl_read_memory(buffer, NULL, page->start, search->page_size))
      {
        /* Free the page if its data was unable to be read */
        page = cl_search_page_free(page);
        continue;
      }

      for (i = page->first_match; i < page->last_match; i++)
      {
        /** @todo Reimplement with bitfield method */
        /* Check if this value is currently valid for comparing */
        if (!page->validity[i])
          continue;

        /* Update "left" counter to current memory contents */
        cl_read(&value_buffer, buffer, i, search->value_size, CL_ENDIAN_NATIVE);
        cl_ctr_store_int(&left, value_buffer);

        /* Update "right" counter to previous memory contents, if needed */
        if (search->source == CL_SRCTYPE_PREVIOUS_RAM)
        {
          /** @todo Other endianness */
          cl_read(&value_buffer, page->data, i, search->value_size, CL_ENDIAN_NATIVE);
          cl_ctr_store_int(&right, value_buffer);
        }

        if (cl_search_value_matches(&left, &right, search->comparison))
        {
          /* Set the first match */
          if (!first_match)
            first_match = i;
          last_match = i;
          page_matches++;
        }
        else
          page->validity[i] = 0;
      }

      /* If there are any matches, copy the new state into the page.
       * Otherwise, free the page. */
      if (page_matches)
      {
        memcpy(page->data, buffer, search->page_size);
        page_count++;
        total_matches += page_matches;
        page->first_match = first_match;
        page->last_match = last_match;
        page = page->next;
      }
      else
        page = cl_search_page_free(page);
    }
    search->matches = total_matches;
    search->page_count = page_count;
    free(buffer);

    return total_matches;
  }
  else
    return 0;
}

cl_search_t *cl_search_init(void)
{
  cl_search_t *search = (cl_search_t*)calloc(sizeof(cl_search_t), 1);

  search->comparison = CL_COMPARE_EQUAL;
  search->matches = 0;
  search->page_count = 0;
  search->page_size = CL_SEARCH_PAGE_SIZE;
  search->steps = 0;
  search->source = CL_SRCTYPE_IMMEDIATE_INT;
  search->value_size = 4;

  search->begin = NULL;
  search->end = NULL;

  return search;
}

void cl_search_free(cl_search_t *search)
{
  if (search)
  {
    cl_search_page_t *page = search->begin;
    unsigned i = 0;

    while (page)
    {
      page = cl_search_page_free(page);
      i++;
    }
    if (i != search->page_count)
      cl_message(CL_MSG_ERROR, "Potential memory leak while freeing search"
                               "(expected %u pages, freed %u)",
                 search->page_count, i);
    free(search);
  }
}
