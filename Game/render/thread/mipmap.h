#ifndef _MIPMAP_H
#define _MIPMAP_H

#include <ogl.h>

// Generated by mkproto
#ifdef EPSILON
#endif
#if defined(__WIN32__) && !defined(OPENSTEP)
void dummy(GLuint j, GLuint k);
#else
#endif

GLint COH_Build2DMipmapsSRC(GLenum target, GLenum dst_format, GLint width, GLint height, GLenum src_format, GLenum type, const void *data);

// End mkproto
#endif