/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

// No Tracy headers, client, or argument evaluation in ordinary builds.
#ifdef UKNCBTL_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#define UKNC_PROFILE_SCOPE(name) ZoneScopedN(name)
#define UKNC_PROFILE_FRAME() FrameMark
#else
#define UKNC_PROFILE_SCOPE(name) ((void)0)
#define UKNC_PROFILE_FRAME() ((void)0)
#endif
