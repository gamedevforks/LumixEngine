#pragma once


#include "lumix.h"
#include "core/iallocator.h"
#include "core/mt/spin_mutex.h"


namespace Lumix
{


	/// FIFOAllocator uses fixed ring buffer to allocate memory in FIFO order.
	class LUMIX_ENGINE_API FIFOAllocator : public IAllocator
	{
		public:
			FIFOAllocator(size_t buffer_size);
			~FIFOAllocator();

			void* allocate(size_t n);
			void deallocate(void* p);
		
		private:
			size_t m_buffer_size;
			uint8_t* m_buffer;
			int32_t m_start;
			int32_t m_end;
			MT::SpinMutex m_mutex;
	};


} // ~namespace Lumix
