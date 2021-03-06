#include "insert_mesh_command.h"
#include "core/crc32.h"
#include "core/json_serializer.h"
#include "core/stack_allocator.h"
#include "engine.h"
#include "renderer/render_scene.h"
#include "universe/universe.h"


static const uint32_t RENDERABLE_HASH = Lumix::crc32("renderable");


InsertMeshCommand::InsertMeshCommand(Lumix::WorldEditor& editor)
	: m_editor(editor)
{
}


InsertMeshCommand::InsertMeshCommand(Lumix::WorldEditor& editor,
									 const Lumix::Vec3& position,
									 const Lumix::Path& mesh_path)
	: m_mesh_path(mesh_path)
	, m_position(position)
	, m_editor(editor)
{
}


void InsertMeshCommand::serialize(Lumix::JsonSerializer& serializer)
{
	serializer.serialize("path", m_mesh_path.c_str());
	serializer.beginArray("pos");
	serializer.serializeArrayItem(m_position.x);
	serializer.serializeArrayItem(m_position.y);
	serializer.serializeArrayItem(m_position.z);
	serializer.endArray();
}


void InsertMeshCommand::deserialize(Lumix::JsonSerializer& serializer)
{
	char path[Lumix::MAX_PATH_LENGTH];
	serializer.deserialize("path", path, sizeof(path), "");
	m_mesh_path = path;
	serializer.deserializeArrayBegin("pos");
	serializer.deserializeArrayItem(m_position.x, 0);
	serializer.deserializeArrayItem(m_position.y, 0);
	serializer.deserializeArrayItem(m_position.z, 0);
	serializer.deserializeArrayEnd();
}


void InsertMeshCommand::execute()
{
	Lumix::Engine& engine = m_editor.getEngine();
	Lumix::Universe* universe = m_editor.getUniverse();
	m_entity = universe->createEntity();
	universe->setPosition(m_entity, m_position);
	const Lumix::Array<Lumix::IScene*>& scenes = m_editor.getScenes();
	Lumix::ComponentIndex cmp;
	Lumix::IScene* scene = nullptr;
	for (int i = 0; i < scenes.size(); ++i)
	{
		cmp = scenes[i]->createComponent(RENDERABLE_HASH, m_entity);

		if (cmp >= 0)
		{
			scene = scenes[i];
			break;
		}
	}
	if (cmp >= 0)
	{
		char rel_path[Lumix::MAX_PATH_LENGTH];
		m_editor.getRelativePath(
			rel_path, Lumix::MAX_PATH_LENGTH, m_mesh_path.c_str());
		Lumix::StackAllocator<Lumix::MAX_PATH_LENGTH> allocator;
		static_cast<Lumix::RenderScene*>(scene)
			->setRenderablePath(cmp, rel_path);
	}
}


void InsertMeshCommand::undo()
{
	const Lumix::WorldEditor::ComponentList& cmps =
		m_editor.getComponents(m_entity);
	for (int i = 0; i < cmps.size(); ++i)
	{
		cmps[i].scene->destroyComponent(cmps[i].index, cmps[i].type);
	}
	m_editor.getUniverse()->destroyEntity(m_entity);
	m_entity = Lumix::INVALID_ENTITY;
}


uint32_t InsertMeshCommand::getType()
{
	static const uint32_t type = Lumix::crc32("insert_mesh");
	return type;
}


bool InsertMeshCommand::merge(IEditorCommand&)
{
	return false;
}
