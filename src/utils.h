#ifndef LIBCONCORD_UTILS_H_
#define LIBCONCORD_UTILS_H_

#define utils_strtol(s) ((s) ? strtol((s), NULL, 10) : 0)
#define utils_strtof(s) ((s) ? strtof((s), NULL) : 0.0)

#endif
