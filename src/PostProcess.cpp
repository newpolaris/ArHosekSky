#include "PostProcess.h"

#include <vector>
#include <Types.h>
#include <Mesh.h>
#include <GLType/ProgramShader.h>
#include <GLType/GraphicsDevice.h>
#include <GLType/GraphicsTexture.h>
#include <GLType/GraphicsFramebuffer.h>

namespace postprocess
{
    const uint32_t DownsampleGroudSize = 16;

    GraphicsDevicePtr getDevice();

    void extractLuminance(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept;
    void processBloom(const GraphicsTexturePtr& source, GraphicsTexturePtr target) noexcept;
    void updateExposure(std::vector<GraphicsTexturePtr>& textures) noexcept;

    float m_Exposure;
    uint32_t m_FrameWidth, m_FrameHeight;
    GraphicsDeviceWeakPtr m_Device;
    ShaderPtr m_ExtractLuminance;
    ShaderPtr m_DownsamplingLuma;
    ShaderPtr m_BlurVert, m_BlurHori;
    ShaderPtr m_Bloom;
    ShaderPtr m_BlitColor;
    FullscreenTriangleMesh m_ScreenTraingle;
    GraphicsTexturePtr m_BlurTexture;
    GraphicsTexturePtr m_BloomTexture;
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

    m_ExtractLuminance = std::make_shared<ProgramShader>();
    m_ExtractLuminance->setDevice(device);
    m_ExtractLuminance->create();
    m_ExtractLuminance->addShader(GL_COMPUTE_SHADER, "ExtractLuminance.Compute");
    m_ExtractLuminance->link();

    m_DownsamplingLuma = std::make_shared<ProgramShader>();
    m_DownsamplingLuma->setDevice(device);
    m_DownsamplingLuma->create();
    m_DownsamplingLuma->addShader(GL_COMPUTE_SHADER, "DownsamplingLuma.Compute");
    m_DownsamplingLuma->link();

    m_Bloom = std::make_shared<ProgramShader>();
    m_Bloom->setDevice(device);
    m_Bloom->create();
    m_Bloom->addShader(GL_VERTEX_SHADER, "Bloom.Vertex");
    m_Bloom->addShader(GL_FRAGMENT_SHADER, "Bloom.Fragment");
    m_Bloom->link();

    m_BlurHori = std::make_shared<ProgramShader>();
    m_BlurHori->setDevice(device);
    m_BlurHori->create();
	m_BlurHori->addShader(GL_VERTEX_SHADER, "BlurHorizontal.Vertex");
	m_BlurHori->addShader(GL_FRAGMENT_SHADER, "BlurHorizontal.Fragment");
    m_BlurHori->link();

    m_BlurVert = std::make_shared<ProgramShader>();
    m_BlurVert->setDevice(device);
    m_BlurVert->create();
	m_BlurVert->addShader(GL_VERTEX_SHADER, "BlurVertical.Vertex");
	m_BlurVert->addShader(GL_FRAGMENT_SHADER, "BlurVertical.Fragment");
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

void postprocess::extractLuminance(const GraphicsTexturePtr& source, GraphicsTexturePtr& target) noexcept
{
    auto width = source->getGraphicsTextureDesc().getWidth();
    auto height = source->getGraphicsTextureDesc().getHeight();

    m_ExtractLuminance->bind();
    m_ExtractLuminance->bindImage("uTexSource", source, 0, 0, false, 0, GL_READ_ONLY);
    m_ExtractLuminance->bindImage("uTexTarget", target, 1, 0, false, 0, GL_WRITE_ONLY);
    m_ExtractLuminance->Dispatch2D(width, height, DownsampleGroudSize, DownsampleGroudSize);
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

void postprocess::processBloom(const GraphicsTexturePtr& source, GraphicsTexturePtr dest) noexcept
{
    auto device = getDevice();

    device->setFramebuffer(dest->getGraphicsRenderTarget());
    m_Bloom->bind();
    m_Bloom->bindTexture("uTexSource", source, 0);
    m_ScreenTraingle.draw();

    const int numBlurTimes = 2;
    for (int i = 0; i < numBlurTimes; i++)
    {
        device->setFramebuffer(m_BlurTexture->getGraphicsRenderTarget());
        m_BlurVert->bind();
        m_BlurVert->bindTexture("uTexSource", dest, 0);
        m_ScreenTraingle.draw();

        device->setFramebuffer(dest->getGraphicsRenderTarget());
        m_BlurHori->bind();
        m_BlurHori->bindTexture("uTexSource", m_BlurTexture, 0);
        m_ScreenTraingle.draw();
    }
}

void postprocess::render(const GraphicsTexturePtr& source) noexcept
{
    assert(m_DownsampledLumaTextures.size() > 1);

    auto logLumaFullTexture = m_DownsampledLumaTextures.front();
    auto Texture = m_DownsampledLumaTextures.front();

    auto desc = m_BloomTexture->getGraphicsTextureDesc();
    auto width = desc.getWidth();
    auto height = desc.getHeight();

    glViewport(0, 0, width, height);

    // extractLuminance(source, logLumaFullTexture);
    // updateExposure(m_DownsampledLumaTextures);
    processBloom(source, m_BloomTexture);

    // tone mapping
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_FrameWidth, m_FrameHeight);

    glDisable(GL_DEPTH_TEST);
    m_BlitColor->bind();
    m_BlitColor->setUniform("uExposure", m_Exposure);
    m_BlitColor->bindTexture("uTexSource", source, 0);
    m_BlitColor->bindTexture("uTexBloom", m_BloomTexture, 1);
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

    auto halfwidth = width/2;
    auto halfheight = height/2;

    GraphicsTextureDesc halfDesc;
    halfDesc.setWidth(halfwidth);
    halfDesc.setHeight(halfheight);
    halfDesc.setFormat(gli::FORMAT_RGBA16_SFLOAT_PACK16);
    halfDesc.setWrapS(GL_CLAMP);
    halfDesc.setWrapT(GL_CLAMP);
    m_BlurTexture = device->createTexture(halfDesc);
    m_BloomTexture = device->createTexture(halfDesc);

    m_FrameWidth = width;
    m_FrameHeight = height;
}
