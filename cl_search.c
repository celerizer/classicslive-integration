#include <math.h>
#include <string.h>

#include "cl_common.h"
#include "cl_frontend.h"
#include "cl_memory.h"
#include "cl_search.h"

cl_searchbank_t* cl_searchbank_from_address(cl_search_t *search,
  cl_addr_t address)
{
  if (!search)
    return NULL;
  else
  {
    cl_searchbank_t *sbank;
    unsigned i;

    for (i = 0; i < search->searchbank_count; i++)
    {
      sbank = &search->searchbanks[i];
      if (sbank->bank->start < address &&
          sbank->bank->start + sbank->bank->size > address)
        return sbank;
    }

    return NULL;
  }
}

bool cl_search_free(cl_search_t *search)
{
  if (!search)
    return false;
  else
  {
    unsigned i;

    for (i = 0; i < search->searchbank_count; i++)
    {
      if (search->searchbanks[i].backup)
        free(search->searchbanks[i].backup);
      if (search->searchbanks[i].valid)
        free(search->searchbanks[i].valid);
    }
    free(search->searchbanks);

    return true;
  }
}

bool cl_search_init(cl_search_t *search)
{
  if (!memory.bank_count)
    return false;
  else
  {
    unsigned i;

    cl_log("Initializing a new search...\n");
    search->matches = 0;
    search->searchbank_count = memory.bank_count;
    search->searchbanks = (cl_searchbank_t*)calloc(search->searchbank_count,
                                                   sizeof(cl_searchbank_t));

    search->params.compare_type = CLE_CMPTYPE_EQUAL;
    search->params.value_type = CL_MEMTYPE_UINT8;
    search->params.size = cl_sizeof_memtype(search->params.value_type);

    for (i = 0; i < search->searchbank_count; i++)
    {
      cl_searchbank_t *sbank = &search->searchbanks[i];

      sbank->any_valid = true;
      sbank->bank = &memory.banks[i];

      /** @todo Move this to a runtime config option */
#if 1
      sbank->backup = NULL;
#else
      sbank->backup = (uint8_t*)malloc(memory.banks[i].size);
#endif
      sbank->valid = (uint8_t*)malloc(memory.banks[i].size);
    }
    cl_search_reset(search);

    return true;
  }
}

bool cl_read_search(uint32_t *value, cl_search_t *search,
  cl_searchbank_t *sbank, cl_addr_t address)
{
  if (!sbank)
  {
    if (memory.bank_count == 0)
      return false;
    else if (memory.bank_count == 1)
    {
      sbank = &search->searchbanks[0];
      address -= sbank->bank->start;
    }
    else
    {
      uint8_t i;

      for (i = 0; i < memory.bank_count; i++)
      {
        if ((address >= memory.banks[i].start) &&
           (address <  memory.banks[i].start + memory.banks[i].size))
        {
          sbank = &search->searchbanks[i];
          address -= sbank->bank->start;
        }
      }
    }
  }
  if (sbank->backup)
    return cl_read(value, sbank->backup, address, search->params.size,
                   memory.endianness);

  return false;
}

bool cl_search_remove(cl_search_t *search, cl_addr_t address)
{
  cl_searchbank_t *sbank = cl_searchbank_from_address(search, address);

  if (!sbank || sbank->valid[address - sbank->bank->start] == 0)
    return false;
  else
  {
    sbank->valid[address - sbank->bank->start] = 0;
    search->matches--;

    return true;
  }
}

bool cl_search_reset(cl_search_t *search)
{
  if (!search)
    return false;
  else
  {
    cl_searchbank_t *sbank;
    uint8_t i;

#if CL_EXTERNAL_MEMORY == true
    cl_fe_search_deep_copy(search);
#endif
    for (i = 0; i < search->searchbank_count; i++)
    {
      sbank = &search->searchbanks[i];
      if (sbank->backup)
        memcpy(sbank->backup, sbank->bank->data, sbank->bank->size);
      memset(sbank->valid, 1, sbank->bank->size);
      sbank->any_valid = true;
      sbank->first_valid = 0;
      sbank->last_valid = sbank->bank->size - 1;
    }
    search->matches = 0;

    return true;
  }
}

bool compare_to_nothing(uint32_t previous, uint32_t current, uint8_t type)
{
  switch (type)
  {
  case CLE_CMPTYPE_EQUAL:
    return previous == current;
  case CLE_CMPTYPE_LESS:
  case CLE_CMPTYPE_DECREASED:
    return previous > current;
  case CLE_CMPTYPE_GREATER:
  case CLE_CMPTYPE_INCREASED:
    return previous < current;
  case CLE_CMPTYPE_NOT_EQUAL:
    return previous != current;
  }

  return false;
}

bool compare_to_nothing_float(uint32_t previous, uint32_t current, uint8_t type)
{
  float fprevious, fcurrent;

  /* Cast to float */
  memcpy(&fprevious, &previous, sizeof(float));
  memcpy(&fcurrent,  &current,  sizeof(float));

  switch (type)
  {
  case CLE_CMPTYPE_EQUAL:
    return (uint32_t)fprevious == (uint32_t)fcurrent;
  case CLE_CMPTYPE_LESS:
  case CLE_CMPTYPE_DECREASED:
    return (uint32_t)fprevious > (uint32_t)fcurrent;
  case CLE_CMPTYPE_GREATER:
  case CLE_CMPTYPE_INCREASED:
    return (uint32_t)fprevious < (uint32_t)fcurrent;
  case CLE_CMPTYPE_NOT_EQUAL:
    return (uint32_t)fprevious != (uint32_t)fcurrent;
  }

  return false;
}

bool compare_to_value(uint32_t previous, uint32_t current, uint8_t type, uint32_t value)
{
  switch (type)
  {
  case CLE_CMPTYPE_EQUAL:
    return current == value;
  case CLE_CMPTYPE_GREATER:
    return current > value;
  case CLE_CMPTYPE_LESS:
    return current < value;
  case CLE_CMPTYPE_NOT_EQUAL:
    return current != value;
  case CLE_CMPTYPE_INCREASED:
    return current == previous + value;
  case CLE_CMPTYPE_DECREASED:
    return current + value == previous;
  }

  return false;
}

bool compare_to_value_float(uint32_t previous, uint32_t current, uint8_t type,
  float value)
{
  float fprevious, fcurrent;
  bool has_decimal_precision;

  /* Cast to float */
  memcpy(&fprevious, &previous, sizeof(float));
  memcpy(&fcurrent,  &current,  sizeof(float));

  /* This float is NaN */
  if (isnan(fcurrent))
    return false;

  /* Only check decimal precision on equal ops if the user has specified */
  has_decimal_precision = floor(value) != value;

  switch (type)
  {
  case CLE_CMPTYPE_EQUAL:
    if (has_decimal_precision)
      return fcurrent == value;
    else
      return floor(fcurrent) == value;
  case CLE_CMPTYPE_GREATER:
    return fcurrent > value;
  case CLE_CMPTYPE_LESS:
    return fcurrent < value;
  case CLE_CMPTYPE_NOT_EQUAL:
    return fcurrent != value;
  case CLE_CMPTYPE_INCREASED:
    if (has_decimal_precision)
      return fcurrent == fprevious + value;
    else
      return floor(fcurrent) == floor(fprevious) + value;
  case CLE_CMPTYPE_DECREASED:
    if (has_decimal_precision)
      return fcurrent + value == fprevious;
    else
      return floor(fcurrent) + value == floor(fprevious);
  }

  return false;
}

bool resolve_pointerresult(cl_addr_t *final_address, const cl_pointerresult_t *result,
  const uint8_t passes)
{
  cl_addr_t address = result->address_initial;
  uint8_t  i;

  for (i = 0; i < passes; i++)
  {
    if (!cl_read_memory(&address, NULL, address, memory.pointer_size))
      return false;
    address += result->offsets[i];
  }
  *final_address = address;

  return true;
}

uint32_t cl_search_ascii(cl_search_t *search, const char *needle, uint8_t length)
{
  if (!search || search->searchbank_count == 0)
    return 0;
  else
  {
    cl_searchbank_t *sbank;
    const char *haystack;
    char *position;
    uint32_t matches = 0;
    uint8_t  i;
    uint32_t j;

    for (i = 0; i < search->searchbank_count; i++)
    {
      uint32_t matches_this_bank = 0;

      sbank = &search->searchbanks[i];
      memset(sbank->valid, 0, sbank->bank->size);
      haystack = (const char*)sbank->bank->data;

      for (j = 0; j < sbank->bank->size; j++)
      {
        if (!memcmp(&haystack[j], needle, length))
        {
          sbank->valid[j] = true;
          matches_this_bank++;
        }
      }

      if (matches_this_bank == 0)
        sbank->any_valid = false;
      else
        matches += matches_this_bank;
      if (sbank->backup)
        memcpy(sbank->backup, sbank->bank->data, sbank->bank->size);
    }
    search->matches = matches;

    return matches;
  }
}

uint32_t cl_search_step(cl_search_t *search, void *value)
{
  if (!search || search->searchbank_count == 0)
    return 0;
  else
  {
    cl_searchbank_t *sbank;
    bool compare_result;
    uint32_t left, right;
    uint32_t matches  = 0;
    uint8_t  cmp_type = search->params.compare_type;
    uint8_t  size    = search->params.size;
    uint8_t  val_type = search->params.value_type;
    uint8_t  i;
    cl_addr_t j;

    if (!value)
      cl_log("Comparing to nothing...");
    else if (val_type == CL_MEMTYPE_FLOAT)
      cl_log("Comparing to %f...", *((float*)value));
    else
      cl_log("Comparing to %u...", *((uint32_t*)value));

#if CL_EXTERNAL_MEMORY == true
    cl_fe_search_deep_copy(search);
#endif

    for (i = 0; i < search->searchbank_count; i++)
    {
      cl_addr_t matches_this_bank = 0;
      cl_addr_t last_valid = 0;
      bool first_found = false;

      sbank = &search->searchbanks[i];
      if (!sbank->any_valid)
        continue;

      for (j = sbank->first_valid; j <= sbank->last_valid; j += size)
      {
        /* This address has been weeded out already */
        if (!sbank->valid[j])
          continue;

        cl_read_search(&left, search, sbank, j);
        cl_read_memory_internal(&right, NULL, j + sbank->bank->start, size);

        if (!value)
        {
          if (val_type == CL_MEMTYPE_FLOAT)
            compare_result = compare_to_nothing_float(left, right, cmp_type);
          else
            compare_result = compare_to_nothing(left, right, cmp_type);
        }
        else
        {
          if (val_type == CL_MEMTYPE_FLOAT)
            compare_result = compare_to_value_float(left, right, cmp_type, *((float*)value));
          else
          {
            uint32_t* intval = (uint32_t*)value;

            if (size == 1)
              *intval &= 0xFF;
            else if (size == 2)
              *intval &= 0xFFFF;

            compare_result = compare_to_value(left, right, cmp_type, *intval);
          }
        }

        if (!compare_result)
          sbank->valid[j] = 0;
        else
        {
          /* Set our new first valid offset */
          if (!first_found)
          {
            sbank->first_valid = j;
            first_found = true;
          }
          last_valid = j;
          sbank->valid[j] = 1;
          matches_this_bank++;
        }
      }

      if (matches_this_bank == 0)
        sbank->any_valid = false;
      else
        matches += matches_this_bank;
      if (sbank->backup)
        memcpy(sbank->backup, sbank->bank->data, sbank->bank->size);
      sbank->last_valid = last_valid;
    }
    search->matches = matches;
    cl_log(" %u matches.\n", matches);

    return matches;
  }
}

bool cl_pointersearch_free(cl_pointersearch_t *search)
{
  if (!search)
    return false;
  else
  {
    free(search->results);
    return true;
  }
}

bool add_pass(cl_pointersearch_t* search, uint32_t range, uint32_t max_results)
{
  cl_membank_t *bank;
  cl_pointerresult_t *result;
  uint32_t matches, target, value;
  uint32_t i, j, k, l;

  cl_pointerresult_t* new_results = (cl_pointerresult_t*)calloc(
    max_results, sizeof(cl_pointerresult_t));
  matches = 0;
  search->passes += 1;

  for (i = 0; i < search->result_count; i++)
  {
    cl_pointerresult_t *next_result = &search->results[i];

    for (j = 0; j < memory.bank_count; j++)
    {
      bank = &memory.banks[j];
      target = next_result->address_initial;

      if (bank->size < memory.pointer_size)
        continue;

      for (k = 0; k < bank->size; k += memory.pointer_size)
      {
        cl_read_memory(&value, bank, k, memory.pointer_size);

        if (value <= target && value >= target - range)
        {
          result = &new_results[matches];

          /* Shift all offsets over by one */
          for (l = search->passes - 1; l > 0; l--)
            result->offsets[l] = next_result->offsets[l - 1];

          /* Make this the new initial offset */
          result->offsets[0] = target - value;
          result->address_initial = bank->start + k;
          matches++;
        }

        /* Back out if we have too many results */
        if (matches == max_results)
        {
          cl_log("Search reached maximum count of %u.\n", max_results);
          goto end;
        }
      }
    }
  }
  end:
  free(search->results);
  search->results = new_results;
  search->result_count = matches;

  return true;
}

bool cl_pointersearch_init(cl_pointersearch_t *search,
  cl_addr_t address, uint8_t val_type, uint8_t passes, uint32_t range,
  uint32_t max_results)
{
  if (!search || address == 0 || passes == 0)
    return false;
  else
  {
    cl_membank_t *bank;
    cl_pointerresult_t *result;
    uint32_t current_match, matches, prev_value, value;
    bool exact_only;
    uint32_t i, j;

    /* Is the address we're looking for valid? */
    if (!cl_read_memory(&prev_value, NULL, address, cl_sizeof_memtype(val_type)))
    {
      cl_log("Address %08X is invalid for a pointer search.\n", address);
      return false;
    }

    /* Initialize search parameters */
    search->passes          = 1;
    search->range          = range;
    search->params.compare_type = CLE_CMPTYPE_EQUAL;
    search->params.size      = cl_sizeof_memtype(val_type);
    search->params.value_type  = val_type;

    /* Only one memory bank; all pointed addresses are probably relative */
    if (memory.bank_count == 1)
      exact_only = false;

    /* We create a temporary array of max size and trim it down after */
    search->results = (cl_pointerresult_t*)calloc(
      max_results, sizeof(cl_pointerresult_t));
    matches = 0;

    /* Do a quick scan to see how many results we start with */
    for (i = 0; i < memory.bank_count; i++)
    {
      /* This is where per-console hacks were being inserted */
      uint32_t target = exact_only ? bank->start + address : address;

      bank = &memory.banks[i];

      if (bank->size < memory.pointer_size)
        continue;

      for (j = 0; j < bank->size; j += memory.pointer_size)
      {
        cl_read_memory(&value, bank, j, memory.pointer_size);

        if (value <= target && value >= target - range)
        {
          result = &search->results[matches];

          result->offsets[0]    = address - value;
          result->address_initial = bank->start + j;
          result->address_final  = address;
          result->value_current  = prev_value;
          result->value_previous  = prev_value;
          matches++;
        }

        if (matches == max_results)
        {
          search->result_count = max_results;
          cl_log("Pointer search for %08X reached maximum result count of %u.\n", address, max_results);

          return true;
        }
      }
    }
    search->result_count = matches;

    /* We've only done one pass so far. Run any extra passes */
    for (i = passes; i > 1; i--)
      add_pass(search, range, max_results);

    /* Clear the unneeded memory */
    search->result_count = matches;
    search->results = (cl_pointerresult_t*)realloc(
      search->results, matches * sizeof(cl_pointerresult_t));

    cl_log("Pointer search for %08X found %u results.\n", address, matches);

    return true;
  }
}

uint32_t cl_pointersearch_step(cl_pointersearch_t *search, void *value)
{
  if (!search)
    return false;
  else
  {
    cl_pointerresult_t *result;
    cl_addr_t address;
    uint32_t matches, final_value, valid_pointers;
    bool compare_result;
    uint8_t cmp_type = search->params.compare_type;
    uint32_t i, j;

    matches = 0;
    valid_pointers = 0;
    cl_log("Result count at start: %u\n", search->result_count);
    for (i = 0; i < search->result_count; i++)
    {
      result  = &search->results[i];

      if (!resolve_pointerresult(&address, result, search->passes))
        continue;
      else if (!cl_read_memory(&final_value, NULL, address, search->params.size))
        continue;
      else
      {
        result->value_current = final_value;

        if (!value)
        {
          compare_result = search->params.value_type == CL_MEMTYPE_FLOAT ?
            compare_to_nothing_float(result->value_previous, result->value_current, cmp_type) :
            compare_to_nothing(result->value_previous, result->value_current, cmp_type);
        }
        else
        {
          compare_result = search->params.value_type == CL_MEMTYPE_FLOAT ?
            compare_to_value_float(result->value_previous, result->value_current, cmp_type, *((float*)value)) :
            compare_to_value(result->value_previous, result->value_current, cmp_type, *((uint32_t*)value));
        }

        if (compare_result)
        {
          memcpy(&search->results[matches], result, sizeof(cl_pointerresult_t));
          matches++;
        }
        result->value_previous = result->value_current;
        valid_pointers++;
      }
    }
    /* All of the still valid results are grouped together, the rest of memory can be cleared */
    search->result_count = matches;
    search->results = (cl_pointerresult_t*)realloc(search->results, matches * sizeof(cl_pointerresult_t));
    cl_log("Pointer search now has %u matches across %u valid pointers.\n", matches, valid_pointers);

    return matches;
  }
}

void cl_pointersearch_update(cl_pointersearch_t *search)
{
  if (!search)
    return;
  else
  {
    cl_pointerresult_t *result;
    uint32_t i;

    for (i = 0; i < search->result_count; i++)
    {
      result = &search->results[i];

      if (!resolve_pointerresult(&result->address_final, result, search->passes))
        continue;
      else
        cl_read_memory(&result->value_current, NULL, result->address_final, search->params.size);
    }
  }
}
