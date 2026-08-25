
#ifndef OpenGLHeaderH
#define OpenGLHeaderH

#include <glad/gles2.h>

TJS_EXP_FUNC_DEF(void*, TVPGLGetProcAddress, (const char * procname));

extern bool _CheckGLErrorAndLog(const char *filename, int lineno, const char* funcname=nullptr);

#ifndef  NDEBUG
#define CheckGLErrorAndLog(funcname) _CheckGLErrorAndLog(__FILE__, __LINE__, funcname);
#else
#define CheckGLErrorAndLog(funcname) (1)
#endif

#endif
