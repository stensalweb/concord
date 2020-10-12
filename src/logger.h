#ifndef LIBCONCORD_LOGGER_H_
#define LIBCONCORD_LOGGER_H_

//#include "libconcord.h" << implicit

#define logger_throw(msg) fprintf(stderr, "[%s:%lu] %s\n", \
                                        __FILE__, (unsigned long)__LINE__, msg)

#define logger_excep(cond, msg) \
                            do { \
                              if (cond){ \
                                logger_throw(msg); \
                                exit(EXIT_FAILURE); \
                              } \
                            } while(0)

#endif
