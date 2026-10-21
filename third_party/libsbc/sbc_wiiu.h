/* force-included when building libsbc for the Wii U (big endian PowerPC, no config.h) */
#pragma once

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __BYTE_ORDER
#define __BYTE_ORDER __BIG_ENDIAN
#endif
