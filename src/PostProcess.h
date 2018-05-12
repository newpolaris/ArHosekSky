#pragma once

#include <cstdint>
#include <GraphicsTypes.h>

namespace postprocess
{
    void initialize(const GraphicsDevicePtr& device) noexcept;
    void shutdown() noexcept;
    void update(float exposure) noexcept;
    void render(const GraphicsTexturePtr& source) noexcept;
    void framesizeChange(int32_t width, int32_t height) noexcept;
}