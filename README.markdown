[Consistent Overhead Byte Stuffing](http://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) is an encoding that removes all 0 bytes
from arbitrary binary data. The encoded data consists only of bytes with values from 0x01 to 0xFF. This is useful for preparing data for
transmission over a serial link (RS-232 or RS-485 for example), as the 0 byte can be used to unambiguously indicate packet boundaries. COBS
also has the advantage of adding very little overhead (at least 1 byte, plus up to an additional byte per 254 bytes of data). For messages
smaller than 254 bytes, the overhead is constant.

This is an implementation of COBS for C. It is designed to be both efficient and robust. The code is implemented in modern C99. The decoder
is designed to detect  malformed input data and report an error upon detection. A test suite is included to validate the encoder and decoder.

For more information, see my blog bost on [Consistent Overhead Byte Stuffing](http://www.jacquesf.com/2011/03/consistent-overhead-byte-stuffing).
