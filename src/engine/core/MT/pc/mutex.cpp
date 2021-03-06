#include "core/mt/mutex.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>


namespace Lumix
{
	namespace MT
	{
		Mutex::Mutex(bool locked)
		{
			m_id = ::CreateMutex(nullptr, locked, nullptr);
		}

		Mutex::~Mutex()
		{
			::CloseHandle(m_id);
		}

		void Mutex::lock()
		{
			::WaitForSingleObject(m_id, INFINITE);
		}

		bool Mutex::poll()
		{
			uint32_t res = ::WaitForSingleObject(m_id, 0);
			return res > 0;
		}

		void Mutex::unlock()
		{
			::ReleaseMutex(m_id);
		}
	} // ~namespace MT
} // ~namespace Lumix
