#include "PostProcess.h"

#include <vector>
#include <Types.h>
#include <Mesh.h>
#include <GLType/ProgramShader.h>
#include <GLType/GraphicsDevice.h>
#include <GLType/GraphicsTexture.h>

namespace postprocess
{
    const uint32_t DownsampleGroudSize = 16;

    GraphicsDevicePtr getDevice();

    void extractBrightColor(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept;
    void processBloom(const GraphicsTexturePtr& source) noexcept;
    void updateExposure(std::vector<GraphicsTexturePtr>& textures) noexcept;

    float m_Exposure;
    uint32_t m_FrameWidth, m_FrameHeight;
    GraphicsDeviceWeakPtr m_Device;
    ShaderPtr m_ExtractColor;
    ShaderPtr m_DownsamplingLuma;
    ShaderPtr m_BlurVert, m_BlurHori;
    ShaderPtr m_BlitColor;
    FullscreenTriangleMesh m_ScreenTraingle;
    GraphicsTexturePtr m_BlurTempTexture;
    std::vector<GraphicsTexturePtr> m_DownsampledLumaTextures;
}

void postprocess::initialize(const GraphicsDevicePtr& device) noexcept
{
    assert(device);

    m_ScreenTraingle.create();

    m_BlitColor = std::make_shared<ProgramShader>();
	m_BlitColor->setDevice(device);
	m_BlitColor->create();
	m_BlitColor->addShader(GL_VERTEX_SHADER, "BlitTexture.Vertex");
	m_BlitColor->addShader(GL_FRAGMENT_SHADER, "BlitTexture.Fragment");
	m_BlitColor->link();

    m_ExtractColor = std::make_shared<ProgramShader>();
    m_ExtractColor->setDevice(device);
    m_ExtractColor->create();
    m_ExtractColor->addShader(GL_COMPUTE_SHADER, "ExtractLuminance.Compute");
    m_ExtractColor->link();

    m_ExtractColor = std::make_shared<ProgramShader>();
    m_ExtractColor->setDevice(device);
    m_ExtractColor->create();
    m_ExtractColor->addShader(GL_COMPUTE_SHADER, "ExtractLuminance.Compute");
    m_ExtractColor->link();

    m_DownsamplingLuma = std::make_shared<ProgramShader>();
    m_DownsamplingLuma->setDevice(device);
    m_DownsamplingLuma->create();
    m_DownsamplingLuma->addShader(GL_COMPUTE_SHADER, "DownsamplingLuma.Compute");
    m_DownsamplingLuma->link();

    m_BlurHori = std::make_shared<ProgramShader>();
    m_BlurHori->setDevice(device);
    m_BlurHori->create();
    m_BlurHori->addShader(GL_COMPUTE_SHADER, "BlurHorizontal.Compute");
    m_BlurHori->link();

    m_BlurVert = std::make_shared<ProgramShader>();
    m_BlurVert->setDevice(device);
    m_BlurVert->create();
    m_BlurVert->addShader(GL_COMPUTE_SHADER, "BlurVertical.Compute");
    m_BlurVert->link();


    m_Device = device;
}

void postprocess::shutdown() noexcept
{
    m_ScreenTraingle.destroy();
}

GraphicsDevicePtr postprocess::getDevice()
{
    auto device = m_Device.lock();
    assert(device);
    return device;
}

void postprocess::extractBrightColor(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept
{
    auto width = source->getGraphicsTextureDesc().getWidth();
    auto height = source->getGraphicsTextureDesc().getHeight();

    m_ExtractColor->bind();
    m_ExtractColor->bindImage("uSource", source, 0, 0, false, 0, GL_READ_ONLY);
    m_ExtractColor->bindImage("uTarget", target, 1, 0, false, 0, GL_WRITE_ONLY);
    m_ExtractColor->Dispatch2D(width, height, DownsampleGroudSize, DownsampleGroudSize);
}

void postprocess::updateExposure(std::vector<GraphicsTexturePtr>& textures) noexcept
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

void postprocess::processBloom(const GraphicsTexturePtr& source) noexcept
{
    auto width = source->getGraphicsTextureDesc().getWidth();
    auto height = source->getGraphicsTextureDesc().getHeight();

    const int numBlurTimes = 8;
    for (int i = 0; i < numBlurTimes; i++)
    {
        m_BlurVert->bind();
        m_BlurVert->bindTexture("uSource", source, 0);
        m_BlurVert->bindImage("uTarget", m_BlurTempTexture, 0, 0, false, 0, GL_WRITE_ONLY);
        m_BlurVert->Dispatch2D(width, height);

        m_BlurHori->bind();
        m_BlurHori->bindTexture("uSource", m_BlurTempTexture, 0);
        m_BlurHori->bindImage("uTarget", source, 0, 0, false, 0, GL_WRITE_ONLY);
        m_BlurHori->Dispatch2D(width, height);
    }
}

void postprocess::render(const GraphicsTexturePtr& source) noexcept
{
    assert(m_DownsampledLumaTextures.size() > 1);

    auto logLumaFullTexture = m_DownsampledLumaTextures.front();
    auto Texture = m_DownsampledLumaTextures.front();

    extractBrightColor(source, logLumaFullTexture);
    // updateExposure(m_DownsampledLumaTextures);
    processBloom(logLumaFullTexture);

    // tone mapping
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_FrameWidth, m_FrameHeight);

    glDisable(GL_DEPTH_TEST);
    m_BlitColor->bind();
    m_BlitColor->setUniform("uExposure", m_Exposure);
    m_BlitColor->bindTexture("uTexSource", source, 0);
    m_BlitColor->bindTexture("uTexBloom", logLumaFullTexture, 1);
    m_ScreenTraingle.draw();
    glEnable(GL_DEPTH_TEST);
}

void postprocess::update(float exposure) noexcept
{
    m_Exposure = exposure;
}

void postprocess::framesizeChange(int32_t width, int32_t height) noexcept
{
    auto device = getDevice();

    auto w = width, h = height;

    GraphicsTextureDesc lumaDesc;
    lumaDesc.setWidth(w);
    lumaDesc.setHeight(h);
    lumaDesc.setFormat(gli::FORMAT_RGBA16_SFLOAT_PACK16);
    m_DownsampledLumaTextures.emplace_back(std::move(device->createTexture(lumaDesc)));

    while (w > 1 && h > 1)
    {
        w = Math::DivideByMultiple(w, DownsampleGroudSize);
        h = Math::DivideByMultiple(h, DownsampleGroudSize);

        lumaDesc.setWidth(w);
        lumaDesc.setHeight(h);
        m_DownsampledLumaTextures.emplace_back(std::move(device->createTexture(lumaDesc)));
    }

    GraphicsTextureDesc blurDesc;
    blurDesc.setWidth(width);
    blurDesc.setHeight(height);
    blurDesc.setFormat(gli::FORMAT_RGBA16_SFLOAT_PACK16);
    m_BlurTempTexture = device->createTexture(blurDesc);

    m_FrameWidth = width;
    m_FrameHeight = height;
}
