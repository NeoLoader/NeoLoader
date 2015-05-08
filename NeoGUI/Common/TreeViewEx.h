#pragma once

#include <QStyledItemDelegate>

class QTreeViewEx: public QTreeView
{
	Q_OBJECT
public:
	QTreeViewEx(QWidget *parent = 0) : QTreeView(parent) 
	{
		header()->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

		m_pMenu = new QMenu(this);
	}

	QModelIndexList selectedRows() const
	{
		QModelIndexList IndexList;
		foreach(const QModelIndex& Index, selectedIndexes())
		{
			if(Index.column() == 0)
				IndexList.append(Index);
		}
		return IndexList;
	}

	template<class T>
	void StartUpdatingWidgets(T& OldMap, T& Map)
	{
		for(typename T::iterator I = Map.begin(); I != Map.end();)
		{
			if(I.value().first == NULL)
				I = Map.erase(I);
			else
			{
				OldMap.insert(I.key(), I.value());
				I++;
			}
		}
	}

	template<class T>
	void EndUpdatingWidgets(T& OldMap, T& Map)
	{
		for(typename T::iterator I = OldMap.begin(); I != OldMap.end(); I++)
		{
			Map.remove(I.key());
			if(I.value().second.isValid())
				setIndexWidget(I.value().second, NULL);
		}
	}

private slots:
	void				OnMenuRequested(const QPoint &point)
	{
		QAbstractItemModel* pModel = model();
		if(m_Columns.isEmpty())
		{
			for(int i=0; i < pModel->columnCount(); i++)
			{
				QString Label = pModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
				if(Label.isEmpty())
					continue;
				QAction* pAction = new QAction(Label, m_pMenu);
				pAction->setCheckable(true);
				pAction->setChecked(!isColumnHidden(i));
				connect(pAction, SIGNAL(triggered()), this, SLOT(OnMenu()));
				m_pMenu->addAction(pAction);
				m_Columns[pAction] = i;
			}
		}

		for(QMap<QAction*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
			I.key()->setChecked(!isColumnHidden(I.value()));

		m_pMenu->popup(QCursor::pos());	
	}

	void				OnMenu()
	{
		QAction* pAction = (QAction*)sender();
		int Column = m_Columns.value(pAction, -1);
		setColumnHidden(Column, !pAction->isChecked());
	}

protected:
	QMenu*				m_pMenu;
	QMap<QAction*, int>	m_Columns;
};

class QStyledItemDelegate16 : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QStyledItemDelegate16(QObject *parent = 0)
        : QStyledItemDelegate(parent) {}
	
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(16);
		return size;
	}
};