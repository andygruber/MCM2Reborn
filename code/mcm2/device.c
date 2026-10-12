/*
    OpenMCM2 - Direct3D 7 on OpenGL: the Direct3D object, the device and vertex buffers. See render.h.

    Direct3D's fixed-function pipeline maps onto OpenGL's almost one to one. Its row-vector matrices
    are OpenGL's column-major ones as they stand; its depth range of 0..1 is folded into the
    projection; and its texture stages become texture-environment combiners. The game never lights
    anything, so lighting is not implemented.
*/

#include "render.h"

#include <intrin.h>

HRESULT unsupported(const char* method)
{
    static const char* said[64];
    int i;

    for (i = 0; i < 64 && said[i]; ++i)
    {
        if (said[i] == method)
            return DDERR_UNSUPPORTED;
    }
    if (i < 64)
        said[i] = method;
    log_line("render: %s is not implemented", method);
    return DDERR_UNSUPPORTED;
}

HRESULT no_interface(const char* object, REFIID iid, void** out)
{
    log_line("render: %s has no interface {%08X-%04X-%04X-%02X%02X}", object, (unsigned) iid->Data1,
             iid->Data2, iid->Data3, iid->Data4[0], iid->Data4[1]);
    *out = NULL;
    return E_NOINTERFACE;
}

typedef union { DWORD u; float f; } bits;

/* ------------------------------------------------------------------ vertex buffers */

typedef struct vertex_buffer
{
    IDirect3DVertexBuffer7 iface;
    LONG                   refs;
    D3DVERTEXBUFFERDESC    desc;
    BYTE*                  data;
    float*                 clip;        /* ProcessVertices output, in clip space: see draw_buffer */
    BOOL                   processed;
} vertex_buffer;

/* Bytes per vertex of a flexible vertex format. */
static DWORD fvf_size(DWORD fvf)
{
    DWORD size = 0;

    if (fvf & D3DFVF_XYZRHW)
        size += 16;
    else if (fvf & D3DFVF_XYZ)
        size += 12;
    if (fvf & D3DFVF_RESERVED1) size += 4;
    if (fvf & D3DFVF_NORMAL) size += 12;
    if (fvf & D3DFVF_DIFFUSE) size += 4;
    if (fvf & D3DFVF_SPECULAR) size += 4;
    return size + ((fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT) * 8;
}

/* Where each part of a vertex is, as a byte offset; -1 for absent. */
typedef struct layout
{
    DWORD stride;
    int   diffuse, specular, tex[2];
    BOOL  pretransformed;
} layout;

static layout layout_of(DWORD fvf)
{
    layout l;
    int at = (fvf & D3DFVF_XYZRHW) ? 16 : 12, n, i;

    l.pretransformed = (fvf & D3DFVF_XYZRHW) != 0;
    if (fvf & D3DFVF_RESERVED1) at += 4;
    if (fvf & D3DFVF_NORMAL) at += 12;
    l.diffuse = (fvf & D3DFVF_DIFFUSE) ? at : -1;
    if (fvf & D3DFVF_DIFFUSE) at += 4;
    l.specular = (fvf & D3DFVF_SPECULAR) ? at : -1;
    if (fvf & D3DFVF_SPECULAR) at += 4;
    n = (int) ((fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT);
    for (i = 0; i < 2; ++i)
        l.tex[i] = i < n ? at + i * 8 : -1;
    l.stride = fvf_size(fvf);
    return l;
}

/* ------------------------------------------------------------------ the device */

typedef struct device
{
    IDirect3DDevice7 iface;
    LONG             refs;
    IDirect3D7*      d3d;
    surface*         target;
    DWORD            rs[256];
    DWORD            tss[8][32];
    surface*         texture[8];
    D3DMATRIX        world, view, projection;
    D3DVIEWPORT7     viewport;
    D3DMATERIAL7     material;

    /* What one draw hands OpenGL: position, colour, two texture coordinates. */
    float*           vertices;
    DWORD            capacity;
} device;

#define FLOATS_PER_VERTEX 9     /* x y z w, colour (four bytes in one slot), u0 v0 u1 v1 */

static GLuint g_white;          /* a 1x1 white texture, for stages that read no texture */

static void identity(D3DMATRIX* m)
{
    __stosb((unsigned char*) m, 0, sizeof *m);
    m->_11 = m->_22 = m->_33 = m->_44 = 1;
}

static void multiply(D3DMATRIX* out, const D3DMATRIX* a, const D3DMATRIX* b)
{
    const float* x = &a->_11;
    const float* y = &b->_11;
    float r[16];
    int i, j;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            r[i * 4 + j] = x[i * 4] * y[j] + x[i * 4 + 1] * y[4 + j] + x[i * 4 + 2] * y[8 + j] + x[i * 4 + 3] * y[12 + j];
    __movsb((unsigned char*) out, (const unsigned char*) r, sizeof r);
}

static void set_defaults(device* d)
{
    bits one;
    int i;

    one.f = 1.0f;
    d->rs[D3DRENDERSTATE_ZENABLE] = D3DZB_TRUE;
    d->rs[D3DRENDERSTATE_FILLMODE] = D3DFILL_SOLID;
    d->rs[D3DRENDERSTATE_SHADEMODE] = D3DSHADE_GOURAUD;
    d->rs[D3DRENDERSTATE_ZWRITEENABLE] = TRUE;
    d->rs[D3DRENDERSTATE_SRCBLEND] = D3DBLEND_ONE;
    d->rs[D3DRENDERSTATE_DESTBLEND] = D3DBLEND_ZERO;
    d->rs[D3DRENDERSTATE_CULLMODE] = D3DCULL_CCW;
    d->rs[D3DRENDERSTATE_ZFUNC] = D3DCMP_LESSEQUAL;
    d->rs[D3DRENDERSTATE_ALPHAFUNC] = D3DCMP_ALWAYS;
    d->rs[D3DRENDERSTATE_FOGEND] = one.u;
    d->rs[D3DRENDERSTATE_FOGDENSITY] = one.u;
    d->rs[D3DRENDERSTATE_TEXTUREFACTOR] = 0xFFFFFFFF;
    d->rs[D3DRENDERSTATE_LIGHTING] = TRUE;
    d->rs[D3DRENDERSTATE_CLIPPING] = TRUE;

    for (i = 0; i < 8; ++i)
    {
        d->tss[i][D3DTSS_COLOROP] = i ? D3DTOP_DISABLE : D3DTOP_MODULATE;
        d->tss[i][D3DTSS_COLORARG1] = D3DTA_TEXTURE;
        d->tss[i][D3DTSS_COLORARG2] = D3DTA_CURRENT;
        d->tss[i][D3DTSS_ALPHAOP] = i ? D3DTOP_DISABLE : D3DTOP_SELECTARG1;
        d->tss[i][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
        d->tss[i][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
        d->tss[i][D3DTSS_TEXCOORDINDEX] = (DWORD) i;
        d->tss[i][D3DTSS_ADDRESSU] = D3DTADDRESS_WRAP;
        d->tss[i][D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
        d->tss[i][D3DTSS_MAGFILTER] = D3DTFG_POINT;
        d->tss[i][D3DTSS_MINFILTER] = D3DTFN_POINT;
        d->tss[i][D3DTSS_MIPFILTER] = D3DTFP_NONE;
    }

    identity(&d->world);
    identity(&d->view);
    identity(&d->projection);
    d->viewport.dwWidth = d->target ? d->target->desc.dwWidth : 640;
    d->viewport.dwHeight = d->target ? d->target->desc.dwHeight : 480;
    d->viewport.dvMaxZ = 1;
}

/* ------------------------------------------------------------------ state, mapped onto OpenGL */

static GLenum compare(DWORD f)
{
    return (f >= D3DCMP_NEVER && f <= D3DCMP_ALWAYS) ? GL_NEVER + (f - D3DCMP_NEVER) : GL_NEVER + 7;
}

static GLenum blend_factor(DWORD b, BOOL source)
{
    static const GLenum k[] = {
        0, 0 /* ZERO */, 1 /* ONE */, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA_SATURATE,
    };

    if (b == D3DBLEND_BOTHSRCALPHA)
        return source ? GL_SRC_ALPHA : GL_ONE_MINUS_SRC_ALPHA;
    if (b == D3DBLEND_BOTHINVSRCALPHA)
        return source ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
    return b < sizeof k / sizeof k[0] ? k[b] : 1;
}

static void colour_of(DWORD argb, float* rgba)
{
    rgba[0] = (argb >> 16 & 255) / 255.0f;
    rgba[1] = (argb >> 8 & 255) / 255.0f;
    rgba[2] = (argb & 255) / 255.0f;
    rgba[3] = (argb >> 24) / 255.0f;
}

static void apply_states(device* d)
{
    const DWORD* rs = d->rs;
    bits start, end, density;

    target_bind(d->target);

    if (rs[D3DRENDERSTATE_ZENABLE])
        gl.Enable(GL_DEPTH_TEST);
    else
        gl.Disable(GL_DEPTH_TEST);
    gl.DepthFunc(compare(rs[D3DRENDERSTATE_ZFUNC]));
    gl.DepthMask(rs[D3DRENDERSTATE_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);

    if (rs[D3DRENDERSTATE_ALPHABLENDENABLE])
    {
        gl.Enable(GL_BLEND);
        gl.BlendFunc(blend_factor(rs[D3DRENDERSTATE_SRCBLEND], TRUE), blend_factor(rs[D3DRENDERSTATE_DESTBLEND], FALSE));
    }
    else
    {
        gl.Disable(GL_BLEND);
    }

    if (rs[D3DRENDERSTATE_ALPHATESTENABLE])
    {
        gl.Enable(GL_ALPHA_TEST);
        gl.AlphaFunc(compare(rs[D3DRENDERSTATE_ALPHAFUNC]), (rs[D3DRENDERSTATE_ALPHAREF] & 255) / 255.0f);
    }
    else
    {
        gl.Disable(GL_ALPHA_TEST);
    }

    /* Both APIs judge winding as the viewer sees it, and nothing here flips the image, so
       D3DCULL_CCW culls the faces OpenGL also calls counter-clockwise: those are its back faces
       when the front is clockwise. */
    if (rs[D3DRENDERSTATE_CULLMODE] == D3DCULL_NONE)
    {
        gl.Disable(GL_CULL_FACE);
    }
    else
    {
        gl.Enable(GL_CULL_FACE);
        gl.CullFace(GL_BACK);
        gl.FrontFace(rs[D3DRENDERSTATE_CULLMODE] == D3DCULL_CCW ? GL_CW : GL_CCW);
    }

    gl.ShadeModel(rs[D3DRENDERSTATE_SHADEMODE] == D3DSHADE_FLAT ? GL_FLAT : GL_SMOOTH);
    gl.Disable(GL_SCISSOR_TEST);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* Table fog only; vertex fog would need the fog coordinate, and the game has not asked for it. */
    if (rs[D3DRENDERSTATE_FOGENABLE] && rs[D3DRENDERSTATE_FOGTABLEMODE] != D3DFOG_NONE)
    {
        float colour[4];
        static const GLenum mode[] = { 0, GL_EXP, GL_EXP2, GL_LINEAR };

        start.u = rs[D3DRENDERSTATE_FOGSTART];
        end.u = rs[D3DRENDERSTATE_FOGEND];
        density.u = rs[D3DRENDERSTATE_FOGDENSITY];
        colour_of(rs[D3DRENDERSTATE_FOGCOLOR], colour);
        gl.Enable(GL_FOG);
        gl.Fogi(GL_FOG_MODE, (GLint) mode[rs[D3DRENDERSTATE_FOGTABLEMODE] & 3]);
        gl.Fogf(GL_FOG_START, start.f);
        gl.Fogf(GL_FOG_END, end.f);
        gl.Fogf(GL_FOG_DENSITY, density.f);
        gl.Fogfv(GL_FOG_COLOR, colour);
    }
    else
    {
        if (rs[D3DRENDERSTATE_FOGENABLE])
            unsupported("vertex fog");
        gl.Disable(GL_FOG);
    }
}

/* One texture-stage argument as a combiner source and operand. */
static void stage_argument(DWORD arg, int stage, BOOL alpha, GLenum* source, GLenum* operand)
{
    switch (arg & D3DTA_SELECTMASK)
    {
    case D3DTA_TEXTURE:  *source = GL_TEXTURE; break;
    case D3DTA_TFACTOR:  *source = GL_CONSTANT; break;
    case D3DTA_CURRENT:  *source = stage ? GL_PREVIOUS : GL_PRIMARY_COLOR; break;
    default:             *source = GL_PRIMARY_COLOR; break;     /* DIFFUSE, and SPECULAR as near enough */
    }

    if (alpha || (arg & D3DTA_ALPHAREPLICATE))
        *operand = (arg & D3DTA_COMPLEMENT) ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
    else
        *operand = (arg & D3DTA_COMPLEMENT) ? GL_ONE_MINUS_SRC_COLOR : GL_SRC_COLOR;
}

/* One stage's colour or alpha operation as a combiner. */
static void stage_combiner(DWORD op, DWORD arg1, DWORD arg2, int stage, BOOL alpha)
{
    GLenum combine = alpha ? GL_COMBINE_ALPHA : GL_COMBINE_RGB;
    GLenum source0 = alpha ? GL_SOURCE0_ALPHA : GL_SOURCE0_RGB;
    GLenum operand0 = alpha ? GL_OPERAND0_ALPHA : GL_OPERAND0_RGB;
    GLenum scale = alpha ? GL_ALPHA_SCALE : GL_RGB_SCALE;
    GLenum mode = GL_MODULATE, s, o, blend = 0;
    DWORD first = arg1, second = arg2;
    int factor = 1;

    switch (op)
    {
    case D3DTOP_SELECTARG1:         mode = GL_REPLACE; break;
    case D3DTOP_SELECTARG2:         mode = GL_REPLACE; first = arg2; break;
    case D3DTOP_MODULATE2X:         factor = 2; break;
    case D3DTOP_MODULATE4X:         factor = 4; break;
    case D3DTOP_ADD:
    case D3DTOP_ADDSMOOTH:          mode = GL_ADD; break;
    case D3DTOP_ADDSIGNED:          mode = GL_ADD_SIGNED; break;
    case D3DTOP_ADDSIGNED2X:        mode = GL_ADD_SIGNED; factor = 2; break;
    case D3DTOP_SUBTRACT:           mode = GL_SUBTRACT; break;
    case D3DTOP_BLENDDIFFUSEALPHA:  mode = GL_INTERPOLATE; blend = GL_PRIMARY_COLOR; break;
    case D3DTOP_BLENDTEXTUREALPHA:
    case D3DTOP_BLENDTEXTUREALPHAPM: mode = GL_INTERPOLATE; blend = GL_TEXTURE; break;
    case D3DTOP_BLENDFACTORALPHA:   mode = GL_INTERPOLATE; blend = GL_CONSTANT; break;
    case D3DTOP_BLENDCURRENTALPHA:  mode = GL_INTERPOLATE; blend = stage ? GL_PREVIOUS : GL_PRIMARY_COLOR; break;
    case D3DTOP_MODULATE:           break;
    default:                        unsupported("a texture stage operation"); break;
    }

    gl.TexEnvi(GL_TEXTURE_ENV, combine, (GLint) mode);
    gl.TexEnvi(GL_TEXTURE_ENV, scale, factor);

    stage_argument(first, stage, alpha, &s, &o);
    gl.TexEnvi(GL_TEXTURE_ENV, source0, (GLint) s);
    gl.TexEnvi(GL_TEXTURE_ENV, operand0, (GLint) o);

    stage_argument(second, stage, alpha, &s, &o);
    gl.TexEnvi(GL_TEXTURE_ENV, source0 + 1, (GLint) s);
    gl.TexEnvi(GL_TEXTURE_ENV, operand0 + 1, (GLint) o);

    if (blend)
    {
        gl.TexEnvi(GL_TEXTURE_ENV, source0 + 2, (GLint) blend);
        gl.TexEnvi(GL_TEXTURE_ENV, operand0 + 2, GL_SRC_ALPHA);
    }
}

static GLint wrap(DWORD address)
{
    return address == D3DTADDRESS_WRAP ? GL_REPEAT : address == D3DTADDRESS_MIRROR ? 0x8370 /* MIRRORED_REPEAT */
                                                                                   : GL_CLAMP_TO_EDGE;
}

static void apply_stages(device* d)
{
    float factor[4];
    int stage, active = 1;

    colour_of(d->rs[D3DRENDERSTATE_TEXTUREFACTOR], factor);

    for (stage = 0; stage < 2; ++stage)
    {
        const DWORD* t = d->tss[stage];

        gl.ActiveTexture(GL_TEXTURE0 + stage);
        if (!active || t[D3DTSS_COLOROP] == D3DTOP_DISABLE)
        {
            gl.Disable(GL_TEXTURE_2D);
            active = 0;
            continue;
        }

        gl.Enable(GL_TEXTURE_2D);
        if (d->texture[stage])
        {
            BOOL mips = d->texture[stage]->top->mip != NULL && t[D3DTSS_MIPFILTER] != D3DTFP_NONE;
            BOOL linear = t[D3DTSS_MINFILTER] != D3DTFN_POINT;
            GLint min = !mips ? (linear ? GL_LINEAR : GL_NEAREST)
                      : t[D3DTSS_MIPFILTER] == D3DTFP_LINEAR ? (linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR)
                                                             : (linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST);

            texture_bind(d->texture[stage]);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t[D3DTSS_MAGFILTER] == D3DTFG_POINT ? GL_NEAREST : GL_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap(t[D3DTSS_ADDRESSU]));
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap(t[D3DTSS_ADDRESSV]));
        }
        else
        {
            gl.BindTexture(GL_TEXTURE_2D, g_white);     /* a missing texture reads as white */
        }

        gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        gl.TexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, factor);
        stage_combiner(t[D3DTSS_COLOROP], t[D3DTSS_COLORARG1], t[D3DTSS_COLORARG2], stage, FALSE);
        if (t[D3DTSS_ALPHAOP] == D3DTOP_DISABLE)
            stage_combiner(D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT, stage, TRUE);
        else
            stage_combiner(t[D3DTSS_ALPHAOP], t[D3DTSS_ALPHAARG1], t[D3DTSS_ALPHAARG2], stage, TRUE);
    }
    gl.ActiveTexture(GL_TEXTURE0);
}

/* The projection for untransformed vertices: the game's, with Direct3D's 0..1 depth range moved to
   OpenGL's -1..1 (z' = 2z - w). */
static void load_projection(const D3DMATRIX* p)
{
    static const GLfloat depth_fix[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 2, 0,  0, 0, -1, 1 };

    gl.MatrixMode(GL_PROJECTION);
    gl.LoadMatrixf(depth_fix);
    gl.MultMatrixf(&p->_11);
}

/* Pre-transformed vertices are already in pixels: map the target's pixels straight to clip space. */
static void setup_pretransformed(device* d)
{
    gl.Viewport(0, 0, d->target->desc.dwWidth, d->target->desc.dwHeight);
    gl.DepthRange(0, 1);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, d->target->desc.dwWidth, d->target->desc.dwHeight, 0, 0, -1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
}

static void setup_viewport(device* d)
{
    const D3DVIEWPORT7* v = &d->viewport;

    gl.Viewport((GLint) v->dwX, (GLint) d->target->desc.dwHeight - (GLint) (v->dwY + v->dwHeight),
                (GLsizei) v->dwWidth, (GLsizei) v->dwHeight);
    gl.DepthRange(v->dvMinZ, v->dvMaxZ);
}

static void setup_transformed(device* d)
{
    setup_viewport(d);
    load_projection(&d->projection);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadMatrixf(&d->view._11);
    gl.MultMatrixf(&d->world._11);
}

/* ------------------------------------------------------------------ drawing */

static GLenum primitive(D3DPRIMITIVETYPE type)
{
    switch (type)
    {
    case D3DPT_POINTLIST:     return GL_POINTS;
    case D3DPT_LINELIST:      return GL_LINES;
    case D3DPT_LINESTRIP:     return GL_LINE_STRIP;
    case D3DPT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN:   return GL_TRIANGLE_FAN;
    default:                  return GL_TRIANGLES;
    }
}

static float* reserve(device* d, DWORD count)
{
    if (count > d->capacity)
    {
        mem_free(d->vertices);
        d->capacity = count + 1024;
        d->vertices = (float*) mem_alloc((SIZE_T) d->capacity * FLOATS_PER_VERTEX * sizeof(float));
        if (!d->vertices)
            d->capacity = 0;
    }
    return d->vertices;
}

/* Direct3D's ARGB colour as the RGBA bytes OpenGL reads. Stored as a DWORD, never through a
   float: some colours are NaN patterns, which an x87 load would change. */
static DWORD rgba_of(DWORD argb)
{
    return (argb >> 16 & 255) | (argb & 0xFF00) | (argb & 255) << 16 | (argb & 0xFF000000);
}

/* Convert `count` vertices into the arrays OpenGL draws from. `clip`, if given, is the position in
   clip space for each vertex, from ProcessVertices. */
static float* convert(device* d, const BYTE* from, DWORD count, DWORD fvf, const float* clip)
{
    layout l = layout_of(fvf);
    float* out = reserve(d, count);
    DWORD i;

    if (!out)
        return NULL;

    for (i = 0; i < count; ++i, from += l.stride)
    {
        const float* p = (const float*) from;
        float* v = out + i * FLOATS_PER_VERTEX;
        int t;

        if (clip)
        {
            v[0] = clip[i * 4]; v[1] = clip[i * 4 + 1]; v[2] = clip[i * 4 + 2]; v[3] = clip[i * 4 + 3];
        }
        else if (l.pretransformed)
        {
            /* x, y, z over rhw, and w = 1 / rhw, so OpenGL interpolates with perspective. */
            float w = p[3] != 0 ? 1.0f / p[3] : 1.0f;
            v[0] = p[0] * w; v[1] = p[1] * w; v[2] = p[2] * w; v[3] = w;
        }
        else
        {
            v[0] = p[0]; v[1] = p[1]; v[2] = p[2]; v[3] = 1;
        }

        ((DWORD*) v)[4] = rgba_of(l.diffuse >= 0 ? *(const DWORD*) (from + l.diffuse) : 0xFFFFFFFF);

        for (t = 0; t < 2; ++t)
        {
            int set = (int) d->tss[t][D3DTSS_TEXCOORDINDEX] & 7;
            const float* uv = (set < 2 && l.tex[set] >= 0) ? (const float*) (from + l.tex[set]) : NULL;

            v[5 + t * 2] = uv ? uv[0] : 0;
            v[6 + t * 2] = uv ? uv[1] : 0;
        }
    }
    return out;
}

static void submit(device* d, D3DPRIMITIVETYPE type, const float* v, DWORD count, const WORD* indices, DWORD index_count)
{
    GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);

    apply_states(d);
    apply_stages(d);

    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.EnableClientState(GL_COLOR_ARRAY);
    gl.VertexPointer(4, GL_FLOAT, stride, v);
    gl.ColorPointer(4, GL_UNSIGNED_BYTE, stride, v + 4);
    gl.ClientActiveTexture(GL_TEXTURE0);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.TexCoordPointer(2, GL_FLOAT, stride, v + 5);
    gl.ClientActiveTexture(GL_TEXTURE0 + 1);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.TexCoordPointer(2, GL_FLOAT, stride, v + 7);
    gl.ClientActiveTexture(GL_TEXTURE0);

    if (indices)
        gl.DrawElements(primitive(type), (GLsizei) index_count, GL_UNSIGNED_SHORT, indices);
    else
        gl.DrawArrays(primitive(type), 0, (GLsizei) count);
    (void) count;
    g_state_lost = FALSE;
}

/* Vertices from the game's memory, in whatever format it names. */
static HRESULT draw(device* d, D3DPRIMITIVETYPE type, DWORD fvf, const BYTE* vertices, DWORD count,
                    const WORD* indices, DWORD index_count)
{
    float* v;

    if (!d->target)
        return D3DERR_SCENE_NOT_IN_SCENE;

    target_bind(d->target);
    v = convert(d, vertices, count, fvf, NULL);
    if (!v)
        return DDERR_OUTOFMEMORY;

    if (fvf & D3DFVF_XYZRHW)
        setup_pretransformed(d);
    else
        setup_transformed(d);
    submit(d, type, v, count, indices, index_count);
    return D3D_OK;
}

/* Vertices ProcessVertices transformed: drawn from their clip-space positions, so OpenGL clips
   whatever crosses the camera, as Direct3D would. */
static HRESULT draw_buffer(device* d, D3DPRIMITIVETYPE type, vertex_buffer* vb, DWORD start, DWORD count,
                           const WORD* indices, DWORD index_count)
{
    layout l = layout_of(vb->desc.dwFVF);
    float* v;

    if (!d->target)
        return D3DERR_SCENE_NOT_IN_SCENE;
    if (start + count > vb->desc.dwNumVertices)
        return DDERR_INVALIDPARAMS;

    target_bind(d->target);
    if (vb->processed)
    {
        /* Already in OpenGL's clip space, depth included (see b_ProcessVertices). */
        v = convert(d, vb->data + start * l.stride, count, vb->desc.dwFVF, vb->clip + start * 4);
        setup_viewport(d);
        gl.MatrixMode(GL_PROJECTION);
        gl.LoadIdentity();
        gl.MatrixMode(GL_MODELVIEW);
        gl.LoadIdentity();
    }
    else
    {
        v = convert(d, vb->data + start * l.stride, count, vb->desc.dwFVF, NULL);
        if (l.pretransformed)
            setup_pretransformed(d);
        else
            setup_transformed(d);
    }

    if (!v)
        return DDERR_OUTOFMEMORY;
    submit(d, type, v, count, indices, index_count);
    return D3D_OK;
}

/* ------------------------------------------------------------------ IDirect3DDevice7 */

#define DEV ((device*) self)

static HRESULT WINAPI v_QueryInterface(IDirect3DDevice7* self, REFIID iid, void** out)
{
    if (IsEqualGUID(iid, &IID_IDirect3DDevice7) || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++DEV->refs;
        return S_OK;
    }
    return no_interface("the device", iid, out);
}

static ULONG WINAPI v_AddRef(IDirect3DDevice7* self)
{
    return (ULONG) ++DEV->refs;
}

static ULONG WINAPI v_Release(IDirect3DDevice7* self)
{
    LONG refs = --DEV->refs;
    int i;

    if (refs == 0)
    {
        for (i = 0; i < 8; ++i)
        {
            if (DEV->texture[i])
                DEV->texture[i]->iface.lpVtbl->Release(&DEV->texture[i]->iface);
        }
        if (DEV->target)
            DEV->target->iface.lpVtbl->Release(&DEV->target->iface);
        mem_free(DEV->vertices);
        mem_free(self);
    }
    return (ULONG) refs;
}

static void fill_device_desc(D3DDEVICEDESC7* desc)
{
    D3DPRIMCAPS tri;

    __stosb((unsigned char*) desc, 0, sizeof *desc);
    __stosb((unsigned char*) &tri, 0, sizeof tri);
    tri.dwSize = sizeof tri;
    tri.dwMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW;
    tri.dwRasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGTABLE |
                       D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZBIAS;
    tri.dwZCmpCaps = tri.dwAlphaCmpCaps = 0xFF;
    tri.dwSrcBlendCaps = tri.dwDestBlendCaps = 0x1FFF;
    tri.dwShadeCaps = D3DPSHADECAPS_COLORFLATRGB | D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_ALPHAGOURAUDBLEND |
                      D3DPSHADECAPS_FOGGOURAUD;
    tri.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_TRANSPARENCY;
    tri.dwTextureFilterCaps = D3DPTFILTERCAPS_NEAREST | D3DPTFILTERCAPS_LINEAR | D3DPTFILTERCAPS_MIPNEAREST |
                              D3DPTFILTERCAPS_MIPLINEAR | D3DPTFILTERCAPS_LINEARMIPNEAREST | D3DPTFILTERCAPS_LINEARMIPLINEAR |
                              D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MIPFPOINT |
                              D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR;
    tri.dwTextureBlendCaps = D3DPTBLENDCAPS_MODULATE | D3DPTBLENDCAPS_MODULATEALPHA | D3DPTBLENDCAPS_DECAL;
    tri.dwTextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR | D3DPTADDRESSCAPS_CLAMP |
                               D3DPTADDRESSCAPS_INDEPENDENTUV;

    desc->dwDevCaps = D3DDEVCAPS_FLOATTLVERTEX | D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
                      D3DDEVCAPS_TEXTUREVIDEOMEMORY | D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_CANRENDERAFTERFLIP |
                      D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX | D3DDEVCAPS_HWRASTERIZATION;
    desc->dpcLineCaps = tri;
    desc->dpcTriCaps = tri;
    desc->dwDeviceRenderBitDepth = DDBD_16 | DDBD_32;
    desc->dwDeviceZBufferBitDepth = DDBD_16 | DDBD_24;
    desc->dwMinTextureWidth = desc->dwMinTextureHeight = 1;
    desc->dwMaxTextureWidth = desc->dwMaxTextureHeight = 2048;
    desc->dwMaxTextureRepeat = 32768;
    desc->dwMaxTextureAspectRatio = 2048;
    desc->dwMaxAnisotropy = 1;
    desc->dvGuardBandLeft = desc->dvGuardBandTop = -8192;
    desc->dvGuardBandRight = desc->dvGuardBandBottom = 8192;
    desc->dwFVFCaps = 2;
    desc->dwTextureOpCaps = 0x00FFFFFF;
    desc->wMaxTextureBlendStages = 2;
    desc->wMaxSimultaneousTextures = 2;
    desc->dwMaxActiveLights = 8;
    desc->dvMaxVertexW = 1e10f;
    desc->deviceGUID = IID_IDirect3DHALDevice;
}

static HRESULT WINAPI v_GetCaps(IDirect3DDevice7* self, LPD3DDEVICEDESC7 desc)
{
    (void) self;
    fill_device_desc(desc);
    return D3D_OK;
}

static HRESULT WINAPI v_EnumTextureFormats(IDirect3DDevice7* self, LPD3DENUMPIXELFORMATSCALLBACK callback, LPVOID context)
{
    DDPIXELFORMAT pf;
    int f;

    (void) self;
    for (f = 0; f < PF_FORMATS; ++f)
    {
        describe_format(f, &pf);
        if (callback(&pf, context) == D3DENUMRET_CANCEL)
            break;
    }
    return D3D_OK;
}

static HRESULT WINAPI v_BeginScene(IDirect3DDevice7* self)
{
    target_bind(DEV->target);
    return D3D_OK;
}

static HRESULT WINAPI v_EndScene(IDirect3DDevice7* self)
{
    (void) self;
    return D3D_OK;
}

static HRESULT WINAPI v_GetDirect3D(IDirect3DDevice7* self, LPDIRECT3D7* out)
{
    *out = DEV->d3d;
    DEV->d3d->lpVtbl->AddRef(DEV->d3d);
    return D3D_OK;
}

static HRESULT WINAPI v_SetRenderTarget(IDirect3DDevice7* self, LPDIRECTDRAWSURFACE7 target, DWORD flags)
{
    surface* s = surface_of(target);

    (void) flags;
    if (!s)
        return DDERR_INVALIDPARAMS;
    s->iface.lpVtbl->AddRef(&s->iface);
    if (DEV->target)
        DEV->target->iface.lpVtbl->Release(&DEV->target->iface);
    DEV->target = s;
    return D3D_OK;
}

static HRESULT WINAPI v_GetRenderTarget(IDirect3DDevice7* self, LPDIRECTDRAWSURFACE7* out)
{
    *out = &DEV->target->iface;
    DEV->target->iface.lpVtbl->AddRef(&DEV->target->iface);
    return D3D_OK;
}

static HRESULT WINAPI v_Clear(IDirect3DDevice7* self, DWORD count, LPD3DRECT rects, DWORD flags, D3DCOLOR colour,
                              D3DVALUE z, DWORD stencil)
{
    device* d = DEV;
    GLbitfield mask = 0;
    float rgba[4];
    DWORD i;

    if (!d->target)
        return D3D_OK;

    target_bind(d->target);
    colour_of(colour, rgba);
    gl.ClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
    gl.ClearDepth(z);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.DepthMask(GL_TRUE);
    if (flags & D3DCLEAR_TARGET) mask |= GL_COLOR_BUFFER_BIT;
    if (flags & D3DCLEAR_ZBUFFER) mask |= GL_DEPTH_BUFFER_BIT;
    if (flags & D3DCLEAR_STENCIL) mask |= GL_STENCIL_BUFFER_BIT;
    (void) stencil;

    gl.Enable(GL_SCISSOR_TEST);
    if (!count)
    {
        const D3DVIEWPORT7* v = &d->viewport;
        gl.Scissor((GLint) v->dwX, (GLint) d->target->desc.dwHeight - (GLint) (v->dwY + v->dwHeight),
                   (GLsizei) v->dwWidth, (GLsizei) v->dwHeight);
        gl.Clear(mask);
    }
    for (i = 0; i < count; ++i)
    {
        gl.Scissor(rects[i].x1, (GLint) d->target->desc.dwHeight - rects[i].y2, rects[i].x2 - rects[i].x1,
                   rects[i].y2 - rects[i].y1);
        gl.Clear(mask);
    }
    gl.Disable(GL_SCISSOR_TEST);
    g_state_lost = TRUE;
    return D3D_OK;
}

static D3DMATRIX* transform(device* d, D3DTRANSFORMSTATETYPE type)
{
    switch (type)
    {
    case D3DTRANSFORMSTATE_WORLD:      return &d->world;
    case D3DTRANSFORMSTATE_VIEW:       return &d->view;
    case D3DTRANSFORMSTATE_PROJECTION: return &d->projection;
    default:                           return NULL;
    }
}

static HRESULT WINAPI v_SetTransform(IDirect3DDevice7* self, D3DTRANSFORMSTATETYPE type, LPD3DMATRIX m)
{
    D3DMATRIX* t = transform(DEV, type);

    if (!t)
        return unsupported("Device7::SetTransform of a texture or blend matrix");
    *t = *m;
    return D3D_OK;
}

static HRESULT WINAPI v_GetTransform(IDirect3DDevice7* self, D3DTRANSFORMSTATETYPE type, LPD3DMATRIX m)
{
    D3DMATRIX* t = transform(DEV, type);

    if (!t)
        return DDERR_INVALIDPARAMS;
    *m = *t;
    return D3D_OK;
}

static HRESULT WINAPI v_SetViewport(IDirect3DDevice7* self, LPD3DVIEWPORT7 v)
{
    DEV->viewport = *v;
    return D3D_OK;
}

static HRESULT WINAPI v_MultiplyTransform(IDirect3DDevice7* self, D3DTRANSFORMSTATETYPE type, LPD3DMATRIX m)
{
    D3DMATRIX* t = transform(DEV, type);

    if (!t)
        return DDERR_INVALIDPARAMS;
    multiply(t, m, t);
    return D3D_OK;
}

static HRESULT WINAPI v_GetViewport(IDirect3DDevice7* self, LPD3DVIEWPORT7 v)
{
    *v = DEV->viewport;
    return D3D_OK;
}

static HRESULT WINAPI v_SetMaterial(IDirect3DDevice7* self, LPD3DMATERIAL7 m)
{
    DEV->material = *m;
    return D3D_OK;
}

static HRESULT WINAPI v_GetMaterial(IDirect3DDevice7* self, LPD3DMATERIAL7 m)
{
    *m = DEV->material;
    return D3D_OK;
}

static HRESULT WINAPI v_SetLight(IDirect3DDevice7* self, DWORD index, LPD3DLIGHT7 light)
{
    (void) self; (void) index; (void) light;
    return D3D_OK;      /* the game never turns lighting on */
}

static HRESULT WINAPI v_GetLight(IDirect3DDevice7* self, DWORD index, LPD3DLIGHT7 light)
{
    (void) self; (void) index; (void) light;
    return DDERR_INVALIDPARAMS;
}

static HRESULT WINAPI v_SetRenderState(IDirect3DDevice7* self, D3DRENDERSTATETYPE state, DWORD value)
{
    if ((DWORD) state < 256)
        DEV->rs[state] = value;
    return D3D_OK;
}

static HRESULT WINAPI v_GetRenderState(IDirect3DDevice7* self, D3DRENDERSTATETYPE state, LPDWORD value)
{
    *value = (DWORD) state < 256 ? DEV->rs[state] : 0;
    return D3D_OK;
}

static HRESULT WINAPI v_BeginStateBlock(IDirect3DDevice7* self)
{
    (void) self;
    return unsupported("Device7::BeginStateBlock");
}

static HRESULT WINAPI v_EndStateBlock(IDirect3DDevice7* self, LPDWORD block)
{
    (void) self; (void) block;
    return unsupported("Device7::EndStateBlock");
}

static HRESULT WINAPI v_PreLoad(IDirect3DDevice7* self, LPDIRECTDRAWSURFACE7 texture)
{
    (void) self; (void) texture;
    return D3D_OK;
}

static HRESULT WINAPI v_DrawPrimitive(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, DWORD fvf, LPVOID vertices,
                                      DWORD count, DWORD flags)
{
    (void) flags;
    return draw(DEV, type, fvf, (const BYTE*) vertices, count, NULL, 0);
}

static HRESULT WINAPI v_DrawIndexedPrimitive(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, DWORD fvf, LPVOID vertices,
                                             DWORD count, LPWORD indices, DWORD index_count, DWORD flags)
{
    (void) flags;
    return draw(DEV, type, fvf, (const BYTE*) vertices, count, indices, index_count);
}

static HRESULT WINAPI v_SetClipStatus(IDirect3DDevice7* self, LPD3DCLIPSTATUS status)
{
    (void) self; (void) status;
    return D3D_OK;
}

static HRESULT WINAPI v_GetClipStatus(IDirect3DDevice7* self, LPD3DCLIPSTATUS status)
{
    (void) self;
    __stosb((unsigned char*) status, 0, sizeof *status);
    return D3D_OK;
}

static HRESULT WINAPI v_DrawPrimitiveStrided(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, DWORD fvf,
                                             LPD3DDRAWPRIMITIVESTRIDEDDATA data, DWORD count, DWORD flags)
{
    (void) self; (void) type; (void) fvf; (void) data; (void) count; (void) flags;
    return unsupported("Device7::DrawPrimitiveStrided");
}

static HRESULT WINAPI v_DrawIndexedPrimitiveStrided(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, DWORD fvf,
                                                    LPD3DDRAWPRIMITIVESTRIDEDDATA data, DWORD count, LPWORD indices,
                                                    DWORD index_count, DWORD flags)
{
    (void) self; (void) type; (void) fvf; (void) data; (void) count; (void) indices; (void) index_count; (void) flags;
    return unsupported("Device7::DrawIndexedPrimitiveStrided");
}

static HRESULT WINAPI v_DrawPrimitiveVB(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, LPDIRECT3DVERTEXBUFFER7 vb,
                                        DWORD start, DWORD count, DWORD flags)
{
    (void) flags;
    return draw_buffer(DEV, type, (vertex_buffer*) vb, start, count, NULL, 0);
}

static HRESULT WINAPI v_DrawIndexedPrimitiveVB(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, LPDIRECT3DVERTEXBUFFER7 vb,
                                               DWORD start, DWORD count, LPWORD indices, DWORD index_count, DWORD flags)
{
    (void) flags;
    return draw_buffer(DEV, type, (vertex_buffer*) vb, start, count, indices, index_count);
}

static HRESULT WINAPI v_ComputeSphereVisibility(IDirect3DDevice7* self, LPD3DVECTOR centres, LPD3DVALUE radii,
                                                DWORD count, DWORD flags, LPDWORD results)
{
    (void) self; (void) centres; (void) radii; (void) count; (void) flags; (void) results;
    return unsupported("Device7::ComputeSphereVisibility");
}

static HRESULT WINAPI v_GetTexture(IDirect3DDevice7* self, DWORD stage, LPDIRECTDRAWSURFACE7* out)
{
    surface* t = stage < 8 ? DEV->texture[stage] : NULL;

    *out = t ? &t->iface : NULL;
    if (t)
        t->iface.lpVtbl->AddRef(&t->iface);
    return D3D_OK;
}

static HRESULT WINAPI v_SetTexture(IDirect3DDevice7* self, DWORD stage, LPDIRECTDRAWSURFACE7 texture)
{
    surface* t = surface_of(texture);

    if (stage >= 8)
        return DDERR_INVALIDPARAMS;
    if (t)
        t->iface.lpVtbl->AddRef(&t->iface);
    if (DEV->texture[stage])
        DEV->texture[stage]->iface.lpVtbl->Release(&DEV->texture[stage]->iface);
    DEV->texture[stage] = t;
    return D3D_OK;
}

static HRESULT WINAPI v_GetTextureStageState(IDirect3DDevice7* self, DWORD stage, D3DTEXTURESTAGESTATETYPE type, LPDWORD value)
{
    *value = (stage < 8 && (DWORD) type < 32) ? DEV->tss[stage][type] : 0;
    return D3D_OK;
}

static HRESULT WINAPI v_SetTextureStageState(IDirect3DDevice7* self, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
{
    if (stage >= 8 || (DWORD) type >= 32)
        return DDERR_INVALIDPARAMS;
    DEV->tss[stage][type] = value;
    if (type == D3DTSS_ADDRESS)
        DEV->tss[stage][D3DTSS_ADDRESSU] = DEV->tss[stage][D3DTSS_ADDRESSV] = value;
    return D3D_OK;
}

static HRESULT WINAPI v_ValidateDevice(IDirect3DDevice7* self, LPDWORD passes)
{
    (void) self;
    *passes = 1;
    return D3D_OK;
}

static HRESULT WINAPI v_ApplyStateBlock(IDirect3DDevice7* self, DWORD block)
{
    (void) self; (void) block;
    return unsupported("Device7::ApplyStateBlock");
}

static HRESULT WINAPI v_CaptureStateBlock(IDirect3DDevice7* self, DWORD block)
{
    (void) self; (void) block;
    return unsupported("Device7::CaptureStateBlock");
}

static HRESULT WINAPI v_DeleteStateBlock(IDirect3DDevice7* self, DWORD block)
{
    (void) self; (void) block;
    return unsupported("Device7::DeleteStateBlock");
}

static HRESULT WINAPI v_CreateStateBlock(IDirect3DDevice7* self, D3DSTATEBLOCKTYPE type, LPDWORD block)
{
    (void) self; (void) type; (void) block;
    return unsupported("Device7::CreateStateBlock");
}

static HRESULT WINAPI v_Load(IDirect3DDevice7* self, LPDIRECTDRAWSURFACE7 to, LPPOINT at, LPDIRECTDRAWSURFACE7 from,
                             LPRECT rect, DWORD flags)
{
    (void) self; (void) to; (void) at; (void) from; (void) rect; (void) flags;
    return unsupported("Device7::Load");
}

static HRESULT WINAPI v_LightEnable(IDirect3DDevice7* self, DWORD index, BOOL enable)
{
    (void) self; (void) index; (void) enable;
    return D3D_OK;
}

static HRESULT WINAPI v_GetLightEnable(IDirect3DDevice7* self, DWORD index, BOOL* enable)
{
    (void) self; (void) index;
    *enable = FALSE;
    return D3D_OK;
}

static HRESULT WINAPI v_SetClipPlane(IDirect3DDevice7* self, DWORD index, D3DVALUE* plane)
{
    (void) self; (void) index; (void) plane;
    return D3D_OK;
}

static HRESULT WINAPI v_GetClipPlane(IDirect3DDevice7* self, DWORD index, D3DVALUE* plane)
{
    (void) self; (void) index; (void) plane;
    return DDERR_INVALIDPARAMS;
}

static HRESULT WINAPI v_GetInfo(IDirect3DDevice7* self, DWORD id, LPVOID info, DWORD size)
{
    (void) self; (void) id; (void) info; (void) size;
    return S_FALSE;
}

#undef DEV

static IDirect3DDevice7Vtbl k_device_vtbl = {
    .QueryInterface = v_QueryInterface,
    .AddRef = v_AddRef,
    .Release = v_Release,
    .GetCaps = v_GetCaps,
    .EnumTextureFormats = v_EnumTextureFormats,
    .BeginScene = v_BeginScene,
    .EndScene = v_EndScene,
    .GetDirect3D = v_GetDirect3D,
    .SetRenderTarget = v_SetRenderTarget,
    .GetRenderTarget = v_GetRenderTarget,
    .Clear = v_Clear,
    .SetTransform = v_SetTransform,
    .GetTransform = v_GetTransform,
    .SetViewport = v_SetViewport,
    .MultiplyTransform = v_MultiplyTransform,
    .GetViewport = v_GetViewport,
    .SetMaterial = v_SetMaterial,
    .GetMaterial = v_GetMaterial,
    .SetLight = v_SetLight,
    .GetLight = v_GetLight,
    .SetRenderState = v_SetRenderState,
    .GetRenderState = v_GetRenderState,
    .BeginStateBlock = v_BeginStateBlock,
    .EndStateBlock = v_EndStateBlock,
    .PreLoad = v_PreLoad,
    .DrawPrimitive = v_DrawPrimitive,
    .DrawIndexedPrimitive = v_DrawIndexedPrimitive,
    .SetClipStatus = v_SetClipStatus,
    .GetClipStatus = v_GetClipStatus,
    .DrawPrimitiveStrided = v_DrawPrimitiveStrided,
    .DrawIndexedPrimitiveStrided = v_DrawIndexedPrimitiveStrided,
    .DrawPrimitiveVB = v_DrawPrimitiveVB,
    .DrawIndexedPrimitiveVB = v_DrawIndexedPrimitiveVB,
    .ComputeSphereVisibility = v_ComputeSphereVisibility,
    .GetTexture = v_GetTexture,
    .SetTexture = v_SetTexture,
    .GetTextureStageState = v_GetTextureStageState,
    .SetTextureStageState = v_SetTextureStageState,
    .ValidateDevice = v_ValidateDevice,
    .ApplyStateBlock = v_ApplyStateBlock,
    .CaptureStateBlock = v_CaptureStateBlock,
    .DeleteStateBlock = v_DeleteStateBlock,
    .CreateStateBlock = v_CreateStateBlock,
    .Load = v_Load,
    .LightEnable = v_LightEnable,
    .GetLightEnable = v_GetLightEnable,
    .SetClipPlane = v_SetClipPlane,
    .GetClipPlane = v_GetClipPlane,
    .GetInfo = v_GetInfo,
};

/* ------------------------------------------------------------------ IDirect3DVertexBuffer7 */

#define VB ((vertex_buffer*) self)

static HRESULT WINAPI b_QueryInterface(IDirect3DVertexBuffer7* self, REFIID iid, void** out)
{
    if (IsEqualGUID(iid, &IID_IDirect3DVertexBuffer7) || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++VB->refs;
        return S_OK;
    }
    return no_interface("a vertex buffer", iid, out);
}

static ULONG WINAPI b_AddRef(IDirect3DVertexBuffer7* self)
{
    return (ULONG) ++VB->refs;
}

static ULONG WINAPI b_Release(IDirect3DVertexBuffer7* self)
{
    LONG refs = --VB->refs;

    if (refs == 0)
    {
        mem_free(VB->data);
        mem_free(VB->clip);
        mem_free(self);
    }
    return (ULONG) refs;
}

static HRESULT WINAPI b_Lock(IDirect3DVertexBuffer7* self, DWORD flags, LPVOID* data, LPDWORD size)
{
    (void) flags;
    *data = VB->data;
    if (size)
        *size = VB->desc.dwNumVertices * fvf_size(VB->desc.dwFVF);
    VB->processed = FALSE;      /* the game is writing vertices of its own */
    return D3D_OK;
}

static HRESULT WINAPI b_Unlock(IDirect3DVertexBuffer7* self)
{
    (void) self;
    return D3D_OK;
}

/* Transform, as Direct3D's ProcessVertices does, into pre-transformed vertices - keeping each one's
   clip-space position as well, for drawing (see draw_buffer). */
static HRESULT WINAPI b_ProcessVertices(IDirect3DVertexBuffer7* self, DWORD op, DWORD to_index, DWORD count,
                                        LPDIRECT3DVERTEXBUFFER7 source, DWORD from_index, LPDIRECT3DDEVICE7 dev, DWORD flags)
{
    vertex_buffer* to = VB;
    vertex_buffer* from = (vertex_buffer*) source;
    device* d = (device*) dev;
    layout lt = layout_of(to->desc.dwFVF), lf = layout_of(from->desc.dwFVF);
    D3DMATRIX all;
    const D3DVIEWPORT7* v = &d->viewport;
    DWORD i;

    (void) op; (void) flags;
    if (to_index + count > to->desc.dwNumVertices || from_index + count > from->desc.dwNumVertices || lf.pretransformed)
        return DDERR_INVALIDPARAMS;

    if (!to->clip)
        to->clip = (float*) mem_alloc((SIZE_T) to->desc.dwNumVertices * 4 * sizeof(float));
    if (!to->clip)
        return DDERR_OUTOFMEMORY;

    multiply(&all, &d->world, &d->view);
    multiply(&all, &all, &d->projection);

    for (i = 0; i < count; ++i)
    {
        const BYTE* in = from->data + (from_index + i) * lf.stride;
        BYTE* out = to->data + (to_index + i) * lt.stride;
        const float* p = (const float*) in;
        float* c = to->clip + (to_index + i) * 4;
        float* t = (float*) out;
        float rhw;
        int k;

        c[0] = p[0] * all._11 + p[1] * all._21 + p[2] * all._31 + all._41;
        c[1] = p[0] * all._12 + p[1] * all._22 + p[2] * all._32 + all._42;
        c[2] = p[0] * all._13 + p[1] * all._23 + p[2] * all._33 + all._43;
        c[3] = p[0] * all._14 + p[1] * all._24 + p[2] * all._34 + all._44;

        /* Direct3D's 0..1 depth, for the copy the game can read; OpenGL draws from `c`. */
        rhw = c[3] != 0 ? 1.0f / c[3] : 0;
        t[0] = v->dwX + (1 + c[0] * rhw) * v->dwWidth * 0.5f;
        t[1] = v->dwY + (1 - c[1] * rhw) * v->dwHeight * 0.5f;
        t[2] = v->dvMinZ + c[2] * rhw * (v->dvMaxZ - v->dvMinZ);
        t[3] = rhw;
        c[2] = 2 * c[2] - c[3];

        if (lt.diffuse >= 0)
            *(DWORD*) (out + lt.diffuse) = lf.diffuse >= 0 ? *(const DWORD*) (in + lf.diffuse) : 0xFFFFFFFF;
        if (lt.specular >= 0)
            *(DWORD*) (out + lt.specular) = lf.specular >= 0 ? *(const DWORD*) (in + lf.specular) : 0;
        for (k = 0; k < 2; ++k)
        {
            if (lt.tex[k] >= 0)
            {
                const float* uv = lf.tex[k] >= 0 ? (const float*) (in + lf.tex[k]) : NULL;
                ((float*) (out + lt.tex[k]))[0] = uv ? uv[0] : 0;
                ((float*) (out + lt.tex[k]))[1] = uv ? uv[1] : 0;
            }
        }
    }

    to->processed = TRUE;
    return D3D_OK;
}

static HRESULT WINAPI b_GetVertexBufferDesc(IDirect3DVertexBuffer7* self, LPD3DVERTEXBUFFERDESC desc)
{
    *desc = VB->desc;
    return D3D_OK;
}

static HRESULT WINAPI b_Optimize(IDirect3DVertexBuffer7* self, LPDIRECT3DDEVICE7 dev, DWORD flags)
{
    (void) self; (void) dev; (void) flags;
    return D3D_OK;
}

static HRESULT WINAPI b_ProcessVerticesStrided(IDirect3DVertexBuffer7* self, DWORD op, DWORD to_index, DWORD count,
                                               LPD3DDRAWPRIMITIVESTRIDEDDATA data, DWORD fvf, LPDIRECT3DDEVICE7 dev, DWORD flags)
{
    (void) self; (void) op; (void) to_index; (void) count; (void) data; (void) fvf; (void) dev; (void) flags;
    return unsupported("VertexBuffer7::ProcessVerticesStrided");
}

#undef VB

static IDirect3DVertexBuffer7Vtbl k_buffer_vtbl = {
    .QueryInterface = b_QueryInterface,
    .AddRef = b_AddRef,
    .Release = b_Release,
    .Lock = b_Lock,
    .Unlock = b_Unlock,
    .ProcessVertices = b_ProcessVertices,
    .GetVertexBufferDesc = b_GetVertexBufferDesc,
    .Optimize = b_Optimize,
    .ProcessVerticesStrided = b_ProcessVerticesStrided,
};

/* ------------------------------------------------------------------ IDirect3D7 */

typedef struct direct3d
{
    IDirect3D7    iface;
    LONG          refs;
    IDirectDraw7* owner;
} direct3d;

#define D3 ((direct3d*) self)

static HRESULT WINAPI t_QueryInterface(IDirect3D7* self, REFIID iid, void** out)
{
    if (IsEqualGUID(iid, &IID_IDirect3D7) || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++D3->refs;
        return S_OK;
    }
    return D3->owner->lpVtbl->QueryInterface(D3->owner, iid, out);
}

static ULONG WINAPI t_AddRef(IDirect3D7* self)
{
    return (ULONG) ++D3->refs;
}

static ULONG WINAPI t_Release(IDirect3D7* self)
{
    LONG refs = --D3->refs;

    if (refs == 0)
        mem_free(self);
    return (ULONG) refs;
}

static HRESULT WINAPI t_EnumDevices(IDirect3D7* self, LPD3DENUMDEVICESCALLBACK7 callback, LPVOID context)
{
    D3DDEVICEDESC7 desc;

    (void) self;
    fill_device_desc(&desc);
    callback("OpenMCM2 OpenGL renderer", "HAL", &desc, context);
    return D3D_OK;
}

static HRESULT WINAPI t_CreateDevice(IDirect3D7* self, REFCLSID type, LPDIRECTDRAWSURFACE7 target, LPDIRECT3DDEVICE7* out)
{
    device* d;
    surface* s = surface_of(target);

    (void) type;
    if (!s)
        return DDERR_INVALIDPARAMS;

    d = (device*) mem_alloc(sizeof *d);
    if (!d)
        return DDERR_OUTOFMEMORY;

    d->iface.lpVtbl = &k_device_vtbl;
    d->refs = 1;
    d->d3d = self;
    d->target = s;
    s->iface.lpVtbl->AddRef(&s->iface);
    set_defaults(d);

    target_bind(s);
    if (!g_white)
    {
        static const DWORD white = 0xFFFFFFFF;
        gl.GenTextures(1, &g_white);
        gl.BindTexture(GL_TEXTURE_2D, g_white);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    }

    *out = &d->iface;
    return D3D_OK;
}

static HRESULT WINAPI t_CreateVertexBuffer(IDirect3D7* self, LPD3DVERTEXBUFFERDESC desc, LPDIRECT3DVERTEXBUFFER7* out, DWORD flags)
{
    vertex_buffer* vb = (vertex_buffer*) mem_alloc(sizeof *vb);

    (void) self; (void) flags;
    if (!vb)
        return DDERR_OUTOFMEMORY;
    vb->iface.lpVtbl = &k_buffer_vtbl;
    vb->refs = 1;
    vb->desc = *desc;
    vb->data = (BYTE*) mem_alloc((SIZE_T) desc->dwNumVertices * fvf_size(desc->dwFVF));
    if (!vb->data)
    {
        mem_free(vb);
        return DDERR_OUTOFMEMORY;
    }
    *out = &vb->iface;
    return D3D_OK;
}

static HRESULT WINAPI t_EnumZBufferFormats(IDirect3D7* self, REFCLSID type, LPD3DENUMPIXELFORMATSCALLBACK callback, LPVOID context)
{
    static const DWORD depths[] = { 16, 24 };
    DDPIXELFORMAT pf;
    int i;

    (void) self; (void) type;
    for (i = 0; i < 2; ++i)
    {
        __stosb((unsigned char*) &pf, 0, sizeof pf);
        pf.dwSize = sizeof pf;
        pf.dwFlags = DDPF_ZBUFFER;
        pf.dwZBufferBitDepth = depths[i];
        pf.dwZBitMask = depths[i] == 16 ? 0xFFFF : 0xFFFFFF;
        if (callback(&pf, context) == D3DENUMRET_CANCEL)
            break;
    }
    return D3D_OK;
}

static HRESULT WINAPI t_EvictManagedTextures(IDirect3D7* self)
{
    (void) self;
    return D3D_OK;
}

#undef D3

static IDirect3D7Vtbl k_direct3d_vtbl = {
    .QueryInterface = t_QueryInterface,
    .AddRef = t_AddRef,
    .Release = t_Release,
    .EnumDevices = t_EnumDevices,
    .CreateDevice = t_CreateDevice,
    .CreateVertexBuffer = t_CreateVertexBuffer,
    .EnumZBufferFormats = t_EnumZBufferFormats,
    .EvictManagedTextures = t_EvictManagedTextures,
};

IDirect3D7* direct3d_create(IDirectDraw7* owner)
{
    direct3d* d = (direct3d*) mem_alloc(sizeof *d);

    if (!d)
        return NULL;
    d->iface.lpVtbl = &k_direct3d_vtbl;
    d->refs = 1;
    d->owner = owner;
    return &d->iface;
}
