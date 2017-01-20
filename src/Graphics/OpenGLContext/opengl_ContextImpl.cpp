#include <assert.h>
#include <Log.h>
#include <Graphics/Parameters.h>
#include "opengl_ContextImpl.h"
#include "opengl_UnbufferedDrawer.h"
#include "opengl_DummyTextDrawer.h"
#include "opengl_Utils.h"
#include "GLSL/glsl_CombinerProgramBuilder.h"
#include "GLSL/glsl_SpecialShadersFactory.h"
#include "GLSL/glsl_ShaderStorage.h"

using namespace opengl;

ContextImpl::ContextImpl()
{
	initGLFunctions();
}


ContextImpl::~ContextImpl()
{
}

void ContextImpl::init()
{
	m_glInfo.init();

	if (!m_cachedFunctions)
		m_cachedFunctions.reset(new CachedFunctions(m_glInfo));

	{
		TextureManipulationObjectFactory textureObjectsFactory(m_glInfo, *m_cachedFunctions.get());
		m_createTexture.reset(textureObjectsFactory.getCreate2DTexture());
		m_init2DTexture.reset(textureObjectsFactory.getInit2DTexture());
		m_update2DTexture.reset(textureObjectsFactory.getUpdate2DTexture());
		m_set2DTextureParameters.reset(textureObjectsFactory.getSet2DTextureParameters());
	}

	{
		BufferManipulationObjectFactory bufferObjectFactory(m_glInfo, *m_cachedFunctions.get());
		m_fbTexFormats.reset(bufferObjectFactory.getFramebufferTextureFormats());
		m_createFramebuffer.reset(bufferObjectFactory.getCreateFramebufferObject());
		m_createRenderbuffer.reset(bufferObjectFactory.getCreateRenderbuffer());
		m_initRenderbuffer.reset(bufferObjectFactory.getInitRenderbuffer());
		m_addFramebufferRenderTarget.reset(bufferObjectFactory.getAddFramebufferRenderTarget());
		m_createPixelWriteBuffer.reset(bufferObjectFactory.createPixelWriteBuffer());
		m_blitFramebuffers.reset(bufferObjectFactory.getBlitFramebuffers());
	}

	{
		m_graphicsDrawer.reset(new UnbufferedDrawer(m_glInfo, m_cachedFunctions->getCachedVertexAttribArray()));
		m_textDrawer.reset(new DummyTextDrawer);
	}

	m_combinerProgramBuilder.reset(new glsl::CombinerProgramBuilder(m_glInfo, m_cachedFunctions->getCachedUseProgram()));
	m_specialShadersFactory.reset(new glsl::SpecialShadersFactory(m_glInfo,
		m_cachedFunctions->getCachedUseProgram(),
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderEnd()));
}

void ContextImpl::destroy()
{
	m_cachedFunctions.reset();
	m_createTexture.reset();
	m_init2DTexture.reset();
	m_set2DTextureParameters.reset();

	m_createFramebuffer.reset();
	m_createRenderbuffer.reset();
	m_initRenderbuffer.reset();
	m_addFramebufferRenderTarget.reset();


	m_combinerProgramBuilder.reset();
}

void ContextImpl::enable(graphics::Parameter _parameter, bool _enable)
{
	m_cachedFunctions->getCachedEnable(_parameter)->enable(_enable);
}

void ContextImpl::cullFace(graphics::Parameter _mode)
{
	m_cachedFunctions->getCachedCullFace()->setCullFace(_mode);
}

void ContextImpl::enableDepthWrite(bool _enable)
{
	m_cachedFunctions->getCachedDepthMask()->setDepthMask(_enable);
}

void ContextImpl::setDepthCompare(graphics::Parameter _mode)
{
	m_cachedFunctions->getCachedDepthCompare()->setDepthCompare(_mode);
}

void ContextImpl::setViewport(s32 _x, s32 _y, s32 _width, s32 _height)
{
	m_cachedFunctions->getCachedViewport()->setViewport(_x, _y, _width, _height);
}

void ContextImpl::setScissor(s32 _x, s32 _y, s32 _width, s32 _height)
{
	m_cachedFunctions->getCachedScissor()->setScissor(_x, _y, _width, _height);
}

void ContextImpl::setBlending(graphics::Parameter _sfactor, graphics::Parameter _dfactor)
{
	m_cachedFunctions->getCachedBlending()->setBlending(_sfactor, _dfactor);
}

void ContextImpl::setBlendColor(f32 _red, f32 _green, f32 _blue, f32 _alpha)
{
	m_cachedFunctions->getCachedBlendColor()->setBlendColor(_red, _green, _blue, _alpha);
}

void ContextImpl::clearColorBuffer(f32 _red, f32 _green, f32 _blue, f32 _alpha)
{
	CachedEnable * enableScissor = m_cachedFunctions->getCachedEnable(graphics::enable::SCISSOR_TEST);
	enableScissor->enable(false);

	m_cachedFunctions->getCachedClearColor()->setClearColor(_red, _green, _blue, _alpha);
	glClear(GL_COLOR_BUFFER_BIT);

	enableScissor->enable(true);
}

void ContextImpl::clearDepthBuffer()
{
	CachedEnable * enableScissor = m_cachedFunctions->getCachedEnable(graphics::enable::SCISSOR_TEST);
	CachedDepthMask * depthMask = m_cachedFunctions->getCachedDepthMask();
	enableScissor->enable(false);

#ifdef ANDROID
	depthMask->setDepthMask(false);
	glClear(GL_DEPTH_BUFFER_BIT);
#endif

	depthMask->setDepthMask(true);
	glClear(GL_DEPTH_BUFFER_BIT);

	enableScissor->enable(true);
}

void ContextImpl::setPolygonOffset(f32 _factor, f32 _units)
{
	glPolygonOffset(_factor, _units);
}

/*---------------Texture-------------*/

graphics::ObjectHandle ContextImpl::createTexture(graphics::Parameter _target)
{
	return m_createTexture->createTexture(_target);
}

void ContextImpl::deleteTexture(graphics::ObjectHandle _name)
{
	u32 glName(_name);
	glDeleteTextures(1, &glName);
	m_init2DTexture->reset(_name);
}

void ContextImpl::init2DTexture(const graphics::Context::InitTextureParams & _params)
{
	m_init2DTexture->init2DTexture(_params);
}

void ContextImpl::update2DTexture(const graphics::Context::UpdateTextureDataParams & _params)
{
	m_update2DTexture->update2DTexture(_params);
}

void ContextImpl::setTextureParameters(const graphics::Context::TexParameters & _parameters)
{
	m_set2DTextureParameters->setTextureParameters(_parameters);
}

/*---------------Framebuffer-------------*/

graphics::FramebufferTextureFormats * ContextImpl::getFramebufferTextureFormats()
{
	return m_fbTexFormats.release();
}

graphics::ObjectHandle ContextImpl::createFramebuffer()
{
	return m_createFramebuffer->createFramebuffer();
}

void ContextImpl::deleteFramebuffer(graphics::ObjectHandle _name)
{
	u32 fbo(_name);
	if (fbo != 0)
		glDeleteFramebuffers(1, &fbo);
}

void ContextImpl::bindFramebuffer(graphics::Parameter _target, graphics::ObjectHandle _name)
{
	m_cachedFunctions->geCachedBindFramebuffer()->bind(_target, _name);
}

graphics::ObjectHandle ContextImpl::createRenderbuffer()
{
	return m_createRenderbuffer->createRenderbuffer();
}

void ContextImpl::initRenderbuffer(const graphics::Context::InitRenderbufferParams & _params)
{
	m_initRenderbuffer->initRenderbuffer(_params);
}

void ContextImpl::addFrameBufferRenderTarget(const graphics::Context::FrameBufferRenderTarget & _params)
{
	m_addFramebufferRenderTarget->addFrameBufferRenderTarget(_params);
}

bool ContextImpl::blitFramebuffers(const graphics::Context::BlitFramebuffersParams & _params)
{
	return m_blitFramebuffers->blitFramebuffers(_params);
}

graphics::PixelWriteBuffer * ContextImpl::createPixelWriteBuffer(size_t _sizeInBytes)
{
	return m_createPixelWriteBuffer->createPixelWriteBuffer(_sizeInBytes);
}

/*---------------Shaders-------------*/

graphics::CombinerProgram * ContextImpl::createCombinerProgram(Combiner & _color, Combiner & _alpha, const CombinerKey & _key)
{
	return m_combinerProgramBuilder->buildCombinerProgram(_color, _alpha, _key);
}

bool ContextImpl::saveShadersStorage(const graphics::Combiners & _combiners)
{
	glsl::ShaderStorage storage(m_glInfo, m_cachedFunctions->getCachedUseProgram());
	return storage.saveShadersStorage(_combiners);
}

bool ContextImpl::loadShadersStorage(graphics::Combiners & _combiners)
{
	glsl::ShaderStorage storage(m_glInfo, m_cachedFunctions->getCachedUseProgram());
	return storage.loadShadersStorage(_combiners);
}

graphics::ShaderProgram * ContextImpl::createDepthFogShader()
{
	return m_specialShadersFactory->createShadowMapShader();
}

graphics::ShaderProgram * ContextImpl::createMonochromeShader()
{
	return m_specialShadersFactory->createMonochromeShader();
}

graphics::TexDrawerShaderProgram * ContextImpl::createTexDrawerDrawShader()
{
	return m_specialShadersFactory->createTexDrawerDrawShader();
}

graphics::ShaderProgram * ContextImpl::createTexDrawerClearShader()
{
	return m_specialShadersFactory->createTexDrawerClearShader();
}

graphics::ShaderProgram * ContextImpl::createTexrectCopyShader()
{
	return m_specialShadersFactory->createTexrectCopyShader();
}

graphics::ShaderProgram * ContextImpl::createGammaCorrectionShader()
{
	return m_specialShadersFactory->createGammaCorrectionShader();
}

graphics::ShaderProgram * ContextImpl::createOrientationCorrectionShader()
{
	return m_specialShadersFactory->createOrientationCorrectionShader();
}

void ContextImpl::resetShaderProgram()
{
	m_cachedFunctions->getCachedUseProgram()->useProgram(graphics::ObjectHandle());
}

void ContextImpl::drawTriangles(const graphics::Context::DrawTriangleParameters & _params)
{
	m_graphicsDrawer->drawTriangles(_params);
}

void ContextImpl::drawRects(const graphics::Context::DrawRectParameters & _params)
{
	m_graphicsDrawer->drawRects(_params);
}

void ContextImpl::drawLine(f32 _width, SPVertex * _vertices)
{
	m_graphicsDrawer->drawLine(_width, _vertices);
}


f32 ContextImpl::getMaxLineWidth()
{
	GLfloat lineWidthRange[2] = { 0.0f, 0.0f };
	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, lineWidthRange);
	return lineWidthRange[1];
}

void ContextImpl::drawText(const char *_pText, float _x, float _y)
{
	m_textDrawer->drawText(_pText, _x, _y);
}

void ContextImpl::getTextSize(const char *_pText, float & _w, float & _h)
{
	m_textDrawer->getTextSize(_pText, _w, _h);
}

bool ContextImpl::isError() const
{
	return Utils::isGLError();
}

bool ContextImpl::isSupported(graphics::SpecialFeatures _feature) const
{
	switch (_feature) {
	case graphics::SpecialFeatures::FragmentDepthWrite:
	case graphics::SpecialFeatures::NearPlaneClipping:
		return !m_glInfo.isGLESX;
	case graphics::SpecialFeatures::Multisampling:
		return m_glInfo.msaa;
	case graphics::SpecialFeatures::ImageTextures:
		return m_glInfo.imageTextures;
	}
	return false;
}