#include "cle_common.h"

bool stringToCounter(cl_counter_t *counter, QString string, bool *ok)
{
  if (string.isEmpty())
    return false;
  else
  {
    int base = 0;
    bool negative = false;
    char prefix = string.at(0).toLatin1();

    /* Pop one more if entered value is negative */
    if (prefix == '-')
    {
      negative = true;
      string.remove(0, 1);
      prefix = string.at(0).toLatin1();
    }

    /* Return interpretation of entered number */
    switch (tolower(prefix))
    {
    case 'b':
      /* Binary */
      base = 2;
      break;
    case 'o':
      /* Octal */
      base = 8;
      break;
    case 'd':
      /* Decimal */
      base = 10;
      break;
    case 'h':
    case 'x':
      /* Hexidecimal */
      base = 16;
    }

    if (base)
      string.remove(0, 1);
    else
      base = 10; // TODO: Config option for default base?

    return cl_ctr_store_int(counter, negative ? 0 - string.toInt(ok, base) :
                                                string.toInt(ok, base)) && ok;
  }
}

void valueToString(char *string, uint8_t length, uint32_t value, 
   uint8_t memtype)
{
   switch (memtype)
   {
   case CL_MEMTYPE_INT8:
      snprintf(string, length, "%02X (%i)", value, value);
      break;
   case CL_MEMTYPE_UINT8:
      snprintf(string, length, "%02X (%u)", value, value);
      break;
   case CL_MEMTYPE_INT16:
      snprintf(string, length, "%04X (%i)", value, value);
      break;
   case CL_MEMTYPE_UINT16:
      snprintf(string, length, "%04X (%u)", value, value);
      break;
   case CL_MEMTYPE_INT32:
      snprintf(string, length, "%08X (%i)", value, value);
      break;
   case CL_MEMTYPE_UINT32:
      snprintf(string, length, "%08X (%u)", value, value);
      break;
   case CL_MEMTYPE_FLOAT:
      snprintf(string, length, "%08X (%f)", value, *((float*)(&value)));
      break;
   default:
      snprintf(string, length, "%08X", value);
      break;
   }
}
