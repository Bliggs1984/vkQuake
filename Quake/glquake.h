/*
Dispatcher: the classic Vulkan renderer and the RayTracedGL1 renderer have
irreconcilable internals, so each build configuration gets its own header.
RT-Release / RT-Debug define RT_RENDERER on the compiler command line.
*/
#ifndef GLQUAKE_DISPATCH_H
#define GLQUAKE_DISPATCH_H
#ifdef RT_RENDERER
#include "rt_glquake.h"
#else
#include "vk_glquake.h"
#endif
#endif
