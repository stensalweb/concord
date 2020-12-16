#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>


/* @todo instead of abort(), it should throw the error
	  somewhere */
//this is redefined as a macro
void*
__safe_calloc(size_t nmemb, size_t size, const char file[], const int line, const char func[])
{
	void *ptr = calloc(nmemb, size);

	if (NULL == ptr){
        fprintf(stderr, "[%s:%d] %s()\n\tOut of memory(%ld bytes)\n", file, line, func, size);
        abort();
	}
#if MEMDEBUG_MODE == 1
	fprintf(stderr, "[%s:%d] %s()\n\tAlloc:\t%p(%ld bytes)\n", file, line, func, ptr, size);
#else
    (void)file;
    (void)line;
    (void)func;
#endif

	return ptr;
}

void*
__safe_malloc(size_t size, const char file[], const int line, const char func[])
{
	void *ptr = malloc(size);

	if (NULL == ptr){
        fprintf(stderr, "[%s:%d] %s()\n\tOut of memory(%ld bytes)\n", file, line, func, size);
        abort();
	}
#if MEMDEBUG_MODE == 1
	fprintf(stderr, "[%s:%d] %s()\n\tAlloc:\t%p(%ld bytes)\n", file, line, func, ptr, size);
#else
	  (void)file;
	  (void)line;
	  (void)func;
#endif

	return ptr;
}

void*
__safe_realloc(void *ptr, size_t size, const char file[], const int line, const char func[])
{
	void *tmp = realloc(ptr, size);

	if (NULL == tmp){
        fprintf(stderr, "[%s:%d] %s()\n\tOut of memory(%ld bytes)\n", file, line, func, size);
        abort();
	}
#if MEMDEBUG_MODE == 1
	fprintf(stderr, "[%s:%d] %s()\n\tAlloc:\t%p(%ld bytes)\n", file, line, func, tmp, size);
#else
    (void)file;
    (void)line;
    (void)func;
#endif

	return tmp;
}

//this is redefined as a macro
void
__safe_free(void **p_ptr, const char file[], const int line, const char func[])
{
	if(*p_ptr){
        free(*p_ptr);
#if MEMDEBUG_MODE == 1
        fprintf(stderr, "[%s:%d] %s()\n\tFree:\t%p\n", file, line, func, *p_ptr);
#else
        (void)file;
        (void)line;
        (void)func;
#endif

	  *p_ptr = NULL;
	} 
}

