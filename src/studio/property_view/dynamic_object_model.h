#pragma once


#include "core/vec3.h"
#include <qabstractitemmodel.h>
#include <qstyleditemdelegate.h>
#include <functional>


class DynamicObjectItemDelegate : public QStyledItemDelegate
{
public:
	DynamicObjectItemDelegate(QWidget* parent);
	
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem&, const QModelIndex& index) override;
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};



class DynamicObjectModel : public QAbstractItemModel
{
	public:
		enum UserRoles
		{
			PersistentEditorRole = Qt::UserRole + 1
		};

		class Node
		{
			public:
				Node(QString name, Node* parent, int index) : m_name(name), m_parent(parent), m_index(index), m_is_persistent_editor(false) {}
				~Node();

				Node& addChild(QString name)
				{
					Node* child = new Node(name, this, m_children.size());
					m_children.push_back(child);
					return *child;
				}

				Node& addChild(QString name, int row)
				{
					Node* child = new Node(name, this, m_children.size());
					m_children.insert(row, child);
					return *child;
				}

				void enablePeristentEditor() { m_is_persistent_editor = true; }

				std::function<QVariant()> m_getter;
				std::function<QVariant()> m_decoration;
				std::function<QVariant()> m_size_hint;
				std::function<void(const QVariant&)> m_setter;
				std::function<void(QWidget*, QPoint)> onClick;
				std::function<void(QPainter*, const QStyleOptionViewItem&)> onPaint;
				std::function<QWidget*(QWidget*, const QStyleOptionViewItem&)> onCreateEditor;
				std::function<void(QWidget*)> onSetModelData;
				std::function<bool(const QMimeData*, Qt::DropAction)> onDrop;
				int m_index;
				QString m_name;
				Node* m_parent;
				QList<Node*> m_children;
				bool m_is_persistent_editor;
		};

		typedef char yes[2];
		typedef char no[1];

		template <typename T> static yes& isFunctor(decltype(T::operator())*);
		template <typename T> static no& isFunctor(...);

		template<typename Functor, typename Ret>
		struct IsFunctor : std::enable_if<sizeof(isFunctor<Functor>(0)) == sizeof(yes), Ret>
		{};

		template<typename Functor, typename Ret>
		struct IsNotFunctor : std::enable_if<sizeof(isFunctor<Functor>(0)) == sizeof(no), Ret>
		{};

		template <typename T>
		class Object
		{
			public:

				template <typename Getter, typename Namer>
				class Array
				{
					public:
						Array(T* parent, int count, Node* node, Getter getter, Namer namer)
						{
							m_parent = parent;
							m_node = node;
							m_getter = getter;
							for (int i = 0; i < count; ++i)
							{
								Node& child = node->addChild(QString("%1").arg(i));
								auto o = (m_parent->*m_getter)(i);
								child.m_getter = [o, namer]() -> QVariant { return namer(o); };
							}
						}

						template <typename Functor, typename Adder>
						Array<Getter, Namer>& forEach(Functor functor, Adder adder)
						{
							auto array_node = m_node;
							auto parent = m_parent;
							auto getter = m_getter;
							m_node->onCreateEditor = [=](QWidget* parent_widget, const QStyleOptionViewItem&) -> QWidget* {
								auto button = new QPushButton(" + ", parent_widget);
								button->connect(button, &QPushButton::clicked, [=](){
									if (adder())
									{
										int i = array_node->m_children.size();
										Node& child = array_node->addChild(QString("%1").arg(i));
										auto o = (parent->*getter)(i);
										functor(i, o, child);
									}
								});
								return button;
							};
							m_node->m_setter = [](const QVariant&) {};
							return forEach(functor);
						}

						template <typename Functor>
						Array<Getter, Namer>& forEach(Functor functor)
						{
							for (int i = 0; i < m_node->m_children.size(); ++i)
							{
								auto o = (m_parent->*m_getter)(i);
								Node& node = *m_node->m_children[i];
								functor(i, o, node);
							}
							return *this;
						}

						Node& getNode() { return *m_node; }

					private:
						T* m_parent;
						Node* m_node;
						Getter m_getter;
				};

			public:
				Object(T* instance, Node* node)
					: m_instance(instance) 
					, m_node(node)
				{
				}

				Object& propertyColor(QString name, Lumix::Vec3(T::*getter)() const, void (T::*setter)(const Lumix::Vec3&))
				{
					Node& node = m_node->addChild(name);
					T* inst = m_instance;
					node.m_getter = [getter, inst]() -> QVariant {
						Lumix::Vec3 v = (inst->*getter)();
						return QColor(v.x * 255, v.y * 255, v.z * 255);
					};
					node.m_setter = [inst, setter](const QVariant& value) { 
						Lumix::Vec3 c;
						c.x = qvariant_cast<QColor>(value).redF();
						c.y = qvariant_cast<QColor>(value).greenF();
						c.z = qvariant_cast<QColor>(value).blueF();
						(inst->*setter)(c);
					};
					return *this;
				}

				template <typename Getter, typename PropertyType>
				Object& property(QString name, Getter getter, void (T::*setter)(PropertyType))
				{
					Node& node = m_node->addChild(name);
					T* inst = m_instance;
					node.m_getter = [getter, inst]() -> QVariant { return (inst->*getter)(); };
					node.m_setter = [inst, setter](const QVariant& value) { (inst->*setter)(value.value<PropertyType>()); };
					return *this;
				}

				template <typename Getter, typename Setter>
				Object& property(QString name, Getter getter, Setter setter)
				{
					Node& node = m_node->addChild(name);
					T* inst = m_instance;
					node.m_getter = [getter, inst]() -> QVariant { return getter(inst); };
					node.m_setter = [getter, setter, inst](const QVariant& value) { 
						setter(inst, value.value<decltype(getter(inst))>());
					};
					return *this;
				}
				
				template <typename Getter>
				typename IsFunctor<Getter, Object&>::type property(QString name, Getter getter)
				{
					Node& node = m_node->addChild(name);
					T* inst = m_instance;
					node.m_getter = [getter, inst]() -> QVariant { return getter(inst); };
					return *this;
				}

				template <typename Getter>
				typename IsNotFunctor<Getter, Object&>::type property(QString name, Getter getter)
				{
					Node& node = m_node->addChild(name);
					T* inst = m_instance;
					node.m_getter = [getter, inst]() -> QVariant { return (inst->*getter)(); };
					return *this;
				}

				template <typename Getter, typename Namer>
				Array<Getter, Namer> array(QString name, int count, Getter getter, Namer namer)
				{
					Node& node = m_node->addChild(name);
					node.m_getter = []() -> QVariant { return ""; };
					/*node.onCreateEditor = [adder, this](QWidget* parent, const QStyleOptionViewItem&) -> QWidget* {
						auto button = new QPushButton(" + ", parent);
						connect(button, &QPushButton::clicked, adder);
						return button;
					};
					node.m_setter = [](const QVariant&) {};*/

					return Array<Getter, Namer>(m_instance, count, &node, getter, namer);
				}
				
				Node& getNode() { return *m_node; }

			private:
				T* m_instance;
				Node* m_node;
		};

	public:
		DynamicObjectModel();
		~DynamicObjectModel();

		template <typename T>
		Object<T> object(QString name, T* instance)
		{
			m_root->m_name = name;
			return Object<T>(instance, m_root);
		}

		virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
		virtual QModelIndex parent(const QModelIndex& child) const override;

		virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
		virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

		virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
		virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
		virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
		virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
		virtual Qt::DropActions supportedDropActions() const override;
		virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
		virtual QStringList mimeTypes() const override;

		Node& getRoot() { return *m_root; }
		void childAboutToBeAdded(Node& node);
		void childAdded();
		QModelIndex getIndex(Node& node);
		void removeNode(Node& node);

		static void setSliderEditor(Node& node, float min, float max, float step);

	private:
		Node* m_root;
};