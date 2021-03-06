#include "animation/animation.h"
#include "core/fs/file_system.h"
#include "core/fs/ifile.h"
#include "core/log.h"
#include "core/matrix.h"
#include "core/profiler.h"
#include "core/quat.h"
#include "core/resource_manager.h"
#include "core/vec3.h"
#include "renderer/model.h"
#include "renderer/pose.h"


namespace Lumix
{
	

Resource* AnimationManager::createResource(const Path& path)
{
	return m_allocator.newObject<Animation>(path, getOwner(), m_allocator);
}


void AnimationManager::destroyResource(Resource& resource)
{
	m_allocator.deleteObject(static_cast<Animation*>(&resource));
}


Animation::Animation(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
{
	m_rotations = nullptr;
	m_positions = nullptr;
	m_bones = nullptr;
	m_frame_count = 0;
	m_fps = 30;
}


Animation::~Animation()
{
	IAllocator& allocator = getAllocator();
	allocator.deallocate(m_positions);
	allocator.deallocate(m_rotations);
	allocator.deallocate(m_bones);
}


void Animation::getPose(float time, Pose& pose, Model& model) const
{
	PROFILE_FUNCTION();
	if(model.isReady())
	{
		int frame = (int)(time * m_fps);
		frame = frame >= m_frame_count ? m_frame_count - 1 : frame;
		Vec3* pos = pose.getPositions();
		Quat* rot = pose.getRotations();
		int off = frame * m_bone_count;
		int off2 = off + m_bone_count;
		float t = (time - frame / (float)m_fps) / (1.0f / m_fps);
	
		if(frame < m_frame_count - 1)
		{
			for(int i = 0; i < m_bone_count; ++i)
			{
				Model::BoneMap::iterator iter = model.getBoneIndex(m_bones[i]);
				if (iter.isValid())
				{
					int model_bone_index = iter.value();
					lerp(m_positions[off + i], m_positions[off2 + i], &pos[model_bone_index], t);
					nlerp(m_rotations[off + i], m_rotations[off2 + i], &rot[model_bone_index], t);
				}
			}
		}
		else
		{
			for(int i = 0; i < m_bone_count; ++i)
			{
				Model::BoneMap::iterator iter = model.getBoneIndex(m_bones[i]);
				if (iter.isValid())
				{
					int model_bone_index = iter.value();
					pos[model_bone_index] = m_positions[off + i];
					rot[model_bone_index] = m_rotations[off + i];
				}
			}
		}
		pose.setIsRelative();
		pose.computeAbsolute(model);
	}
}


void Animation::loaded(FS::IFile& file, bool success, FS::FileSystem& fs)
{
	if (success)
	{
		IAllocator& allocator = getAllocator();
		allocator.deallocate(m_positions);
		allocator.deallocate(m_rotations);
		allocator.deallocate(m_bones);
		m_positions = nullptr;
		m_rotations = nullptr;
		m_bones = 0;
		m_frame_count = m_bone_count = 0;
		Header header;
		file.read(&header, sizeof(header));
		if (header.magic != HEADER_MAGIC)
		{
			onFailure();
			g_log_error.log("animation") << m_path.c_str() << " is not an animation file";
			return;
		}
		if (header.version > 1)
		{
			onFailure();
			g_log_error.log("animation") << "Unsupported animation version " << header.version << " (" << m_path.c_str() << ")";
			return;
		}
		m_fps = header.fps;
		file.read(&m_frame_count, sizeof(m_frame_count));
		file.read(&m_bone_count, sizeof(m_bone_count));

		m_positions = static_cast<Vec3*>(allocator.allocate(sizeof(Vec3) * m_frame_count * m_bone_count));
		m_rotations = static_cast<Quat*>(allocator.allocate(sizeof(Quat) * m_frame_count * m_bone_count));
		m_bones = static_cast<uint32_t*>(allocator.allocate(sizeof(uint32_t) * m_bone_count));
		file.read(&m_positions[0], sizeof(Vec3)* m_bone_count * m_frame_count);
		file.read(&m_rotations[0], sizeof(Quat)* m_bone_count * m_frame_count);
		file.read(m_bones, sizeof(m_bones[0]) * m_bone_count);
		
		m_size = file.size();
		decrementDepCount();
	}
	else
	{
		onFailure();
	}
}


IAllocator& Animation::getAllocator()
{
	return static_cast<AnimationManager*>(m_resource_manager.get(ResourceManager::ANIMATION))->getAllocator();
}


void Animation::doUnload(void)
{
	IAllocator& allocator = getAllocator();
	allocator.deallocate(m_positions);
	allocator.deallocate(m_rotations);
	allocator.deallocate(m_bones);
	m_rotations = nullptr;
	m_positions = nullptr;
	m_bones = nullptr;
	m_frame_count = 0;
	m_size = 0;
	onEmpty();
}


} // ~namespace Lumix
