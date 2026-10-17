/*
    OpenMCM2 - OpenGL, loaded at runtime.

    The game does not import opengl32.dll, and an import of our own would need an import table
    (see tools/link.py), so every entry point is looked up when the renderer starts. This is the
    whole of OpenGL the renderer uses: the 1.1 fixed-function pipeline, multitexture and texture
    combiners for the Direct3D texture stages, fog coordinates for its vertex fog, and framebuffer
    objects for the game's back buffer.
*/

#pragma once

#include "runtime.h"

typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLboolean;
typedef unsigned char  GLubyte;
typedef float          GLfloat;
typedef double         GLdouble;
typedef void           GLvoid;
typedef char           GLchar;

#define GL_FALSE                        0
#define GL_TRUE                         1
#define GL_POINTS                       0x0000
#define GL_LINES                        0x0001
#define GL_LINE_STRIP                   0x0003
#define GL_TRIANGLES                    0x0004
#define GL_TRIANGLE_STRIP               0x0005
#define GL_TRIANGLE_FAN                 0x0006
#define GL_ONE                          1
#define GL_NEVER                        0x0200
#define GL_SRC_COLOR                    0x0300
#define GL_ONE_MINUS_SRC_COLOR          0x0301
#define GL_SRC_ALPHA                    0x0302
#define GL_ONE_MINUS_SRC_ALPHA          0x0303
#define GL_DST_ALPHA                    0x0304
#define GL_ONE_MINUS_DST_ALPHA          0x0305
#define GL_DST_COLOR                    0x0306
#define GL_ONE_MINUS_DST_COLOR          0x0307
#define GL_SRC_ALPHA_SATURATE           0x0308
#define GL_FRONT                        0x0404
#define GL_BACK                         0x0405
#define GL_CW                           0x0900
#define GL_CCW                          0x0901
#define GL_CULL_FACE                    0x0B44
#define GL_FOG                          0x0B60
#define GL_FOG_DENSITY                  0x0B62
#define GL_FOG_START                    0x0B63
#define GL_FOG_END                      0x0B64
#define GL_FOG_MODE                     0x0B65
#define GL_FOG_COLOR                    0x0B66
#define GL_DEPTH_TEST                   0x0B71
#define GL_ALPHA_TEST                   0x0BC0
#define GL_BLEND                        0x0BE2
#define GL_SCISSOR_TEST                 0x0C11
#define GL_UNPACK_ALIGNMENT             0x0CF5
#define GL_PACK_ALIGNMENT               0x0D05
#define GL_TEXTURE_2D                   0x0DE1
#define GL_DONT_CARE                    0x1100
#define GL_UNSIGNED_BYTE                0x1401
#define GL_UNSIGNED_SHORT               0x1403
#define GL_FLOAT                        0x1406
#define GL_MODELVIEW                    0x1700
#define GL_PROJECTION                   0x1701
#define GL_TEXTURE                      0x1702
#define GL_RGB                          0x1907
#define GL_RGBA                         0x1908
#define GL_FLAT                         0x1D00
#define GL_SMOOTH                       0x1D01
#define GL_VENDOR                       0x1F00
#define GL_RENDERER                     0x1F01
#define GL_VERSION                      0x1F02
#define GL_MODULATE                     0x2100
#define GL_TEXTURE_ENV_MODE             0x2200
#define GL_TEXTURE_ENV_COLOR            0x2201
#define GL_TEXTURE_ENV                  0x2300
#define GL_NEAREST                      0x2600
#define GL_LINEAR                       0x2601
#define GL_NEAREST_MIPMAP_NEAREST       0x2700
#define GL_LINEAR_MIPMAP_NEAREST        0x2701
#define GL_NEAREST_MIPMAP_LINEAR        0x2702
#define GL_LINEAR_MIPMAP_LINEAR         0x2703
#define GL_TEXTURE_MAG_FILTER           0x2800
#define GL_TEXTURE_MIN_FILTER           0x2801
#define GL_TEXTURE_WRAP_S               0x2802
#define GL_TEXTURE_WRAP_T               0x2803
#define GL_REPEAT                       0x2901
#define GL_CLAMP                        0x2900
#define GL_EXP                          0x0800
#define GL_EXP2                         0x0801
#define GL_COLOR_BUFFER_BIT             0x00004000
#define GL_DEPTH_BUFFER_BIT             0x00000100
#define GL_STENCIL_BUFFER_BIT           0x00000400
#define GL_VERTEX_ARRAY                 0x8074
#define GL_COLOR_ARRAY                  0x8076
#define GL_TEXTURE_COORD_ARRAY          0x8078
#define GL_POLYGON_OFFSET_FILL          0x8037
#define GL_UNSIGNED_SHORT_4_4_4_4_REV   0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV   0x8366
#define GL_UNSIGNED_SHORT_5_6_5         0x8363
#define GL_UNSIGNED_INT_8_8_8_8_REV     0x8367
#define GL_BGRA                         0x80E1
#define GL_CLAMP_TO_EDGE                0x812F
#define GL_TEXTURE_MAX_LEVEL            0x813D
#define GL_TEXTURE0                     0x84C0
#define GL_SUBTRACT                     0x84E7
#define GL_COMBINE                      0x8570
#define GL_COMBINE_RGB                  0x8571
#define GL_COMBINE_ALPHA                0x8572
#define GL_RGB_SCALE                    0x8573
#define GL_ADD_SIGNED                   0x8574
#define GL_INTERPOLATE                  0x8575
#define GL_CONSTANT                     0x8576
#define GL_PRIMARY_COLOR                0x8577
#define GL_PREVIOUS                     0x8578
#define GL_SOURCE0_RGB                  0x8580
#define GL_SOURCE1_RGB                  0x8581
#define GL_SOURCE2_RGB                  0x8582
#define GL_SOURCE0_ALPHA                0x8588
#define GL_SOURCE1_ALPHA                0x8589
#define GL_SOURCE2_ALPHA                0x858A
#define GL_OPERAND0_RGB                 0x8590
#define GL_OPERAND1_RGB                 0x8591
#define GL_OPERAND2_RGB                 0x8592
#define GL_OPERAND0_ALPHA               0x8598
#define GL_OPERAND1_ALPHA               0x8599
#define GL_OPERAND2_ALPHA               0x859A
#define GL_ALPHA_SCALE                  0x0D1C
#define GL_ADD                          0x0104
#define GL_REPLACE                      0x1E01
#define GL_COLOR_SUM                    0x8458
#define GL_FOG_COORD_SRC                0x8450
#define GL_FOG_COORD                    0x8451
#define GL_FRAGMENT_DEPTH               0x8452
#define GL_FOG_COORD_ARRAY              0x8457
#define GL_SECONDARY_COLOR_ARRAY        0x845E
#define GL_FRAMEBUFFER                  0x8D40
#define GL_READ_FRAMEBUFFER             0x8CA8
#define GL_DRAW_FRAMEBUFFER             0x8CA9
#define GL_RENDERBUFFER                 0x8D41
#define GL_COLOR_ATTACHMENT0            0x8CE0
#define GL_DEPTH_ATTACHMENT             0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT     0x821A
#define GL_DEPTH24_STENCIL8             0x88F0
#define GL_RGBA8                        0x8058
#define GL_FRAMEBUFFER_COMPLETE         0x8CD5
#define GL_NO_ERROR                     0
#define GL_MAX_SAMPLES                  0x8D57
#define GL_SAMPLE_ALPHA_TO_COVERAGE     0x809E
#define GL_TEXTURE_MAX_ANISOTROPY       0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY   0x84FF

/* name, return type, parameter list: every function is a pointer `gl.name`. */
#define GL_FUNCTIONS(X) \
    X(Enable, void, (GLenum)) \
    X(Disable, void, (GLenum)) \
    X(Viewport, void, (GLint, GLint, GLsizei, GLsizei)) \
    X(Scissor, void, (GLint, GLint, GLsizei, GLsizei)) \
    X(Clear, void, (GLbitfield)) \
    X(ClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(ClearDepth, void, (GLdouble)) \
    X(BlendFunc, void, (GLenum, GLenum)) \
    X(DepthFunc, void, (GLenum)) \
    X(DepthMask, void, (GLboolean)) \
    X(ColorMask, void, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(AlphaFunc, void, (GLenum, GLfloat)) \
    X(CullFace, void, (GLenum)) \
    X(FrontFace, void, (GLenum)) \
    X(ShadeModel, void, (GLenum)) \
    X(PolygonOffset, void, (GLfloat, GLfloat)) \
    X(Fogi, void, (GLenum, GLint)) \
    X(Fogf, void, (GLenum, GLfloat)) \
    X(Fogfv, void, (GLenum, const GLfloat*)) \
    X(TexEnvi, void, (GLenum, GLenum, GLint)) \
    X(TexEnvfv, void, (GLenum, GLenum, const GLfloat*)) \
    X(TexParameteri, void, (GLenum, GLenum, GLint)) \
    X(TexParameterf, void, (GLenum, GLenum, GLfloat)) \
    X(GetIntegerv, void, (GLenum, GLint*)) \
    X(GetFloatv, void, (GLenum, GLfloat*)) \
    X(GenTextures, void, (GLsizei, GLuint*)) \
    X(DeleteTextures, void, (GLsizei, const GLuint*)) \
    X(BindTexture, void, (GLenum, GLuint)) \
    X(TexImage2D, void, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*)) \
    X(TexSubImage2D, void, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*)) \
    X(PixelStorei, void, (GLenum, GLint)) \
    X(ReadPixels, void, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*)) \
    X(MatrixMode, void, (GLenum)) \
    X(LoadMatrixf, void, (const GLfloat*)) \
    X(MultMatrixf, void, (const GLfloat*)) \
    X(LoadIdentity, void, (void)) \
    X(Ortho, void, (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble)) \
    X(DepthRange, void, (GLdouble, GLdouble)) \
    X(EnableClientState, void, (GLenum)) \
    X(DisableClientState, void, (GLenum)) \
    X(VertexPointer, void, (GLint, GLenum, GLsizei, const GLvoid*)) \
    X(ColorPointer, void, (GLint, GLenum, GLsizei, const GLvoid*)) \
    X(TexCoordPointer, void, (GLint, GLenum, GLsizei, const GLvoid*)) \
    X(DrawArrays, void, (GLenum, GLint, GLsizei)) \
    X(DrawElements, void, (GLenum, GLsizei, GLenum, const GLvoid*)) \
    X(Color4f, void, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(Finish, void, (void)) \
    X(GetError, GLenum, (void)) \
    X(GetString, const GLubyte*, (GLenum))

/* Functions past OpenGL 1.1, which come from wglGetProcAddress. */
#define GL_LATER_FUNCTIONS(X) \
    X(ActiveTexture, void, (GLenum)) \
    X(ClientActiveTexture, void, (GLenum)) \
    X(SecondaryColorPointer, void, (GLint, GLenum, GLsizei, const GLvoid*)) \
    X(FogCoordPointer, void, (GLenum, GLsizei, const GLvoid*)) \
    X(GenFramebuffers, void, (GLsizei, GLuint*)) \
    X(BindFramebuffer, void, (GLenum, GLuint)) \
    X(FramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GenRenderbuffers, void, (GLsizei, GLuint*)) \
    X(BindRenderbuffer, void, (GLenum, GLuint)) \
    X(RenderbufferStorage, void, (GLenum, GLenum, GLsizei, GLsizei)) \
    X(FramebufferRenderbuffer, void, (GLenum, GLenum, GLenum, GLuint)) \
    X(CheckFramebufferStatus, GLenum, (GLenum)) \
    X(BlitFramebuffer, void, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(DeleteFramebuffers, void, (GLsizei, const GLuint*)) \
    X(DeleteRenderbuffers, void, (GLsizei, const GLuint*)) \
    X(RenderbufferStorageMultisample, void, (GLenum, GLsizei, GLenum, GLsizei, GLsizei)) \
    X(GenerateMipmap, void, (GLenum))

/* OpenGL 2.0's shaders, for the picture's finish (post.c): looked for, but not needed to start. */
#define GL_SHADER_FUNCTIONS(X)     X(CreateShader, GLuint, (GLenum))     X(ShaderSource, void, (GLuint, GLsizei, const GLchar* const*, const GLint*))     X(CompileShader, void, (GLuint))     X(GetShaderiv, void, (GLuint, GLenum, GLint*))     X(GetShaderInfoLog, void, (GLuint, GLsizei, GLsizei*, GLchar*))     X(DeleteShader, void, (GLuint))     X(CreateProgram, GLuint, (void))     X(AttachShader, void, (GLuint, GLuint))     X(LinkProgram, void, (GLuint))     X(GetProgramiv, void, (GLuint, GLenum, GLint*))     X(UseProgram, void, (GLuint))     X(GetUniformLocation, GLint, (GLuint, const GLchar*))     X(Uniform1i, void, (GLint, GLint))     X(Uniform1f, void, (GLint, GLfloat))     X(Uniform2f, void, (GLint, GLfloat, GLfloat))

#define GL_POINTER(name, ret, args) ret (WINAPI *name) args;

typedef struct gl_api
{
    GL_FUNCTIONS(GL_POINTER)
    GL_LATER_FUNCTIONS(GL_POINTER)
    GL_SHADER_FUNCTIONS(GL_POINTER)
    BOOL shaders;                       /* all of GL_SHADER_FUNCTIONS were found */
} gl_api;

extern gl_api gl;

/* Create a context on `window` and load every function. FALSE, logged, if anything is missing. */
BOOL gl_start(HWND window);
void gl_present(void);

/*
    The player's graphics settings (in the dev build, its Options > Graphics). Without vsync the
    frames are still held to the display's refresh rate: the game's physics come apart uncapped.
    `samples` is the anti-aliasing's, 0 for none; `filtering` 0 is the game's own, 1 trilinear,
    and 2 to 4 anisotropic at 4, 8 and 16 times. `frame_limit` holds the frames to so many a
    second, 0 to the display's refresh rate.

    The picture's finish (post.c), each 0 for the picture as the game drew it: `fxaa` 1 smooths
    its edges after it is drawn; `sharpen`, how much its detail is sharpened, and `vibrance`, how
    much its dull colours are deepened, in percent; `brightness` in percent, 100 as drawn; `bloom`,
    how much the brightest light glows into what is around it, in percent, and only in a race.
*/
typedef struct render_options {
    BOOL vsync;
    int  samples;
    int  filtering;
    int  frame_limit;
    int  fxaa;
    int  sharpen;
    int  vibrance;
    int  brightness;
    int  bloom;
} render_options;

extern render_options g_render;

/* g_render.vsync, set again. */
void gl_vsync_apply(void);

/* The picture `texture` (w x h) drawn finished into the window's x, y, fit_w x fit_h; FALSE when
   it is to be drawn as it is: its finish all off, or no shaders (post.c). */
BOOL post_draw(GLuint texture, int w, int h, int x, int y, int fit_w, int fit_h);

/* Whether a race is on screen (the dev build's HUD knows): the menus are never given a glow. */
extern BOOL g_post_race;
