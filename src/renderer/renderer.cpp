#include "renderer.h"

#include "core/array.h"
#include "core/crc32.h"
#include "debug/debug.h"
#include "core/fs/file_system.h"
#include "core/json_serializer.h"
#include "core/math_utils.h"
#include "core/profiler.h"
#include "core/resource_manager.h"
#include "core/resource_manager_base.h"
#include "core/vec4.h"
#include "debug/allocator.h"
#include "editor/world_editor.h"
#include "engine.h"
#include "engine/property_descriptor.h"
#include "renderer/geometry.h"
#include "renderer/material.h"
#include "renderer/material_manager.h"
#include "renderer/model.h"
#include "renderer/model_instance.h"
#include "renderer/model_manager.h"
#include "renderer/pipeline.h"
#include "renderer/render_scene.h"
#include "renderer/shader.h"
#include "renderer/shader_manager.h"
#include "renderer/texture.h"
#include "renderer/texture_manager.h"
#include "universe/universe.h"
#include <bgfx.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>


namespace bgfx
{


struct PlatformData
{
	void* ndt;			//< Native display type
	void* nwh;			//< Native window handle
	void* context;		//< GL context, or D3D device
	void* backBuffer;   //< GL backbuffer, or D3D render target view
	void* backBufferDS; //< Backbuffer depth/stencil.
};


void setPlatformData(const PlatformData& _pd);
}


namespace Lumix
{


static const uint32_t GLOBAL_LIGHT_HASH = crc32("global_light");
static const uint32_t POINT_LIGHT_HASH = crc32("point_light");
static const uint32_t RENDERABLE_HASH = crc32("renderable");
static const uint32_t CAMERA_HASH = crc32("camera");


struct RendererImpl : public Renderer
{
	struct CallbackStub : public bgfx::CallbackI
	{
		virtual void fatal(bgfx::Fatal::Enum _code, const char* _str) override
		{
			Lumix::g_log_error.log("bgfx") << _str;
			if (bgfx::Fatal::DebugCheck == _code)
			{
				Lumix::Debug::debugBreak();
			}
			else
			{
				abort();
			}
		}


		virtual void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
		{
			char tmp[2048];
			vsnprintf(tmp, sizeof(tmp), _format, _argList);
			Lumix::g_log_info.log("bgfx") << _filePath << "(" << _line << ") " << tmp;
		}


		virtual void screenShot(const char*,
								uint32_t,
								uint32_t,
								uint32_t,
								const void*,
								uint32_t,
								bool) override
		{
			ASSERT(false);
		}


		virtual void captureBegin(uint32_t,
								  uint32_t,
								  uint32_t,
								  bgfx::TextureFormat::Enum,
								  bool) override
		{
			ASSERT(false);
		}


		virtual uint32_t cacheReadSize(uint64_t) override { return 0; }
		virtual bool cacheRead(uint64_t, void*, uint32_t) override
		{
			return false;
		}
		virtual void cacheWrite(uint64_t, const void*, uint32_t) override {}
		virtual void captureEnd() override { ASSERT(false); }
		virtual void captureFrame(const void*, uint32_t) override
		{
			ASSERT(false);
		}
	};


	RendererImpl(Engine& engine)
		: m_engine(engine)
		, m_allocator(engine.getAllocator())
		, m_texture_manager(m_allocator)
		, m_model_manager(m_allocator, *this)
		, m_material_manager(m_allocator)
		, m_shader_manager(*this, m_allocator)
		, m_pipeline_manager(*this, m_allocator)
		, m_passes(m_allocator)
	{
		bgfx::PlatformData d;
		if (s_hwnd)
		{
			memset(&d, 0, sizeof(d));
			d.nwh = s_hwnd;
			bgfx::setPlatformData(d);
		}
		bgfx::init(bgfx::RendererType::Count, 0, 0, &m_callback_stub);
		bgfx::reset(800, 600);
		bgfx::setDebug(BGFX_DEBUG_TEXT);

		ResourceManager& manager = engine.getResourceManager();
		m_texture_manager.create(ResourceManager::TEXTURE, manager);
		m_model_manager.create(ResourceManager::MODEL, manager);
		m_material_manager.create(ResourceManager::MATERIAL, manager);
		m_shader_manager.create(ResourceManager::SHADER, manager);
		m_pipeline_manager.create(ResourceManager::PIPELINE, manager);

		m_current_pass_hash = crc32("MAIN");
		m_view_counter = 0;

		registerPropertyDescriptors();
	}

	~RendererImpl()
	{
		m_texture_manager.destroy();
		m_model_manager.destroy();
		m_material_manager.destroy();
		m_shader_manager.destroy();
		m_pipeline_manager.destroy();

		bgfx::frame();
		bgfx::frame();
		bgfx::shutdown();
	}


	virtual IScene* createScene(UniverseContext& ctx) override
	{
		return RenderScene::createInstance(
			*this, m_engine, *ctx.m_universe, true, m_allocator);
	}


	virtual void destroyScene(IScene* scene) override
	{
		RenderScene::destroyInstance(static_cast<RenderScene*>(scene));
	}


	void registerPropertyDescriptors()
	{
		IAllocator& allocator = m_engine.getAllocator();

		m_engine.registerComponentType("camera", "Camera");
		m_engine.registerComponentType("global_light", "Global light");
		m_engine.registerComponentType("renderable", "Mesh");
		m_engine.registerComponentType("point_light", "Point light");
		m_engine.registerComponentType("terrain", "Terrain");

		m_engine.registerProperty(
			"camera",
			allocator.newObject<StringPropertyDescriptor<RenderScene>>(
				"slot",
				&RenderScene::getCameraSlot,
				&RenderScene::setCameraSlot,
				allocator));
		m_engine.registerProperty(
			"camera",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"fov",
				&RenderScene::getCameraFOV,
				&RenderScene::setCameraFOV,
				0.0f,
				360.0f,
				1.0f,
				allocator));
		m_engine.registerProperty(
			"camera",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"near",
				&RenderScene::getCameraNearPlane,
				&RenderScene::setCameraNearPlane,
				0.0f,
				FLT_MAX,
				0.0f,
				allocator));
		m_engine.registerProperty(
			"camera",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"far",
				&RenderScene::getCameraFarPlane,
				&RenderScene::setCameraFarPlane,
				0.0f,
				FLT_MAX,
				0.0f,
				allocator));

		m_engine.registerProperty(
			"renderable",
			allocator.newObject<ResourcePropertyDescriptor<RenderScene>>(
				"source",
				&RenderScene::getRenderablePath,
				&RenderScene::setRenderablePath,
				"Mesh (*.msh)",
				allocator));
		m_engine.registerProperty(
			"renderable",
			allocator.newObject<BoolPropertyDescriptor<RenderScene>>(
				"is_always_visible",
				&RenderScene::isRenderableAlwaysVisible,
				&RenderScene::setRenderableIsAlwaysVisible,
				allocator));

		m_engine.registerProperty(
			"global_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"ambient_intensity",
				&RenderScene::getLightAmbientIntensity,
				&RenderScene::setLightAmbientIntensity,
				0.0f,
				1.0f,
				0.05f,
				allocator));
		m_engine.registerProperty(
			"global_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"intensity",
				&RenderScene::getGlobalLightIntensity,
				&RenderScene::setGlobalLightIntensity,
				0.0f,
				1.0f,
				0.05f,
				allocator));
		m_engine.registerProperty(
			"global_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"fog_density",
				&RenderScene::getFogDensity,
				&RenderScene::setFogDensity,
				0.0f,
				1.0f,
				0.01f,
				allocator));
		m_engine.registerProperty(
			"global_light",
			allocator.newObject<ColorPropertyDescriptor<RenderScene>>(
				"ambient_color",
				&RenderScene::getLightAmbientColor,
				&RenderScene::setLightAmbientColor,
				allocator));
		m_engine.registerProperty(
			"global_light",
			allocator.newObject<ColorPropertyDescriptor<RenderScene>>(
				"color",
				&RenderScene::getGlobalLightColor,
				&RenderScene::setGlobalLightColor,
				allocator));
		m_engine.registerProperty(
			"global_light",
			allocator.newObject<ColorPropertyDescriptor<RenderScene>>(
				"fog_color",
				&RenderScene::getFogColor,
				&RenderScene::setFogColor,
				allocator));

		m_engine.registerProperty(
			"point_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"intensity",
				&RenderScene::getPointLightIntensity,
				&RenderScene::setPointLightIntensity,
				0.0f,
				1.0f,
				0.05f,
				allocator));
		m_engine.registerProperty(
			"point_light",
			allocator.newObject<ColorPropertyDescriptor<RenderScene>>(
				"color",
				&RenderScene::getPointLightColor,
				&RenderScene::setPointLightColor,
				allocator));
		m_engine.registerProperty(
			"point_light",
			allocator.newObject<ColorPropertyDescriptor<RenderScene>>(
				"specular",
				&RenderScene::getPointLightSpecularColor,
				&RenderScene::setPointLightSpecularColor,
				allocator));
		m_engine.registerProperty(
			"point_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"range",
				&RenderScene::getLightRange,
				&RenderScene::setLightRange,
				0.0f,
				FLT_MAX,
				0.0f,
				allocator));
		m_engine.registerProperty(
			"point_light",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"FOV",
				&RenderScene::getLightFOV,
				&RenderScene::setLightFOV,
				0.0f,
				360.0f,
				5.0f,
				allocator));

		m_engine.registerProperty(
			"terrain",
			allocator.newObject<ResourcePropertyDescriptor<RenderScene>>(
				"material",
				&RenderScene::getTerrainMaterialPath,
				&RenderScene::setTerrainMaterialPath,
				"Material (*.mat)",
				allocator));
		m_engine.registerProperty(
			"terrain",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"xz_scale",
				&RenderScene::getTerrainXZScale,
				&RenderScene::setTerrainXZScale,
				0.0f,
				FLT_MAX,
				0.0f,
				allocator));
		m_engine.registerProperty(
			"terrain",
			allocator.newObject<DecimalPropertyDescriptor<RenderScene>>(
				"y_scale",
				&RenderScene::getTerrainYScale,
				&RenderScene::setTerrainYScale,
				0.0f,
				FLT_MAX,
				0.0f,
				allocator));

		auto grass = allocator.newObject<ArrayDescriptor<RenderScene>>(
			"grass",
			&RenderScene::getGrassCount,
			&RenderScene::addGrass,
			&RenderScene::removeGrass,
			allocator);
		grass->addChild(
			allocator.newObject<ResourceArrayObjectDescriptor<RenderScene>>(
				"mesh",
				&RenderScene::getGrassPath,
				&RenderScene::setGrassPath,
				"Mesh (*.msh)",
				allocator));
		auto ground =
			allocator.newObject<IntArrayObjectDescriptor<RenderScene>>(
				"ground",
				&RenderScene::getGrassGround,
				&RenderScene::setGrassGround,
				allocator);
		ground->setLimit(0, 4);
		grass->addChild(ground);
		grass->addChild(
			allocator.newObject<IntArrayObjectDescriptor<RenderScene>>(
				"density",
				&RenderScene::getGrassDensity,
				&RenderScene::setGrassDensity,
				allocator));
		m_engine.registerProperty("terrain", grass);
	}


	virtual bool create() override { return true; }


	virtual void destroy() override {}


	virtual const char* getName() const override { return "renderer"; }


	virtual Engine& getEngine() override { return m_engine; }


	virtual int getPassIdx(const char* pass) override
	{
		for (int i = 0; i < m_passes.size(); ++i)
		{
			if (strcmp(m_passes[i], pass) == 0)
			{
				return i;
			}
		}

		auto& new_pass = m_passes.pushEmpty();
		copyString(new_pass, sizeof(new_pass), pass);
		return m_passes.size() - 1;
	}


	virtual void makeScreenshot(const Path& filename) override
	{
		bgfx::saveScreenShot(filename.c_str());
	}


	virtual void frame() override
	{
		bgfx::frame();
		m_view_counter = 0;
	}


	virtual int getViewCounter() const override
	{
		return m_view_counter;
	}


	virtual void viewCounterAdd() override
	{
		++m_view_counter;
	}


	Engine& m_engine;
	Debug::Allocator m_allocator;
	Lumix::Array<ShaderCombinations::Pass> m_passes;
	CallbackStub m_callback_stub;
	TextureManager m_texture_manager;
	MaterialManager m_material_manager;
	ShaderManager m_shader_manager;
	ModelManager m_model_manager;
	PipelineManager m_pipeline_manager;
	uint32_t m_current_pass_hash;
	int m_view_counter;

	static HWND s_hwnd;
};


HWND RendererImpl::s_hwnd = nullptr;


void Renderer::setInitData(void* data)
{
	RendererImpl::s_hwnd = (HWND)data;
}


extern "C"
{
	LUMIX_RENDERER_API IPlugin* createPlugin(Engine& engine)
	{
		RendererImpl* r = engine.getAllocator().newObject<RendererImpl>(engine);
		if (r->create())
		{
			return r;
		}
		engine.getAllocator().deleteObject(r);
		return nullptr;
	}
}


} // ~namespace Lumix



