#include "GsKitGfx.hpp"
#include "GameWindow.hpp"
#include "utils.hpp"

#include <cstring>
#include <dmaKit.h>
#include <gsInline.h>
#include <gsToolkit.h>
#include <malloc.h>

// GS modulate treats 0x80 as 1.0, so vertex colors are halved.
static inline u64 PackColor(ZunColor c, bool halveRgb)
{
    u8 r = (c >> 16) & 0xFF;
    u8 g = (c >> 8) & 0xFF;
    u8 b = c & 0xFF;
    u8 a = (c >> 24) >> 1;
    if (halveRgb)
    {
        r >>= 1;
        g >>= 1;
        b >>= 1;
    }
    return GS_SETREG_RGBAQ(r, g, b, a, 0);
}

GfxInterface *GsKitGfx::Init()
{
    GsKitGfx *self = new GsKitGfx;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    GSGLOBAL *gs = gsKit_init_global();
    if (gs == NULL)
    {
        delete self;
        return NULL;
    }
    self->gs = gs;

    // 16-bit framebuffer: the PS2 only has 4 MiB of VRAM, and double-buffered
    // 32-bit framebuffers plus a Z buffer leave almost no room for textures.
    // 16-bit halves the framebuffer footprint (EoSD has a 16-bit mode anyway).
    gs->PSM = GS_PSM_CT16;
    gs->PSMZ = GS_PSMZ_16;
    gs->ZBuffering = GS_SETTING_ON;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_ON;

    gsKit_init_screen(gs);
    gsKit_TexManager_init(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);

    // Alpha test mirrors the alphaThreshold the other backends used
    gs->Test->ATE = 1;
    gs->Test->ATST = 5; // GEQUAL
    gs->Test->AREF = 2; // threshold 4, in GS half range
    gs->Test->AFAIL = 0;
    gs->Test->ZTE = 1;
    gs->Test->ZTST = 1; // ALWAYS until depth test is enabled
    gsKit_set_test(gs, 0);

    gsKit_set_clamp(gs, GS_CMODE_REPEAT);
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

    // Game space is 640x480, the GS framebuffer may be 448 (NTSC) or 512 (PAL)
    self->scaleX = (f32)gs->Width / (f32)GAME_WINDOW_WIDTH;
    self->scaleY = (f32)gs->Height / (f32)GAME_WINDOW_HEIGHT;

    self->model.Identity();
    self->view.Identity();
    self->projection.Identity();
    self->textureMatrix.Identity();
    self->viewport[2] = GAME_WINDOW_WIDTH;
    self->viewport[3] = GAME_WINDOW_HEIGHT;

    // Reserve handle 0 so handle==0 keeps meaning "no texture"
    self->textures.push_back(nullptr);

    utils::DebugPrint2("gsKit init: %dx%d", gs->Width, gs->Height);
    return self;
}

GsKitGfx::~GsKitGfx()
{
    for (GSTEXTURE *tex : textures)
    {
        if (tex != nullptr)
        {
            FreeTextureData(tex);
            delete tex;
        }
    }
    if (gs != nullptr)
    {
        gsKit_deinit_global(gs);
    }
}

void GsKitGfx::FreeTextureData(GSTEXTURE *tex)
{
    gsKit_TexManager_free(gs, tex);
    if (tex->Mem != nullptr)
    {
        free(tex->Mem);
        tex->Mem = nullptr;
    }
}

void GsKitGfx::SetFogRange(f32 nearPlane, f32 farPlane)
{
    // GS fog is not wired up yet; stage backgrounds render unfogged
    fogNear = nearPlane;
    fogFar = farPlane;
}

void GsKitGfx::SetFogColor(ZunColor color)
{
    fogColor = color;
}

void GsKitGfx::ToggleVertexAttribute(u8 attr, bool enable)
{
    if (attr & VERTEX_ATTR_TEX_COORD)
    {
        useTexCoord = enable;
    }
    if (attr & VERTEX_ATTR_DIFFUSE)
    {
        useDiffuse = enable;
    }
}

void GsKitGfx::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr)
{
    switch (attr)
    {
    case VERTEX_ARRAY_POSITION:
        vertexData = ptr;
        vertexStride = stride;
        break;
    case VERTEX_ARRAY_TEX_COORD:
        texCoordData = ptr;
        texCoordStride = stride;
        break;
    case VERTEX_ARRAY_DIFFUSE:
        diffuseData = ptr;
        diffuseStride = stride;
        break;
    }
}

void GsKitGfx::SetColorOp(TextureOpComponent component, ColorOp op)
{
    // The GS texture function is fixed to modulate here, which covers the
    // game's main path (REPLACE degenerates to modulate by a white color)
    (void)component;
    (void)op;
}

void GsKitGfx::SetTextureFactor(ZunColor factor)
{
    textureFactor = factor;
}

void GsKitGfx::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix)
{
    switch (type)
    {
    case MATRIX_MODEL:
        model = matrix;
        break;
    case MATRIX_VIEW:
        view = matrix;
        break;
    case MATRIX_PROJECTION:
        projection = matrix;
        break;
    case MATRIX_TEXTURE:
        textureMatrix = matrix;
        break;
    }
}

void GsKitGfx::SetTextureFilter()
{
    // Filtering is set per texture at creation time (always LINEAR)
}

void GsKitGfx::GetViewport(u32 *vp)
{
    for (int i = 0; i < 4; i++)
    {
        vp[i] = viewport[i];
    }
}

void GsKitGfx::GetDepthRange(f32 *depthRange)
{
    depthRange[0] = depthNear;
    depthRange[1] = depthFar;
}

void GsKitGfx::SetViewport(i32 x, i32 y, i32 width, i32 height)
{
    viewport[0] = x;
    viewport[1] = y;
    viewport[2] = width;
    viewport[3] = height;
    ApplyScissor();
}

void GsKitGfx::ApplyScissor()
{
    // GL clips to the viewport via NDC; the GS needs an explicit scissor box
    i32 x0 = (i32)(viewport[0] * scaleX);
    i32 y0 = (i32)(viewport[1] * scaleY);
    i32 x1 = (i32)((viewport[0] + viewport[2]) * scaleX) - 1;
    i32 y1 = (i32)((viewport[1] + viewport[3]) * scaleY) - 1;
    gsKit_set_scissor(gs, GS_SETREG_SCISSOR(x0, x1, y0, y1));
}

void GsKitGfx::SetDepthRange(f32 nearPlane, f32 farPlane)
{
    depthNear = nearPlane;
    depthFar = farPlane;
}

void GsKitGfx::Enable(Capabilities cap)
{
    if (cap == CAPS_DEPTH_TEST)
    {
        useDepthTest = true;
        ApplyTest();
    }
    // CAPS_BLEND is always on (PrimAlphaEnable)
}

void GsKitGfx::ApplyTest()
{
    // GL z grows away from camera, GS z grows toward it, so LEQUAL maps to GEQUAL
    if (!useDepthTest || depthFunc == DEPTH_FUNC_ALWAYS)
    {
        gs->Test->ZTST = 1; // ALWAYS
    }
    else
    {
        gs->Test->ZTST = 2; // GEQUAL
    }
    gsKit_set_test(gs, 0);
}

void GsKitGfx::ApplyDepthMask()
{
    if (gs->ZBuffering != GS_SETTING_ON)
    {
        return;
    }

    u64 *pData = (u64 *)gsKit_heap_alloc(gs, 1, 16, GIF_AD);

    *pData++ = GIF_TAG_AD(1);
    *pData++ = GIF_AD;
    *pData++ = GS_SETREG_ZBUF(gs->ZBuffer / 8192, gs->PSMZ, depthMask ? 0 : 1);
    *pData++ = GS_ZBUF_1 + gs->PrimContext;
}

void GsKitGfx::ApplyFrameMask(u32 mask)
{
    u64 *pData = (u64 *)gsKit_heap_alloc(gs, 1, 16, GIF_AD);

    *pData++ = GIF_TAG_AD(1);
    *pData++ = GIF_AD;
    *pData++ = GS_SETREG_FRAME(gs->ScreenBuffer[gs->ActiveBuffer & 1] / 8192, gs->Width / 64, gs->PSM, mask);
    *pData++ = GS_FRAME_1 + gs->PrimContext;
}

void GsKitGfx::SetBlendMode(BlendMode mode)
{
    blendMode = mode;
    if (mode == BLEND_ONE)
    {
        // Cs*As + Cd
        gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 2, 0, 1, 0), 0);
    }
    else
    {
        // Cs*As + Cd*(1-As)
        gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    }
}

void GsKitGfx::SetDepthMask(bool enable)
{
    depthMask = enable;
    ApplyDepthMask();
}

void GsKitGfx::SetDepthFunc(DepthFunc func)
{
    depthFunc = func;
    ApplyTest();
}

void GsKitGfx::SetClearDepth(f32 depth)
{
    (void)depth; // gsKit_clear always writes far plane (z=0 on GS)
}

void GsKitGfx::SetClearColor(f32 r, f32 g, f32 b, f32 a)
{
    clearColor = ((u32)(a * 255) << 24) | ((u32)(r * 255) << 16) | ((u32)(g * 255) << 8) | (u32)(b * 255);
}

void GsKitGfx::Clear(u32 clearBits)
{
    if ((clearBits & (CLEAR_COLOR_BUFFER | CLEAR_DEPTH_BUFFER)) == 0)
    {
        return;
    }

    // gsKit_clear draws a fullscreen primitive. Use GS write masks so a
    // depth-only clear does not paint the gameplay rectangle into the frame.
    bool colorWrite = (clearBits & CLEAR_COLOR_BUFFER) != 0;
    bool depthWrite = (clearBits & CLEAR_DEPTH_BUFFER) != 0;
    bool savedDepthMask = depthMask;
    if (depthMask != depthWrite)
    {
        depthMask = depthWrite;
        ApplyDepthMask();
    }
    if (!colorWrite)
    {
        ApplyFrameMask(0xFFFFFFFF);
    }

    u8 r = (clearColor >> 16) & 0xFF;
    u8 g = (clearColor >> 8) & 0xFF;
    u8 b = clearColor & 0xFF;
    gsKit_clear(gs, GS_SETREG_RGBAQ(r, g, b, 0x80, 0));

    if (!colorWrite)
    {
        ApplyFrameMask(0);
    }
    if (depthMask != savedDepthMask)
    {
        depthMask = savedDepthMask;
        ApplyDepthMask();
    }
}

GfxTextureHandle GsKitGfx::CreateTexture()
{
    GSTEXTURE *tex = new GSTEXTURE;
    std::memset(tex, 0, sizeof(GSTEXTURE));
    tex->PSM = GS_PSM_CT32;
    tex->Filter = GS_FILTER_LINEAR;
    tex->Delayed = 1;

    u32 id;
    if (!freeTextures.empty())
    {
        id = freeTextures.back();
        freeTextures.pop_back();
        textures[id] = tex;
    }
    else
    {
        id = textures.size();
        textures.push_back(tex);
    }
    return {id};
}

void GsKitGfx::BindTexture(GfxTextureHandle handle)
{
    if (handle.id >= textures.size())
    {
        return;
    }
    boundTexture = textures[handle.id];
}

void GsKitGfx::DeleteTexture(GfxTextureHandle handle)
{
    if (handle.id == 0 || handle.id >= textures.size() || textures[handle.id] == nullptr)
    {
        return;
    }
    GSTEXTURE *tex = textures[handle.id];
    if (boundTexture == tex)
    {
        boundTexture = nullptr;
    }
    FreeTextureData(tex);
    delete tex;
    textures[handle.id] = nullptr;
    freeTextures.push_back(handle.id);
}

// Converts any incoming GL-style pixel format to CT32 with GS half-range alpha
static void ConvertPixels(u32 *dst, const void *src, u32 count, PixelFormat fmt, PixelDataType type)
{
    if (src == NULL)
    {
        // No data: white texture, used as the "no texture bound" dummy
        for (u32 i = 0; i < count; i++)
        {
            dst[i] = 0x80FFFFFF;
        }
        return;
    }

    const u8 *s8 = (const u8 *)src;
    const u16 *s16 = (const u16 *)src;
    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        if (fmt == PIXEL_RGB)
        {
            for (u32 i = 0; i < count; i++)
            {
                dst[i] = (u32)s8[i * 3] | ((u32)s8[i * 3 + 1] << 8) | ((u32)s8[i * 3 + 2] << 16) | 0x80000000;
            }
        }
        else
        {
            for (u32 i = 0; i < count; i++)
            {
                dst[i] = (u32)s8[i * 4] | ((u32)s8[i * 4 + 1] << 8) | ((u32)s8[i * 4 + 2] << 16) |
                         ((u32)(s8[i * 4 + 3] >> 1) << 24);
            }
        }
        break;
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = ((p >> 12) & 0xF) * 17;
            u32 g = ((p >> 8) & 0xF) * 17;
            u32 b = ((p >> 4) & 0xF) * 17;
            u32 a = ((p & 0xF) * 17) >> 1;
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
        break;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = ((p >> 11) & 0x1F) << 3;
            u32 g = ((p >> 6) & 0x1F) << 3;
            u32 b = ((p >> 1) & 0x1F) << 3;
            u32 a = (p & 1) ? 0x80 : 0;
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
        break;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = ((p >> 11) & 0x1F) << 3;
            u32 g = ((p >> 5) & 0x3F) << 2;
            u32 b = (p & 0x1F) << 3;
            dst[i] = r | (g << 8) | (b << 16) | 0x80000000;
        }
        break;
    }
}

static void ConvertPixels16(u16 *dst, const void *src, u32 count, PixelFormat fmt, PixelDataType type)
{
    if (src == NULL)
    {
        for (u32 i = 0; i < count; i++)
        {
            dst[i] = 0xFFFF;
        }
        return;
    }

    const u8 *s8 = (const u8 *)src;
    const u16 *s16 = (const u16 *)src;
    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        if (fmt == PIXEL_RGB)
        {
            for (u32 i = 0; i < count; i++)
            {
                dst[i] = (s8[i * 3] >> 3) | ((s8[i * 3 + 1] >> 3) << 5) | ((s8[i * 3 + 2] >> 3) << 10) | 0x8000;
            }
        }
        else
        {
            for (u32 i = 0; i < count; i++)
            {
                u16 a = s8[i * 4 + 3] >= 128 ? 0x8000 : 0;
                dst[i] = (s8[i * 4] >> 3) | ((s8[i * 4 + 1] >> 3) << 5) | ((s8[i * 4 + 2] >> 3) << 10) | a;
            }
        }
        break;
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = ((p >> 12) & 0xF) << 1;
            u32 g = ((p >> 8) & 0xF) << 1;
            u32 b = ((p >> 4) & 0xF) << 1;
            u16 a = (p & 0xF) >= 8 ? 0x8000 : 0;
            dst[i] = r | (g << 5) | (b << 10) | a;
        }
        break;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = (p >> 11) & 0x1F;
            u32 g = (p >> 6) & 0x1F;
            u32 b = (p >> 1) & 0x1F;
            u16 a = (p & 1) ? 0x8000 : 0;
            dst[i] = r | (g << 5) | (b << 10) | a;
        }
        break;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        for (u32 i = 0; i < count; i++)
        {
            u16 p = s16[i];
            u32 r = (p >> 11) & 0x1F;
            u32 g = ((p >> 5) & 0x3F) >> 1;
            u32 b = p & 0x1F;
            dst[i] = r | (g << 5) | (b << 10) | 0x8000;
        }
        break;
    }
}

void GsKitGfx::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data)
{
    if (boundTexture == nullptr)
    {
        return;
    }
    GSTEXTURE *tex = boundTexture;

    // Large textures (backgrounds) are stored 16-bit to halve their VRAM
    // footprint: the PS2 only has 4 MiB of VRAM and a single 1024x512 CT32
    // background (2 MiB) makes gsKit thrash, re-uploading every texture each
    // frame. Small sprites stay CT32 so their alpha blending is unaffected.
    bool use16 = (width * height) > (256 * 256);
    u32 bytesPerPixel = use16 ? 2 : 4;

    if (tex->Mem != nullptr && (tex->Width != width || tex->Height != height))
    {
        FreeTextureData(tex);
    }
    if (tex->Mem == nullptr)
    {
        tex->Mem = (u32 *)memalign(128, width * height * bytesPerPixel);
    }
    if (tex->Mem == nullptr)
    {
        utils::DebugPrint2("GsKitGfx: out of memory for %ux%u texture (%u KiB)", width, height,
                           (width * height * bytesPerPixel) / 1024);
        tex->Width = 0;
        tex->Height = 0;
        return;
    }
    tex->Width = width;
    tex->Height = height;
    tex->PSM = use16 ? GS_PSM_CT16 : GS_PSM_CT32;

    if (use16)
    {
        ConvertPixels16((u16 *)tex->Mem, data, width * height, fmt, type);
    }
    else
    {
        ConvertPixels(tex->Mem, data, width * height, fmt, type);
    }

    gsKit_setup_tbw(tex);
    gsKit_TexManager_invalidate(gs, tex);
}

void GsKitGfx::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data)
{
    if (boundTexture == nullptr || boundTexture->Mem == nullptr)
    {
        return;
    }
    const u8 *src = (const u8 *)data;
    if (boundTexture->PSM == GS_PSM_CT16)
    {
        u16 *mem = (u16 *)boundTexture->Mem;
        for (i32 row = 0; row < height; row++)
        {
            u16 *dst = mem + (yoffset + row) * boundTexture->Width + xoffset;
            for (i32 col = 0; col < width; col++)
            {
                const u8 *p = src + (row * width + col) * 3;
                dst[col] = (p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) | 0x8000;
            }
        }
    }
    else
    {
        for (i32 row = 0; row < height; row++)
        {
            u32 *dst = boundTexture->Mem + (yoffset + row) * boundTexture->Width + xoffset;
            for (i32 col = 0; col < width; col++)
            {
                const u8 *p = src + (row * width + col) * 3;
                dst[col] = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | 0x80000000;
            }
        }
    }
    gsKit_TexManager_invalidate(gs, boundTexture);
}

void GsKitGfx::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels)
{
    // Screenshots are not supported on PS2; hand back black
    (void)x;
    (void)y;
    std::memset((void *)pixels, 0, (size_t)width * height * 4);
}

void GsKitGfx::Draw(PrimitiveType type, i32 start, i32 count)
{
    if (count == 0)
    {
        return;
    }

    i32 increment = type == PRIM_TRIANGLE_STRIP ? 1 : 3;
    i32 index = start;
    i32 lastIndex = start + count;
    if (type == PRIM_TRIANGLE_STRIP)
    {
        lastIndex -= 2;
    }

    ZunMatrix modelview = view * model;
    bool textured = useTexCoord && boundTexture != nullptr && boundTexture->Mem != nullptr;
    GSTEXTURE *tex = textured ? boundTexture : nullptr;

    static int drawLogCount = 0;
    bool logThis = drawLogCount < 8;
    if (logThis)
    {
        drawLogCount++;
        ZunVec3 p0 = *(ZunVec3 *)((u8 *)vertexData + vertexStride * start);
        utils::DebugPrint2("Draw#%d type=%d count=%d tex=%d(%ux%u) stride=%u v0=(%d,%d,%d)", drawLogCount, type, count,
                           textured, tex ? tex->Width : 0, tex ? tex->Height : 0, (u32)vertexStride, (int)p0.x,
                           (int)p0.y, (int)p0.z);
    }

    if (textured)
    {
        gsKit_TexManager_bind(gs, tex);
    }

    f32 texW = textured ? (f32)tex->Width : 0.0f;
    f32 texH = textured ? (f32)tex->Height : 0.0f;
    f32 depthDif = depthFar - depthNear;
    u64 flatColor = PackColor(textureFactor, textured);

    while (index < lastIndex)
    {
        f32 sx[3], sy[3], su[3], sv[3];
        i32 sz[3];
        u64 scol[3];

        for (i32 i = 0; i < 3; i++)
        {
            i32 vi = index + i;
            ZunVec3 pos = *(ZunVec3 *)((u8 *)vertexData + vertexStride * vi);

            ZunVec4 clip = projection * (modelview * ZunVec4(pos, 1.0f));
            f32 invW = clip.w != 0.0f ? 1.0f / clip.w : 1.0f;
            f32 ndcX = clip.x * invW;
            f32 ndcY = clip.y * invW;
            f32 ndcZ = clip.z * invW;

            // NDC to game window space, then scale into the GS framebuffer
            sx[i] = ((ndcX + 1.0f) * 0.5f * viewport[2] + viewport[0]) * scaleX;
            sy[i] = ((1.0f - (ndcY + 1.0f) * 0.5f) * viewport[3] + viewport[1]) * scaleY;

            f32 depth = (ndcZ * 0.5f + 0.5f) * depthDif + depthNear;
            if (depth < 0.0f)
            {
                depth = 0.0f;
            }
            if (depth > 1.0f)
            {
                depth = 1.0f;
            }
            sz[i] = (i32)((1.0f - depth) * 65535.0f);

            if (textured)
            {
                ZunVec2 tc = *(ZunVec2 *)((u8 *)texCoordData + texCoordStride * vi);
                ZunVec4 tcClip = textureMatrix * ZunVec4(ZunVec3(tc.x, tc.y, 1.0f), 1.0f);
                su[i] = tcClip.x * texW;
                sv[i] = tcClip.y * texH;
            }

            if (useDiffuse)
            {
                ZunColor diffuse = *(ZunColor *)((u8 *)diffuseData + diffuseStride * vi);
                scol[i] = PackColor(diffuse, textured);
            }
            else
            {
                scol[i] = flatColor;
            }
        }

        if (textured)
        {
            gsKit_prim_triangle_goraud_texture_3d(gs, tex, sx[0], sy[0], sz[0], su[0], sv[0], sx[1], sy[1], sz[1],
                                                  su[1], sv[1], sx[2], sy[2], sz[2], su[2], sv[2], scol[0], scol[1],
                                                  scol[2]);
        }
        else
        {
            gsKit_prim_triangle_gouraud_3d(gs, sx[0], sy[0], sz[0], sx[1], sy[1], sz[1], sx[2], sy[2], sz[2], scol[0],
                                           scol[1], scol[2]);
        }

        index += increment;
    }
}

void GsKitGfx::SwapBuffers()
{
    static u32 frameCount = 0;
    if (frameCount < 3)
    {
        utils::DebugPrint2("GsKitGfx: SwapBuffers #%u", frameCount);
    }
    frameCount++;

    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
    gsKit_TexManager_nextFrame(gs);
}
