#ifndef _ERROR_H
#define _ERROR_H

#include <stdio.h>
#include <errno.h>
#include <string.h>


#define VERBAL


#ifndef VERBAL
#define LOG(str, ...)
#define ERROR(str, ...)
#define DEBUG(str, ...)

#else

#define LOG(str, ...) fprintf(stdout, "[LOG] %s, %s():%d: " str "\n", \
			      __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ERROR(str, ...)  fprintf(stdout, "[ERROR] %s, %s():%d: " str "\n", \
			      __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define DEBUG(str, ...)  fprintf(stdout, "[DEBUG] %s, %s():%d: " str "\n", \
			      __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif


#endif /* _ERROR_H */
