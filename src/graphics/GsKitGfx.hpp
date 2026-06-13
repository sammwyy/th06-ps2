#pragma once

#include "GfxInterface.hpp"
#include "ZunMath.hpp"
#include <gsKit.h>
#include <vector>

// PS2 renderer backend. Vertices are transformed on the EE with the same math
// the software rasterizer used, then handed to the GS as gouraud (textured)
// triangles through gsKit. VRAM residency is handled by gsKit's TexManager.
struct GsKitGfx : GfxInterface
{
    static GfxInterface *Init();
    ~GsKitGfx() override;

    virtual void SetFogRange(f32 nearPlane, f32 farPlane);
    virtual void SetFogColor(ZunColor color);
    virtual void ToggleVertexAttribute(u8 attr, bool enable);
    virtual void SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr);
    virtual void SetColorOp(TextureOpComponent component, ColorOp op);
    virtual void SetTextureFactor(ZunColor factor);
    virtual void SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix);

    virtual void SetTextureFilter();

    virtual void GetViewport(u32 *viewport);
    virtual void GetDepthRange(f32 *depthRange);
    virtual void SetViewport(i32 x, i32 y, i32 width, i32 height);
    virtual void SetDepthRange(f32 nearPlane, f32 farPlane);

    virtual void Enable(Capabilities cap);
    virtual bool HasError()
    {
        return false;
    }

    virtual void SetBlendMode(BlendMode mode);
    virtual void SetDepthMask(bool enable);
    virtual void SetDepthFunc(DepthFunc func);

    virtual void SetClearDepth(f32 depth);
    virtual void SetClearColor(f32 r, f32 g, f32 b, f32 a);
    virtual void Clear(u32 clearBits);

    virtual GfxTextureHandle CreateTexture();
    virtual void BindTexture(GfxTextureHandle handle);
    virtual void DeleteTexture(GfxTextureHandle handle);
    virtual void SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data);
    virtual void SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data);

    virtual void ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels);

    virtual void Draw(PrimitiveType type, i32 start, i32 count);
    virtual void SwapBuffers();

  private:
    GSGLOBAL *gs = nullptr;

    std::vector<GSTEXTURE *> textures;
    std::vector<u32> freeTextures;
    GSTEXTURE *boundTexture = nullptr;

    ZunMatrix model, view, projection, textureMatrix;

    i32 viewport[4] = {0, 0, 0, 0};
    f32 depthNear = 0.0f, depthFar = 1.0f;
    bool useDepthTest = false;
    bool depthMask = true;
    DepthFunc depthFunc = DEPTH_FUNC_LEQUAL;

    BlendMode blendMode = BLEND_INV_SRC_ALPHA;
    ZunColor textureFactor = 0xFFFFFFFF;
    ZunColor clearColor = 0;
    f32 fogNear = 0.0f, fogFar = 1.0f;
    ZunColor fogColor = 0;

    void *vertexData = nullptr;
    std::size_t vertexStride = 0;
    void *texCoordData = nullptr;
    std::size_t texCoordStride = 0;
    void *diffuseData = nullptr;
    std::size_t diffuseStride = 0;
    bool useTexCoord = false;
    bool useDiffuse = false;

    // Output scale from the game's 640x480 space to the GS framebuffer
    f32 scaleX = 1.0f, scaleY = 1.0f;

    void ApplyTest();
    void ApplyScissor();
    void ApplyDepthMask();
    void ApplyFrameMask(u32 mask);
    void FreeTextureData(GSTEXTURE *tex);
};
