/*
    OpenMCM2 - the picture's finish: the finished frame drawn into the window through one shader
    that can smooth its edges (FXAA, after Timothy Lottes's), sharpen its detail (contrast
    adaptive, after AMD's FidelityFX CAS), deepen its dull colours and brighten or darken it.
    With all of them off, or no shaders on the card, the frame is copied across as it was.
*/

#include "gl.h"

#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82

static const char* const vertex_source =
    "#version 120\n"
    "varying vec2 at;\n"
    "void main() { at = gl_MultiTexCoord0.xy; gl_Position = gl_Vertex; }\n";

static const char* const fragment_source =
    "#version 120\n"
    "uniform sampler2D picture;\n"
    "uniform vec2 texel;\n"
    "uniform float fxaa, sharpen, vibrance, gamma;\n"
    "varying vec2 at;\n"
    "vec3 tap(vec2 offset) { return texture2D(picture, at + offset * texel).rgb; }\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "void main() {\n"
    "    vec3 middle = tap(vec2(0.0));\n"
    "    vec3 colour = middle;\n"
    "    if (fxaa > 0.5) {\n"
    "        float nw = luma(tap(vec2(-1.0, -1.0))), ne = luma(tap(vec2(1.0, -1.0)));\n"
    "        float sw = luma(tap(vec2(-1.0, 1.0))), se = luma(tap(vec2(1.0, 1.0)));\n"
    "        float m = luma(middle);\n"
    "        float least = min(m, min(min(nw, ne), min(sw, se)));\n"
    "        float most = max(m, max(max(nw, ne), max(sw, se)));\n"
    "        vec2 across = vec2((sw + se) - (nw + ne), (nw + sw) - (ne + se));\n"
    "        float reduce = max((nw + ne + sw + se) * (0.25 / 8.0), 1.0 / 128.0);\n"
    "        float scale = 1.0 / (min(abs(across.x), abs(across.y)) + reduce);\n"
    "        vec2 step = clamp(across * scale, vec2(-8.0), vec2(8.0)) * texel;\n"
    "        vec3 near = 0.5 * (texture2D(picture, at - step / 6.0).rgb + texture2D(picture, at + step / 6.0).rgb);\n"
    "        vec3 wide = 0.5 * near + 0.25 * (texture2D(picture, at - step * 0.5).rgb\n"
    "                                         + texture2D(picture, at + step * 0.5).rgb);\n"
    "        float w = luma(wide);\n"
    "        colour = w < least || w > most ? near : wide;\n"
    "    }\n"
    "    if (sharpen > 0.0) {\n"
    "        vec3 n = tap(vec2(0.0, -1.0)), s = tap(vec2(0.0, 1.0)), w = tap(vec2(-1.0, 0.0)), e = tap(vec2(1.0, 0.0));\n"
    "        vec3 least = min(middle, min(min(n, s), min(w, e)));\n"
    "        vec3 most = max(middle, max(max(n, s), max(w, e)));\n"
    "        vec3 room = sqrt(clamp(min(least, 2.0 - most) / max(most, vec3(0.0001)), 0.0, 1.0));\n"
    "        vec3 weight = room * (-1.0 / mix(8.0, 5.0, sharpen));\n"
    "        vec3 sharp = (middle + (n + s + w + e) * weight) / (1.0 + 4.0 * weight);\n"
    "        colour += clamp(sharp, 0.0, 1.0) - middle;\n"
    "    }\n"
    "    if (vibrance > 0.0) {\n"
    "        float grey = luma(colour);\n"
    "        float saturation = max(colour.r, max(colour.g, colour.b)) - min(colour.r, min(colour.g, colour.b));\n"
    "        colour = mix(vec3(grey), colour, 1.0 + vibrance * (1.0 - saturation));\n"
    "    }\n"
    "    colour = pow(clamp(colour, 0.0, 1.0), vec3(gamma));\n"
    "    gl_FragColor = vec4(colour, 1.0);\n"
    "}\n";

static GLuint g_program;
static BOOL   g_failed;
static GLint  g_texel, g_fxaa, g_sharpen, g_vibrance, g_gamma;

static GLuint compile(GLenum kind, const char* source)
{
    GLuint shader = gl.CreateShader(kind);
    GLint done = 0;
    char why[512];

    gl.ShaderSource(shader, 1, &source, NULL);
    gl.CompileShader(shader);
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &done);
    if (!done) {
        gl.GetShaderInfoLog(shader, sizeof why, NULL, why);
        log_line("post: a shader would not compile: %s", why);
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

static BOOL program_ready(void)
{
    GLuint vertex, fragment;
    GLint linked = 0;

    if (g_program || g_failed)
        return g_program != 0;
    g_failed = TRUE;
    if (!gl.shaders)
        return FALSE;
    vertex = compile(GL_VERTEX_SHADER, vertex_source);
    fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment)
        return FALSE;
    g_program = gl.CreateProgram();
    gl.AttachShader(g_program, vertex);
    gl.AttachShader(g_program, fragment);
    gl.LinkProgram(g_program);
    gl.GetProgramiv(g_program, GL_LINK_STATUS, &linked);
    gl.DeleteShader(vertex);
    gl.DeleteShader(fragment);
    if (!linked) {
        log_line("post: the shader would not link");
        g_program = 0;
        return FALSE;
    }
    gl.UseProgram(g_program);
    gl.Uniform1i(gl.GetUniformLocation(g_program, "picture"), 0);
    g_texel = gl.GetUniformLocation(g_program, "texel");
    g_fxaa = gl.GetUniformLocation(g_program, "fxaa");
    g_sharpen = gl.GetUniformLocation(g_program, "sharpen");
    g_vibrance = gl.GetUniformLocation(g_program, "vibrance");
    g_gamma = gl.GetUniformLocation(g_program, "gamma");
    gl.UseProgram(0);
    g_failed = FALSE;
    log_line("post: the picture's finish is ready");
    return TRUE;
}

BOOL post_draw(GLuint texture, int w, int h, int x, int y, int fit_w, int fit_h)
{
    static const GLfloat corners[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    static const GLfloat places[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    float brightness = g_render.brightness > 0 ? g_render.brightness / 100.0f : 1;

    if ((!g_render.fxaa && !g_render.sharpen && !g_render.vibrance && g_render.brightness == 100)
        || w <= 0 || h <= 0 || !program_ready())
        return FALSE;

    gl.Viewport(x, y, fit_w, fit_h);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_BLEND);
    gl.Disable(GL_ALPHA_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_FOG);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, texture);

    gl.UseProgram(g_program);
    gl.Uniform2f(g_texel, 1.0f / w, 1.0f / h);
    gl.Uniform1f(g_fxaa, g_render.fxaa ? 1.0f : 0.0f);
    gl.Uniform1f(g_sharpen, g_render.sharpen / 100.0f);
    gl.Uniform1f(g_vibrance, g_render.vibrance / 100.0f);
    gl.Uniform1f(g_gamma, 1 / brightness);      /* the picture's middle tones raised, its ends kept */

    gl.DisableClientState(GL_COLOR_ARRAY);         /* the game's last draw may have left them on */
    gl.DisableClientState(GL_FOG_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0 + 1);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0);
    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.VertexPointer(2, GL_FLOAT, 0, corners);
    gl.TexCoordPointer(2, GL_FLOAT, 0, places);
    gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.DisableClientState(GL_VERTEX_ARRAY);
    gl.UseProgram(0);
    return TRUE;
}
