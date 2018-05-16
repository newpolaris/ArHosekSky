#pragma once

#include "GLType/GraphicsFramebuffer.h"

class OGLFramebuffer final : public GraphicsFramebuffer
{
    __DeclareSubInterface(OGLFramebuffer, GraphicsFramebuffer)
public:

    OGLFramebuffer() noexcept;
    ~OGLFramebuffer() noexcept;

    const GraphicsFramebufferDesc& getGraphicsFramebufferDesc() const noexcept;

    bool create(const GraphicsFramebufferDesc& desc) noexcept;
	void destroy() noexcept;

    void bind() noexcept;

private:

	friend class OGLDevice;
	void setDevice(GraphicsDevicePtr device) noexcept;
	GraphicsDevicePtr getDevice() noexcept;

private:

    std::uint32_t m_FBO;
	GraphicsDeviceWeakPtr m_Device;
    GraphicsFramebufferDesc m_FramebufferDesc;
};
