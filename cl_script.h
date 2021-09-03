#ifndef CL_SCRIPT_H
#define CL_SCRIPT_H

enum
{
   CL_SCRSTATUS_INACTIVE = 0,

   CL_SRCSTATUS_ACTIVE,
   CL_SCRSTATUS_PAUSED,

   CL_SCRSTATUS_LAST
};

/* TODO: Arbitrary! Have pages allocate more/less depending on need */
#define CL_COUNTERS_SIZE 16

#include "cl_action.h"

typedef struct cl_counter_t
{
   unsigned type;
   uint64_t value;
} cl_counter_t;

typedef struct cl_page_t
{
   cl_action_t *actions;
   uint16_t     action_count;

   /* Temporary values (bitflags, counters) we can use for logic */
   cl_counter_t counters[CL_COUNTERS_SIZE];

   uint32_t     flags;
} cl_page_t;

typedef struct cl_script_t
{
   cl_page_t *pages;
   uint16_t   page_count;

   cl_action_t *current_action;
   cl_page_t   *current_page;
   bool         evaluation;
   uint8_t      status;

   /* Used for identifying the cause of breaks while debugging */
   bool error_fatal;
   char error_msg[256];
} cl_script_t;

/* Public */
void      cl_free_script       ();
uint32_t* cl_get_counter       (uint8_t counter_number);
bool      cl_get_counter_value (uint32_t *buffer, uint8_t counter_num);
bool      cl_init_script       (const char **pos);
bool      cl_update_script     (void);

/**
 * Signals to halt processing of the script and core. Used when debugging 
 * scripts.
 * @param fatal Whether or not the break reason included a fatal error.
 * @param format A format string with arguments, to specify a reason for breaking. 
 **/
void cl_script_break(bool fatal, const char *format, ...);

extern cl_script_t script;

#endif