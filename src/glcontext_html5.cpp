/*
 * Copyright 2011-2019 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "bgfx_p.h"

#if BGFX_CONFIG_RENDERER_OPENGLES
#	include "renderer_gl.h"

#	if BGFX_USE_HTML5

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// from emscripten gl.c, because we're not going to go
// through egl
extern "C" void* emscripten_GetProcAddress(const char *name_);
extern "C" void* emscripten_webgl1_get_proc_address(const char *name_);
extern "C" void* emscripten_webgl2_get_proc_address(const char *name_);

namespace bgfx { namespace gl
{

#	define GL_IMPORT(_optional, _proto, _func, _import) _proto _func = NULL
#	include "glimports.h"

	static EmscriptenWebGLContextAttributes s_attrs;

	struct SwapChainGL
	{
		SwapChainGL(int _context, const char* _canvas)
			: m_context(_context)
		{
			m_canvas = (char*)BX_ALLOC(g_allocator, strlen(_canvas) + 1);
			strcpy(m_canvas, _canvas);

			makeCurrent();
			GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f) );
			GL_CHECK(glClear(GL_COLOR_BUFFER_BIT) );
			swapBuffers();
			GL_CHECK(glClear(GL_COLOR_BUFFER_BIT) );
			swapBuffers();
		}

		~SwapChainGL()
		{
			emscripten_webgl_destroy_context(m_context);
			BX_FREE(g_allocator, m_canvas);
		}

		void makeCurrent()
		{
			emscripten_webgl_make_context_current(m_context);
		}

		void swapBuffers()
		{
			// There is no swapBuffers equivalent.  A swap happens whenever control is returned back
			// to the browser.
		}

		int m_context;
		char* m_canvas;
	};

	void GlContext::create(uint32_t _width, uint32_t _height)
	{
		// assert?
		if (m_primary != NULL)
			return;

		const char* canvas = (const char*) g_platformData.nwh; // if 0, Module.canvas is used

		m_primary = createSwapChain((void*)canvas);

		if (_width && _height)
			emscripten_set_canvas_element_size(canvas, (int)_width, (int)_height);

		makeCurrent(m_primary);
	}

	void GlContext::destroy()
	{
		if (m_primary)
		{
			if (m_current == m_primary)
				m_current = NULL;
			BX_DELETE(g_allocator, m_primary);
			m_primary = NULL;
		}
	}

	void GlContext::resize(uint32_t _width, uint32_t _height, uint32_t /* _flags */)
    {
		if (m_primary == NULL)
			return;

		emscripten_set_canvas_element_size(m_primary->m_canvas, (int) _width, (int) _height);
    }

	SwapChainGL* GlContext::createSwapChain(void* _nwh)
	{
		emscripten_webgl_init_context_attributes(&s_attrs);

		s_attrs.alpha = false;
		s_attrs.depth = true;
		s_attrs.stencil = true;
		s_attrs.enableExtensionsByDefault = true;

        // let emscripten figure out the best WebGL context to create
		// TODO this isn't necessarily the right thing, I don't think the code actually does the fallback like it should
		s_attrs.majorVersion = 0;
		s_attrs.majorVersion = 0;

		const char* canvas = (const char*) _nwh;

		int context = emscripten_webgl_create_context(canvas, &s_attrs);

		if (context <= 0)
		{
			BX_TRACE("Failed to create WebGL context.  (Canvas handle: '%s', error %d)", canvas, (int)context);
			return NULL;
		}

		int result = emscripten_webgl_make_context_current(context);
		if (EMSCRIPTEN_RESULT_SUCCESS != result)
		{
			BX_TRACE("emscripten_webgl_make_context_current failed (%d)", result);
			return NULL;
		}

		SwapChainGL* swapChain = BX_NEW(g_allocator, SwapChainGL)(context, canvas);

		import(1);

		return swapChain;
	}

	uint64_t GlContext::getCaps() const
	{
		return BGFX_CAPS_SWAP_CHAIN;
	}

	void GlContext::destroySwapChain(SwapChainGL* _swapChain)
	{
		BX_DELETE(g_allocator, _swapChain);
	}

	void GlContext::swap(SwapChainGL* /* _swapChain */)
	{
	}

	void GlContext::makeCurrent(SwapChainGL* _swapChain)
	{
		if (m_current != _swapChain)
		{
			m_current = _swapChain;

			if (NULL == _swapChain)
			{
				if (NULL != m_primary)
				{
					m_primary->makeCurrent();
				}
			}
			else
			{
				_swapChain->makeCurrent();
			}
		}
	}

	void GlContext::import(int webGLVersion)
	{
		BX_TRACE("Import:");
#		define GL_EXTENSION(_optional, _proto, _func, _import)                                                                                                   \
			{                                                                                                                                                    \
				if (NULL == _func)                                                                                                                               \
				{                                                                                                                                                \
					_func = (_proto)emscripten_webgl1_get_proc_address(#_import);                                                                                \
					if (!_func && webGLVersion >= 2)                                                                                                             \
					    _func = (_proto)emscripten_webgl2_get_proc_address(#_import);                                                                            \
					BX_TRACE("\t%p " #_func " (" #_import ")", _func);                                                                                           \
					BGFX_FATAL(_optional || NULL != _func, Fatal::UnableToInitialize, "Failed to create WebGL/OpenGLES context. GetProcAddress(\"%s\")", #_import); \
				}                                                                                                                                                \
			}

#	include "glimports.h"
	}

} /* namespace gl */ } // namespace bgfx

#	endif // BGFX_USE_EGL
#endif // (BGFX_CONFIG_RENDERER_OPENGLES || BGFX_CONFIG_RENDERER_OPENGL)
