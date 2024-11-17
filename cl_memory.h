#ifndef CL_MEMORY_H
#define CL_MEMORY_H

#include "cl_common.h"

/* Memnotes marked with this are submitted to the server on every request,
   as well as on timed intervals to retrieve a play status string. */
#define CL_MEMFLAG_RICH 0

typedef enum
{
  CL_SRCTYPE_IMMEDIATE_INT = 0,
  CL_SRCTYPE_CURRENT_RAM,
  CL_SRCTYPE_PREVIOUS_RAM,
  CL_SRCTYPE_LAST_UNIQUE_RAM,
  CL_SRCTYPE_ROM,
  CL_SRCTYPE_COUNTER,
  CL_SRCTYPE_IMMEDIATE_FLOAT,

  CL_SRCTYPE_SIZE
} cl_src_t;

typedef enum
{
  CL_COMPARE_INVALID = 0,

  CL_COMPARE_EQUAL,
  CL_COMPARE_GREATER,
  CL_COMPARE_LESS,
  CL_COMPARE_NOT_EQUAL,
  CL_COMPARE_INCREASED,
  CL_COMPARE_DECREASED,

  /** @todo Needed? */
  CL_COMPARE_BELOW,
  CL_COMPARE_ABOVE,

  CL_COMPARE_SIZE
} cl_comparison;

#define CLE_CMPTYPE_EQUAL     1
#define CLE_CMPTYPE_GREATER   2
#define CLE_CMPTYPE_LESS      3
#define CLE_CMPTYPE_NOT_EQUAL 4
#define CLE_CMPTYPE_INCREASED 5
#define CLE_CMPTYPE_DECREASED 6
#define CLE_CMPTYPE_ABOVE     7
#define CLE_CMPTYPE_BELOW     8

#include "cl_config.h"
#include "cl_counter.h"
#include "cl_types.h"

/**
 * A "memory bank" or "membank" is a region in emulated memory that has been
 * mapped virtually. This data allows us to follow pointers across different
 * memory regions.
**/
/**
 * A bitfield to represent aspects of a mapped memory region of another
 * process. Primarily used when inspecting a memory region owned by
 * another process.
 */
typedef union cl_memory_region_flags
{
  struct
  {
    unsigned commit : 1;
    unsigned free : 1;
    unsigned reserve : 1;
    unsigned read : 1;
    unsigned write : 1;
    unsigned execute : 1;
    unsigned image : 1;
    unsigned mapped : 1;
    unsigned privated : 1;
  } bits;
  unsigned raw;
} cl_memory_region_flags;

typedef struct cl_memory_region_t
{
  /**
   * Flags that represent how this region can be utilized. Primarily used when
   * reading the memory of an external process.
   * @see cl_memory_region_flags
   */
  cl_memory_region_flags flags;

  /**
   * The byte ordering of this region.
   * @see cl_endianness
   */
  cl_endianness endianness;

  /**
   * A pointer to the location of this memory on the host system. When reading
   * memory from an external process, this will not be a directly accessible
   * pointer, but a location we use to request process memory from the
   * operating system.
   */
  void *base_host;

  /**
   * A pointer to the allocation base location this memory region is
   * contained within. Might be the same as base_host, or CL_ADDRESS_INVALID.
   * Should only be needed when reading external process memory, if ever.
   */
  void *base_alloc;

  /**
   * For emulators, the start of the virtual address for this region in the
   * emulated system, otherwise CL_ADDRESS_INVALID (we do not use NULL here
   * because 0 can be a valid guest memory location).
   */
  cl_addr_t base_guest;

  /** The size, in bytes, of this region. */
  cl_addr_t size;

  /**
   * The size, in bytes, of pointers within this region. Primarily used for
   * emulators that use a smaller pointer length than the host, like 2 or 4.
   */
  unsigned pointer_length;

  /**
   * A string that represents the purpose of this region. Primarily used for
   * virtually-mapped hardware devices on emulators.
   */
  char title[256];
} cl_memory_region_t;

/**
 * A "memory note" or "memnote" is a point in core memory that corresponds
 * with an observable in-game value. Instead of accessing specific addreses
 * every frame, we allocate memory notes and update their contents, then look 
 * off of them for script conditions.

 * Here, we keep track of a memnote's value on the current and previous
 * frame, as well as the last unique value it had before it became
 * what it currently is. 
**/
typedef struct cl_memnote_t
{
  unsigned  key;
  unsigned  order;
  cl_addr_t address;
  cl_addr_t address_initial;

  unsigned flags;
  unsigned type;

  /* Stored values */
  cl_counter_t current;
  cl_counter_t previous;
  cl_counter_t last_unique;

  /* For following pointers to get RAM values */
  unsigned *pointer_offsets;
  unsigned  pointer_passes;

#if CL_HAVE_EDITOR
  /* Metadata for generated human-readable strings in Live Editor */
  /* TODO: Identifiers */
  char description[2048];
  bool edited;
  char title      [256];
#endif
} cl_memnote_t;

/**
 * The global struct that contains information related to the content's memory,
 * including user-defined memory notes.
 **/
typedef struct cl_memory_t
{
  cl_memnote_t *notes;
  unsigned note_count;

  cl_memory_region_t *regions;
  unsigned region_count;
} cl_memory_t;

/**
 * Looks up which memory bank a given virtual address is contained in.
 * @param address A virtual memory address.
 * @return A pointer to the memory bank, or NULL if one is not found.
 **/
cl_memory_region_t* cl_find_memory_region(cl_addr_t address);

/**
 * Frees all values contained within the global memory context.
 **/
void cl_memory_free(void);

/**
 * Frees a memory note. Called automatically as part of cl_free_memory.
 * @param note The memory note to be freed.
 **/
void cl_free_memnote(cl_memnote_t *note);

#if CL_LIBRETRO
/**
 * Initializes memory banks from an array of libretro memory descriptors,
 * informed via environment callback RETRO_ENVIRONMENT_SET_MEMORY_MAPS.
 * @param descs An array of libretro memory descriptors.
 * @param num_descs The count of elements in descs.
 * @return Whether or not memory banks could be initialized.
 **/
struct retro_memory_descriptor;
bool cl_init_membanks_libretro(const struct retro_memory_descriptor **descs,
   const unsigned num_descs);
#endif

/**
 * Checks whether or not a certain flag is set for a given memory note.
 * @param note A pointer to a memory note.
 * @param key A memory note key to be looked up automatically.
 * @param flag The memory note flag to check. For example, CL_MEMFLAG_RICH.
 **/
bool cl_get_memnote_flag(cl_memnote_t *note, uint8_t flag);
bool cl_get_memnote_flag_from_key(uint32_t key, uint8_t flag);

/**
 * Copies the current value of a memory note into a buffer.
 * @param value A buffer for the value to be copied into. Should not be NULL.
 * @param note A pointer to a memory note.
 * @param key A memory note key to be looked up automatically.
 * @param type The data type of the buffer. For example, CL_MEMTYPE_32BIT.
 **/
bool cl_get_memnote_value(cl_counter_t *value, cl_memnote_t *note, unsigned type);
bool cl_get_memnote_value_from_key(cl_counter_t *value, unsigned key, unsigned type);

/* Populate a memory holder with values returned by the web API */
bool cl_init_memory(const char **pos);

/** 
 * Reads a value at a virtual memory address into a buffer by using the memory
 *   bank data pointer.
 * In most cases, the cl_read_memory macro should be used instead.
 * @param value The buffer to be read into.
 * @param bank A pointer to a specific memory bank, or NULL to have it be 
 * looked up automatically.
 * @param address The virtual memory address to read from.
 * @param size The number of bytes to read.
 **/
unsigned cl_read_memory_internal(void *value, const cl_memory_region_t *bank,
  cl_addr_t address, unsigned size);

#if CL_EXTERNAL_MEMORY
/** 
 * Reads a value at a virtual memory address into a buffer by reading external
 *   process memory.
 * In most cases, the cl_read_memory macro should be used instead.
 * @param value The buffer to be read into.
 * @param bank A pointer to a specific memory bank, or NULL to have it be 
 * looked up automatically.
 * @param address The virtual memory address to read from.
 * @param size The number of bytes to read.
 **/
unsigned cl_read_memory_external(void *value, const cl_memory_region_t *bank,
  cl_addr_t address, unsigned size);
#endif

#if CL_EXTERNAL_MEMORY
#define cl_read_memory cl_read_memory_external
#else
#define cl_read_memory cl_read_memory_internal
#endif

/**
 * Returns the size (in bytes) of a given memory type ID.
 * @param type A type of memory value. For example, CL_MEMTYPE_8BIT.
 * @return The number of bytes the memory type takes up, or 0 if invalid.
 **/
unsigned cl_sizeof_memtype(const unsigned type);

/**
 * Steps through all memory notes and updates their values. Should be called 
 * once per frame.
 **/
void cl_update_memory(void);

/**
 * Writes the given data to a location in emulated virtual memory.
 * @param bank A pointer to a specific memory bank, or NULL to have it be 
 * looked up automatically.
 * @param address The virtual address to write to.
 * @param size The number of bytes to write.
 * @param value A pointer to the source data.
 * @return Number of bytes written.
 **/
unsigned cl_write_memory(cl_memory_region_t *bank, cl_addr_t address,
                         unsigned size, const void *value);

/**
 * Writes the value referenced by a memory note with a given value.
 * @param note A pointer to a memory note.
 * @param key The unique key of a memory note.
 * @param value A buffer containing the source value.
 * @return Whether or not the write succeeded.
 **/
bool cl_write_memnote(cl_memnote_t *note, const cl_counter_t *value);
bool cl_write_memnote_from_key(unsigned key, const cl_counter_t *value);

/**
 * Looks up a memory note based on its key.
 * @param key The memory note key to look up. Currently a value between 0-999.
 * @return A pointer to the appropriate memory note, or NULL if one with the
 * given key does not exist.
 **/
cl_memnote_t* cl_find_memnote(uint32_t key);

extern cl_memory_t memory;

#endif
