#ifndef PROCESSOR_BIND_H
#define PROCESSOR_BIND_H

#include <stdint.h>

#define EFIAPI

///
/// 8-byte unsigned value
///
typedef uint64_t UINT64;
///
/// 8-byte signed value
///
typedef int64_t INT64;
///
/// 4-byte unsigned value
///
typedef uint32_t UINT32;
///
/// 4-byte signed value
///
typedef int32_t INT32;
///
/// 2-byte unsigned value
///
typedef uint16_t UINT16;
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef uint16_t CHAR16;
///
/// 2-byte signed value
///
typedef int16_t INT16;
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef uint8_t BOOLEAN;
///
/// 1-byte unsigned value
///
typedef uint8_t UINT8;
///
/// 1-byte Character
///
typedef int8_t CHAR8;
///
/// 1-byte signed value
///
typedef int8_t INT8;

///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef uintptr_t UINTN;
///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef intptr_t INTN;

#endif // PROCESSOR_BIND_H
