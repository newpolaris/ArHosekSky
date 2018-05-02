#include <cassert>
#include <glm/glm.hpp>
#include <HosekSky/ArHosekSkyModel.h>

#include <GLType/GraphicsDevice.h>
#include <GLType/ProgramShader.h>
#include <GLType/GraphicsTexture.h>
#include <tools/gltools.hpp>
#include <Types.h>
#include <Mesh.h>
#include <gli/gli.hpp>
#include "SkyBox.h"

namespace
{
    // Scale factor used for storing physical light units in fp16 floats (equal to 2^-10).
    const float FP16Scale = 0.0009765625f;

    // Utility function to map a XY + Side coordinate to a direction vector
    glm::vec3 MapXYSToDirection(int x, int y, int s, int width, int height)
    {
        float u = ((x + 0.5f) / width) * 2.f - 1.f;
        float v = ((y + 0.5f) / height) * 2.f - 1.f;

        glm::vec3 dir(0.f);

        // https://learnopengl.com/Advanced-OpenGL/Cubemaps
        // +x, -x, +y, -y, +z, -z
        switch(s)
        {
        case 0:
            dir = glm::vec3(1.f, v, u);
            break;
        case 1:
            dir = glm::vec3(-1.f, v, -u);
            break;
        case 2:
            dir = glm::vec3(u, 1.f, v);
            break;
        case 3:
            dir = glm::vec3(u, -1.f, -v);
            break;
        case 4:
            dir = glm::vec3(u, v, -1.f);
            break;
        case 5:
            dir = glm::vec3(-u, v, 1.f);
            break;
        }
        return glm::normalize(dir);
    }

    float angleBetween(const glm::vec3& dir0, const glm::vec3& dir1)
    {
        return std::acos(std::max(glm::dot(dir0, dir1), 0.00001f));
    }
}

SkyCache::SkyCache()
    : m_StateR(nullptr)
    , m_StateG(nullptr)
    , m_StateB(nullptr)
    , m_SunDir(0.f, 1.f, 0.f)
    , m_Albedo(1.f)
    , m_Turbidity(1.f)
{
}

SkyCache::~SkyCache()
{
}

void SkyCache::create()
{
}

bool SkyCache::update(const SkyboxParam& param)
{
    float turbidity = glm::clamp(param.turbidity, 1.f, 100.f);
    glm::vec3 groundAlbedo = glm::clamp(param.groundAlbedo);
    glm::vec3 sunDir = param.sunDir;
    sunDir.y = glm::clamp(param.sunDir.y, 0.f, 1.f);
    sunDir = normalize(sunDir);


    if (sunDir == m_SunDir 
        && groundAlbedo == m_Albedo
        && turbidity == m_Turbidity)
        return false;

    destroy();

    float theta = angleBetween(sunDir, glm::vec3(0, 1, 0));
    float elevation = glm::half_pi<float>() - theta;

    m_StateR = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.r, elevation);
    m_StateG = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.g, elevation);
    m_StateB = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.b, elevation);

    m_SunDir = sunDir;
    m_Albedo = groundAlbedo;
    m_Turbidity = turbidity;

    return true;
}

void SkyCache::destroy()
{
    if (m_StateR != nullptr)
    {
        arhosekskymodelstate_free(m_StateR);
        m_StateR = nullptr;
    }
    if (m_StateG != nullptr)
    {
        arhosekskymodelstate_free(m_StateG);
        m_StateG = nullptr;
    }

    if (m_StateB != nullptr)
    {
        arhosekskymodelstate_free(m_StateB);
        m_StateB = nullptr;
    }
}

Skybox::Skybox()
{
}

Skybox::~Skybox()
{
    destroy();
}

void Skybox::destroy()
{
    m_CubeMesh.destroy();
}

void Skybox::create()
{
    auto device = getDevice();

    m_SkyShader = std::make_shared<ProgramShader>();
    m_SkyShader->setDevice(device);
    m_SkyShader->create();
    m_SkyShader->addShader(GL_VERTEX_SHADER, "Skybox.Vertex");
    m_SkyShader->addShader(GL_FRAGMENT_SHADER, "Skybox.Fragment");
    m_SkyShader->link();

    m_CubeMesh.create();
}

void Skybox::update(const SkyboxParam& param)
{
    //
    // Use code from 'BakingLab'
    //
    // Update the cache, if necessary
    if (!m_SkyCache.update(param) && m_CubemapTex)
        return;

    const uint32_t numFace = 6;
    const uint32_t cubemapRes = 128;
    std::vector<uint64_t> texels;
    texels.resize(cubemapRes * cubemapRes * numFace);

    gli::texture texture(
        gli::texture::target_type::TARGET_CUBE,
        gli::texture::format_type::FORMAT_RGBA16_SFLOAT_PACK16,
        gli::extent3d(cubemapRes, cubemapRes, 1),
        1, numFace, 1);

    assert(texture.size() == cubemapRes*cubemapRes*numFace*sizeof(uint64_t));

    for (int s = 0; s < numFace; s++)
    {
        auto texels = texture.data<uint64_t>(0, s, 0);
        for (int y = 0; y < cubemapRes; y++)
        {
            for (int x = 0; x < cubemapRes; x++)
            {
                glm::vec3 dir = MapXYSToDirection(x, y, s, cubemapRes, cubemapRes);
                glm::vec3 radiance = SampleSky(m_SkyCache, dir);
                
                uint32_t idx = y*cubemapRes + x;
                texels[idx] = glm::packHalf4x16(glm::vec4(radiance, 1.f));
            }
        }
    }

    auto device = getDevice();
    m_CubemapTex = device->createTexture(texture);

    // glDepthMask(GL_TRUE);
    // glEnable(GL_DEPTH_TEST);

    CHECKGLERROR();
}

void Skybox::render(const glm::mat4& view, const glm::mat4& projection)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    m_SkyShader->bind();
    m_SkyShader->setUniform("uView", view);
    m_SkyShader->setUniform("uProjection", projection);
    m_SkyShader->bindTexture("uTexSource", m_CubemapTex, 0);
    m_CubeMesh.draw();
    glEnable(GL_DEPTH_TEST);
}

glm::vec3 Skybox::SampleSky(const SkyCache& cache, glm::vec3 sampleDir)
{
    assert(cache.m_StateR != nullptr);

    float gamma = angleBetween(sampleDir, cache.m_SunDir);
    float theta = angleBetween(sampleDir, glm::vec3(0, 1, 0));

    glm::vec3 radiance;
    radiance.r = (float)arhosek_tristim_skymodel_radiance(cache.m_StateR, theta, gamma, 0);
    radiance.g = (float)arhosek_tristim_skymodel_radiance(cache.m_StateG, theta, gamma, 1);
    radiance.b = (float)arhosek_tristim_skymodel_radiance(cache.m_StateB, theta, gamma, 2);

    // Multiply by standard luminous efficacy of 683 lm/W to bring us in line with the photometric
    // units used during rendering
    radiance *= 683.0f;

    radiance *= FP16Scale;
    radiance /= 20.f;

    return radiance;
}

void Skybox::setDevice(const GraphicsDevicePtr& device) noexcept
{
    m_Device = device;
}

GraphicsDevicePtr Skybox::getDevice() noexcept
{
    return m_Device.lock();
}