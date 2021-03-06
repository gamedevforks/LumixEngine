#include "lumix.h"
#include "core/path.h"
#include "core/resource_manager.h"
#include "core/resource_manager_base.h"

namespace Lumix
{
	ResourceManager::ResourceManager(IAllocator& allocator) 
		: m_resource_managers(allocator)
		, m_allocator(allocator)
		, m_loading_resources_count(0)
	{
	}

	ResourceManager::~ResourceManager()
	{
	}

	void ResourceManager::create(FS::FileSystem& fs)
	{
		m_file_system = &fs;
	}

	void ResourceManager::destroy()
	{
	}
	
	ResourceManagerBase* ResourceManager::get(uint32_t id)
	{
		return m_resource_managers[id]; 
	}

	void ResourceManager::add(uint32_t id, ResourceManagerBase* rm)
	{ 
		m_resource_managers.insert(id, rm);
	}

	void ResourceManager::remove(uint32_t id) 
	{ 
		m_resource_managers.erase(id); 
	}

	void ResourceManager::reload(const char* path)
	{
		for (auto iter = m_resource_managers.begin(), end = m_resource_managers.end(); iter != end; ++iter)
		{
			iter.value()->reload(Path(path));
		}
	}


	void ResourceManager::incrementLoadingResources()
	{
		++m_loading_resources_count;
	}


	void ResourceManager::decrementLoadingResources()
	{
		ASSERT(m_loading_resources_count > 0);
		--m_loading_resources_count;
	}

	
	bool ResourceManager::isLoading() const
	{
		return m_loading_resources_count > 0;
	}
}