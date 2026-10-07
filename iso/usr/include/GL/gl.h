/*
 * GL/gl.h - Standard OpenGL Header
 */

#ifndef __gl_h_
#define __gl_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;

#define GL_FALSE                          0
#define GL_TRUE                           1

/* Primitives */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_QUADS                          0x0007
#define GL_QUAD_STRIP                     0x0008
#define GL_POLYGON                        0x0009

/* Matrix Mode */
#define GL_MATRIX_MODE                    0x0BA0
#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_TEXTURE                        0x1702

/* Depth Buffer */
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_TEST                     0x0B71
#define GL_LEQUAL                         0x0203

/* Lighting */
#define GL_LIGHTING                       0x0B50
#define GL_LIGHT0                         0x4000
#define GL_POSITION                       0x1203
#define GL_DIFFUSE                        0x1201
#define GL_COLOR_MATERIAL                 0x0B57
#define GL_SMOOTH                         0x1D01
#define GL_FLAT                           0x1D00

/* Shading */
#define GL_CULL_FACE                      0x0B44

/* Core Functions */
void glClear(GLbitfield mask);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepth(GLclampd depth);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glMatrixMode(GLenum mode);
void glPushMatrix(void);
void glPopMatrix(void);
void glLoadIdentity(void);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glBegin(GLenum mode);
void glEnd(void);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glColor3f(GLfloat red, GLfloat green, GLfloat blue);
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void glShadeModel(GLenum mode);
void glDepthFunc(GLenum func);

#ifdef __cplusplus
}
#endif

#endif /* __gl_h_ */
