#ifndef __REGEX_LIB_H
#define __REGEX_LIB_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

uint8_t     regex_match(const char* pattern, const char* str);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __REGEX_LIB_H */
