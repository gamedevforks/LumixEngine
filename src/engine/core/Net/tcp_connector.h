#pragma once

#include "lumix.h"

namespace Lumix
{
	class IAllocator;

	namespace Net
	{
		class TCPStream;

		class LUMIX_ENGINE_API TCPConnector
		{
		public:
			TCPConnector(IAllocator& allocator) : m_allocator(allocator), m_socket(0) {}
			~TCPConnector();

			TCPStream* connect(const char* ip, uint16_t port);
			void close(TCPStream* stream);

		private:
			IAllocator& m_allocator;
			uintptr_t m_socket;
		};
	} // ~namespace Net
} // ~namespace Lumix
