/* 4.4BSD standard API replacement for strtok()
 */

#ifndef __STRSEP_H__
#define __STRSEP_H__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char* strsep_s (char** stringp, const char* delim, size_t numberOfElements);

#ifdef __cplusplus
}
#endif

#endif /* __STRSEP_H__ */

/* eof */