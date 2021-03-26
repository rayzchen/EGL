/**
 * EGL windows desktop implementation.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) since 2014 Norbert Nopper
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "egl_internal.h"
#include "../../EGL/include/EGL/eglctxinternals.h"
#include <iostream>

#if defined(EGL_NO_GLEW)
typedef GLXContext (*__PFN_glXCreateContextAttribsARB)(Display*, GLXFBConfig,
                                                       GLXContext, Bool,
                                                       const int*);
typedef void (*__PFN_glXSwapIntervalEXT)(Display*, GLXDrawable, int);
typedef void(*__PFN_glFinish)();

__PFN_glXCreateContextAttribsARB glXCreateContextAttribsARB_PTR = NULL;
__PFN_glXSwapIntervalEXT glXSwapIntervalEXT_PTR = NULL;
__PFN_glFinish glFinish_PTR = NULL;

#define glXSwapIntervalEXT(...) glXSwapIntervalEXT_PTR(__VA_ARGS__)
#define glXCreateContextAttribsARB(...) \
    glXCreateContextAttribsARB_PTR(__VA_ARGS__)
#endif 

__eglMustCastToProperFunctionPointerType __getProcAddress(const char *procname)
{
	return (__eglMustCastToProperFunctionPointerType )glXGetProcAddress((const GLubyte *)procname);
}

EGLBoolean __internalInit(NativeLocalStorageContainer* nativeLocalStorageContainer)
{
	if (nativeLocalStorageContainer->display && nativeLocalStorageContainer->window && nativeLocalStorageContainer->ctx)
	{
		return EGL_TRUE;
	}

	if (nativeLocalStorageContainer->display)
	{
		return EGL_FALSE;
	}

	if (nativeLocalStorageContainer->window)
	{
		return EGL_FALSE;
	}

	if (nativeLocalStorageContainer->ctx)
	{
		return EGL_FALSE;
	}

	nativeLocalStorageContainer->display = XOpenDisplay(NULL);

	if (!nativeLocalStorageContainer->display)
	{
		return EGL_FALSE;
	}

	//

	int glxMajor;
	int glxMinor;

	// GLX version 1.4 or higher needed.
	if (!glXQueryVersion(nativeLocalStorageContainer->display, &glxMajor, &glxMinor))
	{
		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}

	if (glxMajor < 1 || (glxMajor == 1 && glxMinor < 4))
	{
		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}

	//

	nativeLocalStorageContainer->window = DefaultRootWindow(nativeLocalStorageContainer->display);

	if (!nativeLocalStorageContainer->window)
	{
		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}

	int dummyAttribList[] = {
		GLX_USE_GL, True,
		GLX_DOUBLEBUFFER, True,
		GLX_RGBA, True,
		None
	};

	XVisualInfo* visualInfo = glXChooseVisual(nativeLocalStorageContainer->display, 0, dummyAttribList);

	if (!visualInfo)
	{
		nativeLocalStorageContainer->window = 0;

		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}
  
	nativeLocalStorageContainer->ctx = glXCreateContext(nativeLocalStorageContainer->display, visualInfo, NULL, True);

	if (!nativeLocalStorageContainer->ctx)
	{
		nativeLocalStorageContainer->window = 0;

		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}

	if (!glXMakeCurrent(nativeLocalStorageContainer->display, nativeLocalStorageContainer->window, nativeLocalStorageContainer->ctx))
	{
		glXDestroyContext(nativeLocalStorageContainer->display, nativeLocalStorageContainer->ctx);
		nativeLocalStorageContainer->ctx = 0;

		nativeLocalStorageContainer->window = 0;

		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}

#if !defined(EGL_NO_GLEW)
	glewExperimental = GL_TRUE;
	if (glewInit() != GL_NO_ERROR)
	{
		glXMakeCurrent(nativeLocalStorageContainer->display, 0, 0);

		glXDestroyContext(nativeLocalStorageContainer->display, nativeLocalStorageContainer->ctx);
		nativeLocalStorageContainer->ctx = 0;

		nativeLocalStorageContainer->window = 0;

		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;

		return EGL_FALSE;
	}
#else
  glXCreateContextAttribsARB_PTR =
    (__PFN_glXCreateContextAttribsARB)
        __getProcAddress("glXCreateContextAttribsARB");
  glXSwapIntervalEXT_PTR =
    (__PFN_glXSwapIntervalEXT)__getProcAddress("glXSwapIntervalEXT");
  glFinish_PTR = (__PFN_glFinish)__getProcAddress("glFinish");
#endif
	return EGL_TRUE;
}

EGLBoolean __internalTerminate(NativeLocalStorageContainer* nativeLocalStorageContainer)
{
	if (!nativeLocalStorageContainer)
	{
		return EGL_FALSE;
	}

	if (nativeLocalStorageContainer->display)
	{
		glXMakeContextCurrent(nativeLocalStorageContainer->display, 0, 0, 0);
	}

	if (nativeLocalStorageContainer->display && nativeLocalStorageContainer->ctx)
	{
		glXDestroyContext(nativeLocalStorageContainer->display, nativeLocalStorageContainer->ctx);
		nativeLocalStorageContainer->ctx = 0;
	}

	if (nativeLocalStorageContainer->display && nativeLocalStorageContainer->window)
	{
		XDestroyWindow(nativeLocalStorageContainer->display, nativeLocalStorageContainer->window);
		nativeLocalStorageContainer->window = 0;
	}

	if (nativeLocalStorageContainer->display)
	{
		XCloseDisplay(nativeLocalStorageContainer->display);
		nativeLocalStorageContainer->display = 0;
	}

	return EGL_TRUE;
}

EGLBoolean __deleteContext(const EGLDisplayImpl* walkerDpy, const NativeContextContainer* nativeContextContainer)
{
	if (!walkerDpy || !nativeContextContainer)
	{
		return EGL_FALSE;
	}

	glXDestroyContext(walkerDpy->display_id, nativeContextContainer->ctx);

	return EGL_TRUE;
}

EGLBoolean __processAttribList(EGLint* target_attrib_list, const EGLint* attrib_list, EGLint* error)
{
	if (!target_attrib_list || !attrib_list || !error)
	{
		return EGL_FALSE;
	}

	EGLint template_attrib_list[] = {
			GLX_CONTEXT_MAJOR_VERSION_ARB, 1,
			GLX_CONTEXT_MINOR_VERSION_ARB, 0,
			GLX_CONTEXT_FLAGS_ARB, 0,
			GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
			GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, GLX_NO_RESET_NOTIFICATION_ARB,
			0
	};

	EGLint attribListIndex = 0;

	while (attrib_list[attribListIndex] != EGL_NONE)
	{
		EGLint value = attrib_list[attribListIndex + 1];

		switch (attrib_list[attribListIndex])
		{
			case EGL_CONTEXT_MAJOR_VERSION:
			{
				if (value < 1)
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}

				template_attrib_list[1] = value;
			}
			break;
			case EGL_CONTEXT_MINOR_VERSION:
			{
				if (value < 0)
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}

				template_attrib_list[3] = value;
			}
			break;
			case EGL_CONTEXT_OPENGL_PROFILE_MASK:
			{
				if (value == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT)
				{
					template_attrib_list[7] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
				}
				else if (value == EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT)
				{
					template_attrib_list[7] = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
				}
				else
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}
			}
			break;
			case EGL_CONTEXT_OPENGL_DEBUG:
			{
				if (value == EGL_TRUE)
				{
					template_attrib_list[5] |= GLX_CONTEXT_DEBUG_BIT_ARB;
				}
				else if (value == EGL_FALSE)
				{
					template_attrib_list[5] &= ~GLX_CONTEXT_DEBUG_BIT_ARB;
				}
				else
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}
			}
			break;
			case EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE:
			{
				if (value == EGL_TRUE)
				{
					template_attrib_list[5] |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
				}
				else if (value == EGL_FALSE)
				{
					template_attrib_list[5] &= ~GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
				}
				else
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}
			}
			break;
			case EGL_CONTEXT_OPENGL_ROBUST_ACCESS:
			{
				if (value == EGL_TRUE)
				{
					template_attrib_list[5] |= GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;
				}
				else if (value == EGL_FALSE)
				{
					template_attrib_list[5] &= ~GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;
				}
				else
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}
			}
			break;
			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY:
			{
				if (value == EGL_NO_RESET_NOTIFICATION)
				{
					template_attrib_list[9] = GLX_NO_RESET_NOTIFICATION_ARB;
				}
				else if (value == EGL_LOSE_CONTEXT_ON_RESET)
				{
					template_attrib_list[9] = GLX_LOSE_CONTEXT_ON_RESET_ARB;
				}
				else
				{
					*error = EGL_BAD_ATTRIBUTE;

					return EGL_FALSE;
				}
			}
			break;
			default:
			{
				*error = EGL_BAD_ATTRIBUTE;

				return EGL_FALSE;
			}
			break;
		}

		attribListIndex += 2;

		// More than 14 entries can not exist.
		if (attribListIndex >= 7 * 2)
		{
			*error = EGL_BAD_ATTRIBUTE;

			return EGL_FALSE;
		}
	}

	memcpy(target_attrib_list, template_attrib_list, CONTEXT_ATTRIB_LIST_SIZE * sizeof(EGLint));

	return EGL_TRUE;
}

EGLBoolean __createPbufferSurface(EGLSurfaceImpl* newSurface, const EGLint* attrib_list, const EGLDisplayImpl* walkerDpy, const EGLConfigImpl* walkerConfig, EGLint* error)
{
	if (!newSurface || !walkerDpy || !walkerConfig || !error)
	{
		return EGL_FALSE;
	}

	EGLNativeDisplayType display = walkerDpy->display_id;

	int glxattribs[] =
	{
		GLX_PBUFFER_WIDTH, 0,
		GLX_PBUFFER_HEIGHT, 0,
		GLX_LARGEST_PBUFFER, False,

		None
	};
	int* width = glxattribs + 1;
	int* height = glxattribs + 3;
	int* largest_pbuffer = glxattribs + 5;
	EGLBoolean colorspace_srgb = 0;

	EGLint currAttrib = 0;
	while (attrib_list[currAttrib] != EGL_NONE)
	{
		EGLint attrib = attrib_list[currAttrib];
		EGLint value = attrib_list[currAttrib + 1];

		switch (attrib)
		{
		case EGL_WIDTH:
			*width = value; break;
		case EGL_HEIGHT:
			*height = value; break;
		case EGL_LARGEST_PBUFFER:
			*largest_pbuffer = value; break;
		case EGL_GL_COLORSPACE:
			colorspace_srgb = 1; break;
		}

		currAttrib += 2;
	}


	EGLint numberPixelFormats;

	GLXFBConfig* fbConfigs = glXGetFBConfigs(walkerDpy->display_id, DefaultScreen(walkerDpy->display_id), &numberPixelFormats);

	if (!fbConfigs || numberPixelFormats == 0)
	{
		if (fbConfigs)
		{
			XFree(fbConfigs);
		}

		return EGL_FALSE;
	}

	EGLint attribute;

	XVisualInfo* visualInfo;

	GLXFBConfig config = 0;

	for (EGLint currentPixelFormat = 0; currentPixelFormat < numberPixelFormats; currentPixelFormat++)
	{
		EGLint value;

		attribute = GLX_VISUAL_ID;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (!value)
		{
			continue;
		}

		// No check for OpenGL.

		attribute = GLX_RENDER_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (!(value & GLX_RGBA_BIT)) // TODO why?
		{
			continue;
		}

		attribute = GLX_TRANSPARENT_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (value == GLX_TRANSPARENT_INDEX)
		{
			continue;
		}

		attribute = GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (colorspace_srgb && value == False)
		{
			continue;
		}

		//

		visualInfo = glXGetVisualFromFBConfig(walkerDpy->display_id, fbConfigs[currentPixelFormat]);

		if (!visualInfo)
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}

		if (walkerConfig->nativeVisualId == visualInfo->visualid)
		{
			config = fbConfigs[currentPixelFormat];

			XFree(visualInfo);

			break;
		}

		XFree(visualInfo);
	}

	XFree(fbConfigs);

	if (!config)
		return EGL_FALSE;

	newSurface->drawToWindow = EGL_FALSE;
	newSurface->drawToPixmap = EGL_FALSE;
	newSurface->drawToPBuffer = EGL_TRUE;
	newSurface->doubleBuffer = EGL_FALSE;
	newSurface->configId = walkerConfig->configId;

	newSurface->initialized = EGL_TRUE;
	newSurface->destroy = EGL_FALSE;
	newSurface->pbuf = glXCreatePbuffer(display, config, glxattribs);
	newSurface->nativeSurfaceContainer.config = config;
	newSurface->nativeSurfaceContainer.drawable = 0; // TODO (this function doesnt really have access to GLXDrawable)

	return EGL_TRUE;
}

EGLBoolean __createWindowSurface(EGLSurfaceImpl* newSurface, EGLNativeWindowType win, const EGLint *attrib_list, const EGLDisplayImpl* walkerDpy, const EGLConfigImpl* walkerConfig, EGLint* error)
{
	if (!newSurface || !walkerDpy || !walkerConfig || !error)
	{
		return EGL_FALSE;
	}

	EGLBoolean colorspace_srgb = 0;
	EGLBoolean doublebuffer = 0;
	if (attrib_list)
	{
		EGLint indexAttribList = 0;

		while (attrib_list[indexAttribList] != EGL_NONE)
		{
			EGLint value = attrib_list[indexAttribList + 1];

			switch (attrib_list[indexAttribList])
			{
				case EGL_GL_COLORSPACE:
				{
					if (value == EGL_GL_COLORSPACE_LINEAR)
					{
						colorspace_srgb = 0;
					}
					else if (value == EGL_GL_COLORSPACE_SRGB)
					{
						colorspace_srgb = 1;
					}
					else
					{
						*error = EGL_BAD_ATTRIBUTE;

						return EGL_FALSE;
					}
				}
				break;
				case EGL_RENDER_BUFFER:
				{
					if (value == EGL_SINGLE_BUFFER)
					{
						doublebuffer = 0;
						if (walkerConfig->doubleBuffer)
						{
							*error = EGL_BAD_MATCH;

							return EGL_FALSE;
						}
					}
					else if (value == EGL_BACK_BUFFER)
					{
						doublebuffer = 1;
						if (!walkerConfig->doubleBuffer)
						{
							*error = EGL_BAD_MATCH;

							return EGL_FALSE;
						}
					}
					else
					{
						*error = EGL_BAD_ATTRIBUTE;

						return EGL_FALSE;
					}
				}
				break;
				case EGL_VG_ALPHA_FORMAT:
				{
					*error = EGL_BAD_MATCH;

					return EGL_FALSE;
				}
				break;
				case EGL_VG_COLORSPACE:
				{
					*error = EGL_BAD_MATCH;

					return EGL_FALSE;
				}
				break;
			}

			indexAttribList += 2;

			// More than 4 entries can not exist.
			if (indexAttribList >= 4 * 2)
			{
				*error = EGL_BAD_ATTRIBUTE;

				return EGL_FALSE;
			}
		}
	}

	//

	EGLint numberPixelFormats;

	GLXFBConfig* fbConfigs = glXGetFBConfigs(walkerDpy->display_id, DefaultScreen(walkerDpy->display_id), &numberPixelFormats);

	if (!fbConfigs || numberPixelFormats == 0)
	{
		if (fbConfigs)
		{
			XFree(fbConfigs);
		}

		return EGL_FALSE;
	}

	EGLint attribute;

	XVisualInfo* visualInfo;

	GLXFBConfig config = 0;

	for (EGLint currentPixelFormat = 0; currentPixelFormat < numberPixelFormats; currentPixelFormat++)
	{
		EGLint value;

		attribute = GLX_VISUAL_ID;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (!value)
		{
			continue;
		}

		// No check for OpenGL.

		attribute = GLX_RENDER_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (!(value & GLX_RGBA_BIT)) // TODO why?
		{
			continue;
		}

		attribute = GLX_TRANSPARENT_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (value == GLX_TRANSPARENT_INDEX)
		{
			continue;
		}

		attribute = GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (colorspace_srgb && value == False)
		{
			continue;
		}

		attribute = GLX_DOUBLEBUFFER;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}
		if (doublebuffer && value == False)
		{
			continue;
		}

		//

		visualInfo = glXGetVisualFromFBConfig(walkerDpy->display_id, fbConfigs[currentPixelFormat]);

		if (!visualInfo)
		{
			XFree(fbConfigs);

			return EGL_FALSE;
		}

		if (walkerConfig->nativeVisualId == visualInfo->visualid)
		{
			config = fbConfigs[currentPixelFormat];

			XFree(visualInfo);

			break;
		}

		XFree(visualInfo);
	}

	XFree(fbConfigs);

	if (!config)
	{
		return EGL_FALSE;
	}

	newSurface->drawToWindow = EGL_TRUE;
	newSurface->drawToPixmap = EGL_FALSE;
	newSurface->drawToPBuffer = EGL_FALSE;
	newSurface->doubleBuffer = walkerConfig->doubleBuffer;
	newSurface->configId = walkerConfig->configId;

	newSurface->initialized = EGL_TRUE;
	newSurface->destroy = EGL_FALSE;
	newSurface->win = win;
	newSurface->nativeSurfaceContainer.config = config;
	newSurface->nativeSurfaceContainer.drawable = win;

	return EGL_TRUE;
}

EGLBoolean __destroySurface(EGLNativeDisplayType dpy, const EGLSurfaceImpl* surface)
{
	if (!surface)
	{
		return EGL_FALSE;
	}

	if (surface->drawToPBuffer)
	{
		glXDestroyPbuffer(dpy, surface->pbuf);
	}
	// else Nothing to release.

	return EGL_TRUE;
}

EGLBoolean __initialize(EGLDisplayImpl* walkerDpy, const NativeLocalStorageContainer* nativeLocalStorageContainer, EGLint* error)
{
	if (!walkerDpy || !nativeLocalStorageContainer || !error)
	{
		return EGL_FALSE;
	}

	const char* extensions_str = glXQueryExtensionsString(walkerDpy->display_id, DefaultScreen(walkerDpy->display_id));
	int ES_supported = strstr(extensions_str, "GLX_EXT_create_context_es_profile") != NULL;
	const EGLint ES_mask = ES_supported * (EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT);

	// Create configuration list.

	EGLint numberPixelFormats;

	GLXFBConfig* fbConfigs = glXGetFBConfigs(walkerDpy->display_id, DefaultScreen(walkerDpy->display_id), &numberPixelFormats);

	if (!fbConfigs || numberPixelFormats == 0)
	{
		if (fbConfigs)
		{
			XFree(fbConfigs);
		}

		*error = EGL_NOT_INITIALIZED;

		return EGL_FALSE;
	}

	EGLint attribute;

	XVisualInfo* visualInfo;

	EGLConfigImpl* lastConfig = 0;
	for (EGLint currentPixelFormat = 0; currentPixelFormat < numberPixelFormats; currentPixelFormat++)
	{
		EGLint value;

		attribute = GLX_VISUAL_ID;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		if (!value)
		{
			continue;
		}

		// No check for OpenGL.

		attribute = GLX_RENDER_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		if (!(value & GLX_RGBA_BIT))
		{
			continue;
		}

		attribute = GLX_TRANSPARENT_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		if (value == GLX_TRANSPARENT_INDEX)
		{
			continue;
		}

		//

		EGLConfigImpl* newConfig = (EGLConfigImpl*)malloc(sizeof(EGLConfigImpl));
		if (!newConfig)
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		_eglInternalSetDefaultConfig(newConfig);

		// Store in the same order as received.
		newConfig->next = 0;
		if (lastConfig != 0)
		{
			lastConfig->next = newConfig;
		}
		else
		{
			walkerDpy->rootConfig = newConfig;
		}
		lastConfig = newConfig;

		//

		attribute = GLX_DRAWABLE_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &value))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		newConfig->drawToWindow = value & GLX_WINDOW_BIT ? EGL_TRUE : EGL_FALSE;
		newConfig->drawToPixmap = value & GLX_PIXMAP_BIT ? EGL_TRUE : EGL_FALSE;
		newConfig->drawToPBuffer = value & GLX_PBUFFER_BIT ? EGL_TRUE : EGL_FALSE;

		attribute = GLX_DOUBLEBUFFER;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->doubleBuffer))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		//
		//TODO check for `GLX_EXT_create_context_es_profile` extension
		newConfig->conformant = (EGL_OPENGL_BIT | ES_mask);
		newConfig->renderableType = (EGL_OPENGL_BIT | ES_mask);
		newConfig->surfaceType = 0;
		if (newConfig->drawToWindow)
		{
			newConfig->surfaceType |= EGL_WINDOW_BIT;
		}
		if (newConfig->drawToPixmap)
		{
			newConfig->surfaceType |= EGL_PIXMAP_BIT;
		}
		if (newConfig->drawToPBuffer)
		{
			newConfig->surfaceType |= EGL_PBUFFER_BIT;
		}
		newConfig->colorBufferType = EGL_RGB_BUFFER;
		newConfig->configId = currentPixelFormat;

		attribute = GLX_BUFFER_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->bufferSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_RED_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->redSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_GREEN_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->greenSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_BLUE_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->blueSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_ALPHA_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->alphaSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_DEPTH_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->depthSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_STENCIL_SIZE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->stencilSize))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}


		//

		attribute = GLX_SAMPLE_BUFFERS;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->sampleBuffers))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_SAMPLES;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->samples))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		//

		attribute = GLX_BIND_TO_TEXTURE_RGB_EXT;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->bindToTextureRGB))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		newConfig->bindToTextureRGB = newConfig->bindToTextureRGB ? EGL_TRUE : EGL_FALSE;

		attribute = GLX_BIND_TO_TEXTURE_RGBA_EXT;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->bindToTextureRGBA))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		newConfig->bindToTextureRGBA = newConfig->bindToTextureRGBA ? EGL_TRUE : EGL_FALSE;

		//

		attribute = GLX_MAX_PBUFFER_PIXELS;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->maxPBufferPixels))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_MAX_PBUFFER_WIDTH;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->maxPBufferWidth))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_MAX_PBUFFER_HEIGHT;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->maxPBufferHeight))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		//

		attribute = GLX_TRANSPARENT_TYPE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->transparentType))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}
		newConfig->transparentType = newConfig->transparentType == GLX_TRANSPARENT_RGB ? EGL_TRANSPARENT_RGB : EGL_NONE;

		attribute = GLX_TRANSPARENT_RED_VALUE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->transparentRedValue))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_TRANSPARENT_GREEN_VALUE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->transparentGreenValue))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		attribute = GLX_TRANSPARENT_BLUE_VALUE;
		if (glXGetFBConfigAttrib(walkerDpy->display_id, fbConfigs[currentPixelFormat], attribute, &newConfig->transparentBlueValue))
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		//

		visualInfo = glXGetVisualFromFBConfig(walkerDpy->display_id, fbConfigs[currentPixelFormat]);

		if (!visualInfo)
		{
			XFree(fbConfigs);

			*error = EGL_NOT_INITIALIZED;

			return EGL_FALSE;
		}

		newConfig->nativeVisualId = visualInfo->visualid;

		XFree(visualInfo);

		newConfig->matchNativePixmap = EGL_NONE;
		newConfig->nativeRenderable = EGL_DONT_CARE; // ???

		// FIXME: Query and save more values.
	}

	XFree(fbConfigs);

	return EGL_TRUE;
}

static int xerrorhandler(Display *dsp, XErrorEvent *error)
{
  char errorstring[1024];
  XGetErrorText(dsp, error->error_code, errorstring, 1024);
    
  std::cerr << "ack!fatal: X error--" << errorstring << std::endl;
  exit(-1);
}
EGLBoolean __createContext(NativeContextContainer* nativeContextContainer, const EGLDisplayImpl* walkerDpy, const NativeSurfaceContainer* nativeSurfaceContainer, const NativeContextContainer* sharedNativeContextContainer, const EGLint* attribList)
{
	if (!nativeContextContainer || !walkerDpy || !nativeSurfaceContainer)
	{
		return EGL_FALSE;
	}
	XSetErrorHandler(xerrorhandler);
	nativeContextContainer->ctx = glXCreateContextAttribsARB(walkerDpy->display_id, nativeSurfaceContainer->config, sharedNativeContextContainer ? sharedNativeContextContainer->ctx : 0, True, attribList);

	return nativeContextContainer->ctx != 0;
}

EGLBoolean __makeCurrent(const EGLDisplayImpl* walkerDpy, const NativeSurfaceContainer* nativeSurfaceContainer, const NativeContextContainer* nativeContextContainer)
{
	if (!walkerDpy || (!nativeSurfaceContainer && nativeContextContainer) || (nativeSurfaceContainer && !nativeContextContainer))
	{
		return EGL_FALSE;
	}

	if (!nativeContextContainer)
		return (EGLBoolean)glXMakeCurrent(walkerDpy->display_id, None, NULL);

	return (EGLBoolean)glXMakeCurrent(walkerDpy->display_id, nativeSurfaceContainer->drawable, nativeContextContainer->ctx);
}

EGLBoolean __swapBuffers(const EGLDisplayImpl* walkerDpy, const EGLSurfaceImpl* walkerSurface)
{
	if (!walkerDpy || !walkerSurface)
	{
		return EGL_FALSE;
	}

	glXSwapBuffers(walkerDpy->display_id, walkerSurface->win);

	return EGL_TRUE;
}

EGLBoolean __swapInterval(const EGLDisplayImpl* walkerDpy, EGLint interval)
{
	if (!walkerDpy)
	{
		return EGL_FALSE;
	}

	glXSwapIntervalEXT(walkerDpy->display_id, walkerDpy->currentDraw->win, interval);

	return EGL_TRUE;
}

/*
EGLBoolean __getPlatformDependentHandles(void* _out, const EGLDisplayImpl* walkerDpy, const NativeSurfaceContainer* nativeSurfaceContainer, const NativeContextContainer* nativeContextContainer)
{
	if (!nativeSurfaceContainer || !nativeContextContainer)
		return EGL_FALSE;

	EGLContextInternals* out = (EGLContextInternals*) _out;

	out->display = walkerDpy->display_id;
	out->context = nativeContextContainer->ctx;
	out->surface.drawable = nativeSurfaceContainer->drawable;
	out->surface.config = nativeSurfaceContainer->config;

	return EGL_TRUE;
}
*/