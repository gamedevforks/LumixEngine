#include "fileserverwidget.h"
#include "ui_fileserverwidget.h"
#include "core/default_allocator.h"
#include "core/fs/file_system.h"
#include "core/fs/file_events_device.h"
#include "editor/world_editor.h"
#include "engine.h"
#include <QElapsedTimer>


namespace
{
	const QString eventNames[] = 
	{
		"Started Opening",
		"Finished Opening",
		"Started Closing",
		"Finished Closing",
		"Started Reading",
		"Finished Reading",
		"Started Writing",
		"Finished Writing",
		"Started Getting Size",
		"Finished Getting Size",
		"Started Seeking",
		"Finished Seeking",
		"Started Getting Position",
		"Finished Getting Position"
	};
}


class FileServerWatcher
{
	public:
		FileServerWatcher(FileServerWidget& widget)
			: m_widget(widget)
			, m_fe_device(m_allocator)
		{
			m_timer.start();
			m_fe_device.OnEvent.bind<FileServerWatcher, &FileServerWatcher::fsEventCb>(this);
		}

		void fsEventCb(const Lumix::FS::Event& event)
		{
			m_widget.emitFileEvent(event, m_timer.nsecsElapsed());
		}

		Lumix::FS::FileEventsDevice& getFileEventDevice() 
		{ 
			return m_fe_device; 
		}

	private:
		Lumix::DefaultAllocator m_allocator;
		Lumix::FS::FileEventsDevice m_fe_device;
		QElapsedTimer m_timer;
		FileServerWidget& m_widget;
};


FileServerWidget::FileServerWidget(QWidget* parent) 
	: QDockWidget(parent)
	, m_ui(new Ui::FileServerWidget)
{
	m_ui->setupUi(this);
	m_ui->tableWidget->setColumnWidth(0, 75);
	m_ui->tableWidget->setColumnWidth(1, 200);
	m_editor = nullptr;
	m_watcher = new FileServerWatcher(*this);
	connect(this,
			SIGNAL(fileEvent(
				qint32, qint64, const QString&, qint32, qint32, qint64)),
			this,
			SLOT(onFileEvent(
				qint32, qint64, const QString&, qint32, qint32, qint64)));
}


FileServerWidget::~FileServerWidget()
{
	delete m_ui;
	delete m_watcher;
}


void FileServerWidget::onFileEvent(qint32 event, qint64 handle, const QString& path, qint32 ret, qint32 param, qint64 time)
{
	int row = m_ui->tableWidget->rowCount();
	m_ui->tableWidget->insertRow(row);
	
	QString base_path = m_editor->getBasePath();

	QTableWidgetItem* new_item = new QTableWidgetItem(QString::number(double(time / 1000000.0)));
	m_ui->tableWidget->setItem(row, 0, new_item);

	QTableWidgetItem* event_item = new QTableWidgetItem(eventNames[event]);
	m_ui->tableWidget->setItem(row, 1, event_item);

	QTableWidgetItem* handle_item = new QTableWidgetItem(QString::number(handle));
	m_ui->tableWidget->setItem(row, 2, handle_item);

	QString real_path(path);
	if(path.startsWith(base_path))
	{
		QTableWidgetItem* path_item = new QTableWidgetItem(real_path.remove(0, base_path.length()));
		m_ui->tableWidget->setItem(row, 3, path_item);
	}
	else
	{
		QTableWidgetItem* path_item = new QTableWidgetItem(real_path);
		m_ui->tableWidget->setItem(row, 3, path_item);
	}
	
	QTableWidgetItem* param_item = new QTableWidgetItem(QString::number(param));
	m_ui->tableWidget->setItem(row, 4, param_item);

	QTableWidgetItem* return_item = new QTableWidgetItem(QString::number(ret));
	m_ui->tableWidget->setItem(row, 5, return_item);

	filterRow(row);
}


void FileServerWidget::emitFileEvent(const Lumix::FS::Event& event, qint64 time)
{
	emit fileEvent((qint32)event.type, event.handle, event.path, event.ret, event.param, time);
}


void FileServerWidget::setWorldEditor(Lumix::WorldEditor& editor)
{
	m_editor = &editor;

	Lumix::FS::FileEventsDevice& dev = m_watcher->getFileEventDevice();
	Lumix::FS::FileSystem& fs = m_editor->getEngine().getFileSystem();
	fs.mount(&dev);
	if (strcmp(fs.getDefaultDevice().m_devices[0]->name(), "disk") == 0 &&
		strcmp(fs.getDefaultDevice().m_devices[1]->name(), "memory") == 0)
	{
		fs.setDefaultDevice("memory:events:disk");
	}
	else if (strcmp(fs.getDefaultDevice().m_devices[0]->name(), "tcp") == 0 &&
			 strcmp(fs.getDefaultDevice().m_devices[1]->name(), "memory") == 0)
	{
		fs.setDefaultDevice("memory:events:tcp");
	}
	else
	{
		ASSERT(false);
	}
}


void FileServerWidget::on_pushButton_clicked()
{
	while (m_ui->tableWidget->rowCount() > 0)
	{
		m_ui->tableWidget->removeRow(0);
	}
}


void FileServerWidget::on_filterCB_clicked()
{
	filterTable();
}


void FileServerWidget::on_filter_returnPressed()
{
    filterTable();
}


void FileServerWidget::on_checkBox_stateChanged(int)
{
	ASSERT(m_editor);
	Lumix::FS::FileSystem& fs = m_editor->getEngine().getFileSystem();
	if (strcmp(fs.getDefaultDevice().m_devices[0]->name(), "disk") == 0 &&
		strcmp(fs.getDefaultDevice().m_devices[1]->name(), "memory") == 0)
	{
		if (m_ui->checkBox->isChecked())
		{
			fs.setDefaultDevice("memory:events:disk");
		}
		else
		{
			fs.setDefaultDevice("memory:disk");
		}
	}
	else
	{
		ASSERT(false);
	}
}


void FileServerWidget::filterRow(int row)
{
	QString filter = m_ui->filter->text();

	bool match = false;
	for (int j = 0, c = m_ui->tableWidget->columnCount(); j < c; ++j)
	{
		QTableWidgetItem* item = m_ui->tableWidget->item(row, j);
		match |= item->text().contains(filter) ? true : false;
	}

	m_ui->tableWidget->setRowHidden(row, !match);
}


void FileServerWidget::filterTable()
{
	for (int i = 0, c = m_ui->tableWidget->rowCount(); i < c; ++i)
	{
		if (m_ui->filterCB->isChecked())
		{
			filterRow(i);
		}
		else
		{
			m_ui->tableWidget->setRowHidden(i, false);
		}
	}
}
