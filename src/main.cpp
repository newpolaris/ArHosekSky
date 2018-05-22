#include <GL/glew.h>
#include <glfw3.h>

// GLM for matrix transformation
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

#include <tools/gltools.hpp>
#include <tools/Profile.h>
#include <tools/imgui.h>
#include <tools/TCamera.h>

#include <GLType/GraphicsDevice.h>
#include <GLType/GraphicsData.h>
#include <GLType/OGLDevice.h>
#include <GLType/ProgramShader.h>
#include <GLType/GraphicsFramebuffer.h>

#include <GLType/OGLTexture.h>
#include <GLType/OGLCoreTexture.h>
#include <GLType/OGLCoreFramebuffer.h>

#include <GraphicsTypes.h>
#include <Skybox.h>

#include <fstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <GameCore.h>

#include "HosekSky/ArHosekSkyModel.h"
#include "PostProcess.h"
#include "Sampling.h"
#include "Spectrum.h"

enum ProfilerType { ProfilerTypeRender = 0 };

namespace 
{
    float s_CpuTick = 0.f;
    float s_GpuTick = 0.f;
}

struct SceneSettings
{
    bool bProfile = true;
    bool bUiChanged = false;
    bool bResized = false;
    bool bUpdated = true;

    bool bEnableSun = true;
    float angle = 76.f;
    float turbidity = 1.f;
    float exposure = -16.0f;
    float sunSize = 0.27f;
    glm::vec3 groundAlbedo = glm::vec3(0.5f);

    const float baseSunSize = 0.27f;
};

class ArHosekSky final : public gamecore::IGameApp
{
public:
	ArHosekSky() noexcept;
	virtual ~ArHosekSky() noexcept;

	virtual void startup() noexcept override;
	virtual void closeup() noexcept override;
	virtual void update() noexcept override;
    virtual void updateHUD() noexcept override;
	virtual void render() noexcept override;

	virtual void keyboardCallback(uint32_t c, bool bPressed) noexcept override;
	virtual void framesizeCallback(int32_t width, int32_t height) noexcept override;
	virtual void motionCallback(float xpos, float ypos, bool bPressed) noexcept override;
	virtual void mouseCallback(float xpos, float ypos, bool bPressed) noexcept override;

	GraphicsDevicePtr createDevice(const GraphicsDeviceDesc& desc) noexcept;

private:

    glm::vec3 SunIlluminance();
    glm::vec3 SunLuminance();
    glm::vec3 SunLuminance(bool& cached);

    std::vector<glm::vec2> m_Samples;
    Skybox m_Skybox;
    SceneSettings m_Settings;
	TCamera m_Camera;
    GraphicsTexturePtr m_ScreenColorTex;
    GraphicsFramebufferPtr m_ColorRenderTarget;
    GraphicsDevicePtr m_Device;
};

CREATE_APPLICATION(ArHosekSky);

ArHosekSky::ArHosekSky() noexcept
{
}

ArHosekSky::~ArHosekSky() noexcept
{
}

void ArHosekSky::startup() noexcept
{
	GraphicsDeviceDesc deviceDesc;
#if __APPLE__
	deviceDesc.setDeviceType(GraphicsDeviceType::GraphicsDeviceTypeOpenGL);
#else
	deviceDesc.setDeviceType(GraphicsDeviceType::GraphicsDeviceTypeOpenGLCore);
#endif
	m_Device = createDevice(deviceDesc);
	assert(m_Device);

    SampledSpectrum::initialize();
	profiler::initialize();
    postprocess::initialize(m_Device);

    m_Skybox.setDevice(m_Device);
    m_Skybox.create();

    m_Camera.setFov(80.f);
	m_Camera.setViewParams(glm::vec3(2.0f, 5.0f, 15.0f), glm::vec3(2.0f, 0.0f, 0.0f));
	m_Camera.setMoveCoefficient(0.35f);
}

void ArHosekSky::closeup() noexcept
{
	profiler::shutdown();
}

void ArHosekSky::update() noexcept
{
    bool bCameraUpdated = m_Camera.update();

    static int32_t preWidth = 0;
    static int32_t preHeight = 0;

    int32_t width = getFrameWidth();
    int32_t height = getFrameHeight();
    bool bResized = false;
    if (preWidth != width || preHeight != height)
    {
        preWidth = width, preHeight = height;
        bResized = true;
    }
    m_Settings.bUpdated = (m_Settings.bUiChanged || bCameraUpdated || bResized);
    if (m_Settings.bUpdated)
    {
        float angle = glm::radians(m_Settings.angle);
        glm::vec3 sunDir = glm::vec3(0.0f, glm::cos(angle), -glm::sin(angle));
        
        SkyboxParam param;
        param.groundAlbedo = m_Settings.groundAlbedo;
        param.position = m_Camera.getPosition();
        param.view = m_Camera.getViewMatrix();
        param.projection = m_Camera.getProjectionMatrix();
        param.turbidity = m_Settings.turbidity;
        param.sunDir = normalize(sunDir);
        param.sunColor = SunLuminance();

        m_Skybox.update(param);
    }

    postprocess::update(m_Settings.exposure);
}

void ArHosekSky::updateHUD() noexcept
{
    bool bUpdated = false;
    float width = (float)getWindowWidth(), height = (float)getWindowHeight();

    ImGui::SetNextWindowPos(
        ImVec2(width - width / 4.f - 10.f, 10.f),
        ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Settings",
        NULL,
        ImVec2(width / 4.0f, height - 20.0f),
        ImGuiWindowFlags_AlwaysAutoResize);
    bUpdated |= ImGui::Checkbox("Always redraw", &m_Settings.bProfile);
    bUpdated |= ImGui::Checkbox("Enable Sun", &m_Settings.bEnableSun);
    bUpdated |= ImGui::SliderFloat("Sun Angle", &m_Settings.angle, 0.f, 120.f);
    bUpdated |= ImGui::SliderFloat("Sun Size", &m_Settings.sunSize, 0.01f, 120.f);
    bUpdated |= ImGui::SliderFloat("Turbidity", &m_Settings.turbidity, 1.f, 100.f);
    bUpdated |= ImGui::SliderFloat("Exposure", &m_Settings.exposure, -8.f, 8.f);
    ImGui::ColorWheel("Ground albedo", glm::value_ptr<float>(m_Settings.groundAlbedo), 12.f);
    ImGui::Text("CPU %s: %10.5f ms\n", "main", s_CpuTick);
    ImGui::Text("GPU %s: %10.5f ms\n", "main", s_GpuTick);
    ImGui::PushItemWidth(180.0f);
    ImGui::Indent();
    ImGui::Unindent();
    ImGui::End();

    m_Settings.bUiChanged = bUpdated;
}

void ArHosekSky::render() noexcept
{
    bool bUpdate = m_Settings.bProfile || m_Settings.bUpdated;

    profiler::start(ProfilerTypeRender);
    if (bUpdate)
    {
        auto& desc = m_ScreenColorTex->getGraphicsTextureDesc();
        m_Device->setFramebuffer(m_ColorRenderTarget);
        GLenum clearFlag = GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT;
        glViewport(0, 0, desc.getWidth(), desc.getHeight());
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepthf(1.0f);
        glClear(clearFlag);

        m_Skybox.render(
            m_Settings.bEnableSun,
            m_Settings.sunSize,
            SunLuminance(),
            m_Camera.getViewMatrix(),
            m_Camera.getProjectionMatrix());
    }
    postprocess::render(m_ScreenColorTex);

    profiler::stop(ProfilerTypeRender);
    profiler::tick(ProfilerTypeRender, s_CpuTick, s_GpuTick);

}

void ArHosekSky::keyboardCallback(uint32_t key, bool isPressed) noexcept
{
	switch (key)
	{
	case GLFW_KEY_UP:
		m_Camera.keyboardHandler(MOVE_FORWARD, isPressed);
		break;

	case GLFW_KEY_DOWN:
		m_Camera.keyboardHandler(MOVE_BACKWARD, isPressed);
		break;

	case GLFW_KEY_LEFT:
		m_Camera.keyboardHandler(MOVE_LEFT, isPressed);
		break;

	case GLFW_KEY_RIGHT:
		m_Camera.keyboardHandler(MOVE_RIGHT, isPressed);
		break;
	}
}

void ArHosekSky::framesizeCallback(int32_t width, int32_t height) noexcept
{
	float aspectRatio = (float)width/height;
	m_Camera.setProjectionParams(45.0f, aspectRatio, 0.1f, 100.0f);

    GraphicsTextureDesc colorDesc;
    colorDesc.setWidth(width);
    colorDesc.setHeight(height);
    colorDesc.setFormat(gli::FORMAT_RGBA16_SFLOAT_PACK16);
    m_ScreenColorTex = m_Device->createTexture(colorDesc);

    GraphicsTextureDesc depthDesc;
    depthDesc.setWidth(width);
    depthDesc.setHeight(height);
    depthDesc.setFormat(gli::FORMAT_D24_UNORM_S8_UINT_PACK32);
    auto depthTex = m_Device->createTexture(depthDesc);

    GraphicsFramebufferDesc desc;  
    desc.addComponent(GraphicsAttachmentBinding(m_ScreenColorTex, GL_COLOR_ATTACHMENT0));
    desc.addComponent(GraphicsAttachmentBinding(depthTex, GL_DEPTH_ATTACHMENT));
    
    m_ColorRenderTarget = m_Device->createFramebuffer(desc);;

    postprocess::framesizeChange(width, height);
}

void ArHosekSky::motionCallback(float xpos, float ypos, bool bPressed) noexcept
{
	const bool mouseOverGui = ImGui::MouseOverArea();
	if (!mouseOverGui && bPressed) m_Camera.motionHandler(int(xpos), int(ypos), false);    
}

void ArHosekSky::mouseCallback(float xpos, float ypos, bool bPressed) noexcept
{
	const bool mouseOverGui = ImGui::MouseOverArea();
	if (!mouseOverGui && bPressed) m_Camera.motionHandler(int(xpos), int(ypos), true); 
}

GraphicsDevicePtr ArHosekSky::createDevice(const GraphicsDeviceDesc& desc) noexcept
{
	GraphicsDeviceType deviceType = desc.getDeviceType();

#if __APPLE__
	assert(deviceType != GraphicsDeviceType::GraphicsDeviceTypeOpenGLCore);
#endif

	if (deviceType == GraphicsDeviceType::GraphicsDeviceTypeOpenGL ||
		deviceType == GraphicsDeviceType::GraphicsDeviceTypeOpenGLCore)
    {
        auto device = std::make_shared<OGLDevice>();
        if (device->create(desc))
            return device;
        return nullptr;
    }
    return nullptr;
}

// Returns the result of performing a irradiance/illuminance integral over the portion
// of the hemisphere covered by a region with angular radius = theta
static float IlluminanceIntegral(float theta)
{
    const float Pi = glm::pi<float>();
    float cosTheta = std::cos(theta);
    return Pi * (1.0f - (cosTheta * cosTheta));
}

glm::vec3 ArHosekSky::SunLuminance(bool& cached)
{
    const float Pi = glm::pi<float>();
    const float Pi_2 = glm::half_pi<float>();
    const float FP16Scale = 0.0009765625f;

    float angle = glm::radians(m_Settings.angle);
    glm::vec3 sunDirection = glm::vec3(0.0f, glm::cos(angle), -glm::sin(angle));
    sunDirection.y = glm::clamp(sunDirection.y, 0.f, 1.f);
    sunDirection = glm::normalize(sunDirection);
    const float turbidity = glm::clamp(m_Settings.turbidity, 1.0f, 32.0f);
    const float intensityScale = 1.f;
    const glm::vec3 tintColor = glm::vec3(1.f);
    const bool normalizeIntensity = false;
    const float sunSize = m_Settings.sunSize;

    static float turbidityCache = 2.0f;
    static glm::vec3 sunDirectionCache = glm::vec3(-0.579149902f, 0.754439294f, -0.308879942f);
    static glm::vec3 luminanceCache = glm::vec3(1.61212531e+009f, 1.36822630e+009f, 1.07235315e+009f) * FP16Scale;
    static glm::vec3 sunTintCache = glm::vec3(1.0f, 1.0f, 1.0f);
    static float sunIntensityCache = 1.0f;
    static bool normalizeCache = false;
    static float sunSizeCache = m_Settings.baseSunSize;

    if(turbidityCache == turbidity && sunDirection == sunDirectionCache
        && intensityScale == sunIntensityCache && tintColor == sunTintCache
        && normalizeCache == normalizeIntensity && sunSize == sunSizeCache)
    {
        cached = true;
        return luminanceCache;
    }

    cached = false;

    float thetaS = std::acos(1.0f - sunDirection.y);
    float elevation = Pi_2 - thetaS;

    // Get the sun's luminance, then apply tint and scale factors
    glm::vec3 sunLuminance(0.f);

    // For now, we'll compute an average luminance value from Hosek solar radiance model, even though
    // we could compute illuminance directly while we're sampling the disk
    SampledSpectrum groundAlbedoSpectrum = SampledSpectrum::FromRGB(m_Settings.groundAlbedo);
    SampledSpectrum solarRadiance;

    const uint64_t NumDiscSamples = 8;
    for (uint64_t x = 0; x < NumDiscSamples; ++x)
    {
        for (uint64_t y = 0; y < NumDiscSamples; ++y)
        {
            float u = (x + 0.5f) / NumDiscSamples;
            float v = (y + 0.5f) / NumDiscSamples;
            glm::vec2 discSamplePos = SquareToConcentricDiskMapping(u, v);

            float theta = elevation + discSamplePos.y * glm::radians(m_Settings.baseSunSize);
            float gamma = discSamplePos.x * glm::radians(m_Settings.baseSunSize);

            for (int32_t i = 0; i < NumSpectralSamples; ++i)
            {
                ArHosekSkyModelState* skyState = arhosekskymodelstate_alloc_init(elevation, turbidity, groundAlbedoSpectrum[i]);
                float wavelength = glm::mix(float(SampledLambdaStart), float(SampledLambdaEnd), i / float(NumSpectralSamples));

                solarRadiance[i] = float(arhosekskymodel_solar_radiance(skyState, theta, gamma, wavelength));

                arhosekskymodelstate_free(skyState);
                skyState = nullptr;
            }

            glm::vec3 sampleRadiance = solarRadiance.ToRGB() * FP16Scale;
            sunLuminance += sampleRadiance;
        } 
    }

    // Account for luminous efficiency, coordinate system scaling, and sample averaging
    sunLuminance *= 683.0f * 100.0f * (1.0f / NumDiscSamples) * (1.0f / NumDiscSamples);

    sunLuminance = sunLuminance * tintColor;
    sunLuminance = sunLuminance * intensityScale;

    if (normalizeIntensity)
    {
        // Normalize so that the intensity stays the same even when the sun is bigger or smaller
        const float baseIntegral = IlluminanceIntegral(glm::radians(m_Settings.baseSunSize));
        const float currIntegral = IlluminanceIntegral(glm::radians(m_Settings.sunSize));
        sunLuminance *= (baseIntegral / currIntegral);
    }

    turbidityCache = turbidity;
    sunDirectionCache = sunDirection;
    luminanceCache = sunLuminance;
    sunIntensityCache = intensityScale;
    sunTintCache = tintColor;
    normalizeCache = normalizeIntensity;
    sunSizeCache = sunSize;

    return sunLuminance;
}

glm::vec3 ArHosekSky::SunLuminance()
{
    bool cached = false;
    return SunLuminance(cached);
}

glm::vec3 ArHosekSky::SunIlluminance()
{
    glm::vec3 sunLuminance = SunLuminance();

    // Compute partial integral over the hemisphere in order to compute illuminance
    float theta = glm::radians(m_Settings.sunSize);
    float integralFactor = IlluminanceIntegral(theta);

    return sunLuminance * integralFactor;
}