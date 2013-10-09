/* 
For more comments see

http://www.faqs.org/rfcs/rfc3309.html
*/

#include "../sock.h"
#include "crc32.h"

#include "crc32cr.h"

unsigned long crc32c(const unsigned char *buffer, unsigned int length)
{
  unsigned int i;
  unsigned long crc32 = ~0L;
  unsigned long result;
#ifndef LITTLE_ENDIAN
  unsigned int byte0,byte1,byte2,byte3;
#endif

  for (i = 0; i < length; i++){
      CRC32C(crc32, buffer[i]);
  }
  result = ~crc32;

  /*  result  now holds the negated polynomial remainder;
   *  since the table and algorithm is "reflected" [williams95].
   *  That is,  result has the same value as if we mapped the message
   *  to a polynomial, computed the host-bit-order polynomial
   *  remainder, performed final negation, then did an end-for-end
   *  bit-reversal.
   *  Note that a 32-bit bit-reversal is identical to four inplace
   *  8-bit reversals followed by an end-for-end byteswap.
   *  In other words, the bytes of each bit are in the right order,
   *  but the bytes have been byteswapped.  So we now do an explicit
   *  byteswap.  On a little-endian machine, this byteswap and
   *  the final ntohl cancel out and could be elided.
   */

#ifndef LITTLE_ENDIAN
  byte0 = result & 0xff;
  byte1 = (result>>8) & 0xff;
  byte2 = (result>>16) & 0xff;
  byte3 = (result>>24) & 0xff;

  crc32 = ((byte0 << 24) |
           (byte1 << 16) |
           (byte2 << 8)  |
           byte3);

  return ( crc32 );
#else
  return ( result );
#endif
  
}
