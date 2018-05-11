#include "PostEffects.h"

#include <vector>
#include <Types.h>
#include <GLType/ProgramShader.h>
#include <GLType/GraphicsDevice.h>
#include <GLType/GraphicsTexture.h>

namespace posteffects
{
    const uint32_t DownsampleGroudSize = 16;

    GraphicsDevicePtr getDevice();

    void extractLuma(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept;
    void updateExposure(std::vector<GraphicsTexturePtr>& textures) noexcept;
    void processBloom() noexcept;

    GraphicsDeviceWeakPtr m_Device;
    ShaderPtr m_ExtractLuma;
    ShaderPtr m_DownsamplingLuma;
    std::vector<GraphicsTexturePtr> m_DownsampledLumaTextures;
}

void posteffects::initialize(const GraphicsDevicePtr& device) noexcept
{
    assert(device);

    m_ExtractLuma = std::make_shared<ProgramShader>();
    m_ExtractLuma->setDevice(device);
    m_ExtractLuma->create();
    m_ExtractLuma->addShader(GL_COMPUTE_SHADER, "ExtractLuma.Compute");
    m_ExtractLuma->link();

    m_DownsamplingLuma = std::make_shared<ProgramShader>();
    m_DownsamplingLuma->setDevice(device);
    m_DownsamplingLuma->create();
    m_DownsamplingLuma->addShader(GL_COMPUTE_SHADER, "DownsamplingLuma.Compute");
    m_DownsamplingLuma->link();

    m_Device = device;
}

void posteffects::shutdown() noexcept
{
}

GraphicsDevicePtr posteffects::getDevice()
{
    auto device = m_Device.lock();
    assert(device);
    return device;
}

void posteffects::extractLuma(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept
{
    auto width = source->getGraphicsTextureDesc().getWidth();
    auto height = source->getGraphicsTextureDesc().getHeight();

    m_ExtractLuma->bind();
    m_ExtractLuma->bindImage("uSource", source, 0, 0, false, 0, GL_READ_ONLY);
    m_ExtractLuma->bindImage("uTarget", target, 1, 0, false, 0, GL_WRITE_ONLY);
    m_ExtractLuma->Dispatch2D(width, height, DownsampleGroudSize, DownsampleGroudSize);
}

void posteffects::updateExposure(std::vector<GraphicsTexturePtr>& textures) noexcept
{
    const int numReduction = textures.size();

    m_DownsamplingLuma->bind();
    for (int i = 0; i < numReduction - 1; i++)
    {
        auto& source = textures[i];
        auto& target = textures[i+1];
        auto width = source->getGraphicsTextureDesc().getWidth();
        auto height = source->getGraphicsTextureDesc().getHeight();
        m_DownsamplingLuma->bindImage("uSource", source, 0, 0, false, 0, GL_READ_ONLY);
        m_DownsamplingLuma->bindImage("uTarget", target, 1, 0, false, 0, GL_WRITE_ONLY);
        m_DownsamplingLuma->Dispatch2D(width, height, DownsampleGroudSize, DownsampleGroudSize);
    }
}

void posteffects::processBloom() noexcept
{
    // m_BloomExtract->bind();
}

void posteffects::render(const GraphicsTexturePtr& source) noexcept
{
    assert(m_DownsampledLumaTextures.size() > 1);

    auto logLumaFullTexture = m_DownsampledLumaTextures.front();
    auto Texture = m_DownsampledLumaTextures.front();

    extractLuma(source, logLumaFullTexture);
    updateExposure(m_DownsampledLumaTextures);
    processBloom();
}

void posteffects::framesizeChange(int32_t width, int32_t height) noexcept
{
    auto device = getDevice();
    auto w = width, h = height;

    GraphicsTextureDesc lumaDesc;
    lumaDesc.setWidth(w);
    lumaDesc.setHeight(h);
    lumaDesc.setFormat(gli::FORMAT_R16_SFLOAT_PACK16);
    m_DownsampledLumaTextures.emplace_back(std::move(device->createTexture(lumaDesc)));

    while (w > 1 && h > 1)
    {
        w = Math::DivideByMultiple(w, DownsampleGroudSize);
        h = Math::DivideByMultiple(h, DownsampleGroudSize);

        lumaDesc.setWidth(w);
        lumaDesc.setHeight(h);
        m_DownsampledLumaTextures.emplace_back(std::move(device->createTexture(lumaDesc)));
    }
}
