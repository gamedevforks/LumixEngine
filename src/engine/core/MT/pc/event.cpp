#include "core/mt/event.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>

namespace Lumix
{
	namespace MT
	{
		Event::Event(EventFlags flags)
		{
			m_id = ::CreateEvent(nullptr, !!(flags & EventFlags::MANUAL_RESET), !!(flags & EventFlags::SIGNALED), nullptr);
		}

		Event::~Event()
		{
			::CloseHandle(m_id);
		}

		void Event::reset()
		{
			::ResetEvent(m_id);
		}

		void Event::trigger()
		{
			::SetEvent(m_id);
		}

		void Event::wait()
		{
			::WaitForSingleObject(m_id, INFINITE);
		}

		bool Event::poll()
		{
			return WAIT_OBJECT_0 == ::WaitForSingleObject(m_id, 0);
		}
	}; // ~namespace MT
}; // ~namespace Lumix
