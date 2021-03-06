#include "property_view.h"
#include "ui_property_view.h"
#include "assetbrowser.h"
#include "core/crc32.h"
#include "core/log.h"
#include "core/path_utils.h"
#include "core/resource_manager.h"
#include "core/resource_manager_base.h"
#include "editor/ieditor_command.h"
#include "editor/world_editor.h"
#include "engine.h"
#include "entity_list.h"
#include "entity_template_list.h"
#include "property_view/dynamic_object_model.h"
#include "property_view/entity_model.h"
#include "property_view/resource_model.h"
#include <qitemdelegate.h>


PropertyView::PropertyView(QWidget* parent) 
	: QDockWidget(parent)
	, m_ui(new Ui::PropertyView)
	, m_selected_entity(Lumix::INVALID_ENTITY)
{
	m_ui->setupUi(this);
}


PropertyView::~PropertyView()
{
	m_world_editor->entitySelected().unbind<PropertyView, &PropertyView::onEntitySelected>(this);
	delete m_ui;
}


QAbstractItemModel* PropertyView::getModel() const
{
	return m_ui->treeView->model();
}


void PropertyView::setModel(QAbstractItemModel* model, QAbstractItemDelegate* delegate)
{
	m_ui->treeView->model()->deleteLater();
	m_ui->treeView->itemDelegate()->deleteLater();
	m_ui->treeView->setModel(model);
	if (delegate)
	{
		m_ui->treeView->setItemDelegate(delegate);
	}
	else
	{
		m_ui->treeView->setItemDelegate(new QItemDelegate(this));
	}
	if (model)
	{
		connect(model, &QAbstractItemModel::rowsInserted, [this, model](const QModelIndex &parent, int, int){
			openPersistentEditors(model, parent);
		});
		openPersistentEditors(model, QModelIndex());
	}
}


void PropertyView::setWorldEditor(Lumix::WorldEditor& editor)
{
	m_world_editor = &editor;
	m_world_editor->entitySelected().bind<PropertyView, &PropertyView::onEntitySelected>(this);
}


void PropertyView::setAssetBrowser(AssetBrowser& asset_browser)
{
	m_asset_browser = &asset_browser;
	connect(m_asset_browser, &AssetBrowser::fileSelected, this, &PropertyView::setSelectedResourceFilename);
}


void PropertyView::setSelectedResourceFilename(const char* filename)
{
	m_world_editor->selectEntities(nullptr, 0);
	ResourceModel* model = new ResourceModel(*m_world_editor, Lumix::Path(filename));
	connect(model, &ResourceModel::modelReady, [model, this]()
	{
		openPersistentEditors(model, QModelIndex());
		m_ui->treeView->expandToDepth(0); 
	});
	if (model->getResource())
	{
		setModel(model, new DynamicObjectItemDelegate(this));
		m_ui->treeView->expandToDepth(0);
	}
}


void PropertyView::openPersistentEditors(QAbstractItemModel* model, const QModelIndex& parent)
{
	for (int i = 0; i < model->rowCount(parent); ++i)
	{
		auto index = model->index(i, 1, parent);
		if (model->data(index, DynamicObjectModel::PersistentEditorRole).toBool())
		{
			m_ui->treeView->openPersistentEditor(index);
		}
		openPersistentEditors(model, index);
	}
}


void PropertyView::onEntitySelected(const Lumix::Array<Lumix::Entity>& e)
{
	m_selected_entity = e.empty() ? Lumix::INVALID_ENTITY : e[0];
	if (e.size() == 1 && e[0] >= 0)
	{
		EntityModel* model = new EntityModel(*this, *m_world_editor, m_selected_entity);
		setModel(model, new DynamicObjectItemDelegate(m_ui->treeView));
		m_ui->treeView->expandAll();
	}
	else
	{
		setModel(nullptr, nullptr);
	}
}

