#pragma once

#include <vector>
#include <string>
#include <memory>
#include <Types.h>
#include <Mesh.h>
#include <GraphicsTypes.h>

#include "Spectrum.h"

// HosekSky forward declares
struct ArHosekSkyModelState;

struct SkyboxParam
{
    glm::vec3 sunDir;
    glm::vec3 sunColor;
    glm::vec3 groundAlbedo;
    glm::vec3 position;
    glm::mat4 view;
    glm::mat4 projection;
    float turbidity;
};

// Cached data for the procedural sky model
class SkyCache final
{
public:

    SkyCache();
    ~SkyCache();

    void create();
    void destroy();
    bool update(const SkyboxParam& param);

private:

    friend class Skybox;

    ArHosekSkyModelState* m_StateR;
    ArHosekSkyModelState* m_StateG;
    ArHosekSkyModelState* m_StateB;

    glm::vec3 m_SunDir;
    glm::vec3 m_Albedo;
    float m_Turbidity;
};

class Skybox final
{
public:

    Skybox();
    ~Skybox();

    void create();
    void destroy();
    void update(const SkyboxParam& param);
    void render(bool bEnableSun, float sunSize, glm::vec3 sunColor, glm::mat4 view, glm::mat4 projection);

    GraphicsDevicePtr getDevice() noexcept;
    void setDevice(const GraphicsDevicePtr& device) noexcept;

    static glm::vec3 SampleSky(const SkyCache& cache, glm::vec3 sampleDir);

private:

    SkyCache m_SkyCache;
    ShaderPtr m_SkyShader;
    CubeMesh m_CubeMesh;
    GraphicsTexturePtr m_SkyCubemapTex;
    GraphicsDeviceWeakPtr m_Device;
};