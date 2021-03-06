#pragma once


#include "lumix.h"


namespace Lumix
{
	class IAllocator;
}


class LUMIX_STUDIO_LIB_API FPSLimiter
{
	public:
		static void destroy(FPSLimiter* limiter);
		static FPSLimiter* create(int fps, Lumix::IAllocator& allocator);

		virtual ~FPSLimiter() {}

		virtual void beginFrame() = 0;
		virtual void endFrame() = 0;
};