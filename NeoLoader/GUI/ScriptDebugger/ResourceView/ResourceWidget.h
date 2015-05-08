#pragma once

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMenu>
#endif


class CResourceWidget: public QWidget
{
    Q_OBJECT
public:
    CResourceWidget(QWidget *parent = 0);
    ~CResourceWidget();

	void SyncResources(const QVariant& Resources);
	qint64 CurrentResourceID();
	QVariantMap GetResource(qint64 ID);

	void SetContextMenu(QMenu* pMenu);

signals:
    void ResourceSelected(qint64 ID);
	void ResourceActivated(qint64 ID);

private slots:
	void OnMenuRequested(const QPoint &);
	void OnItemClicked(QTreeWidgetItem*, int);
	void OnItemDoubleClicked(QTreeWidgetItem*, int);

protected:
	void UpdateItem(QTreeWidgetItem* CurItem, const QVariantMap& CurResource);

	QTreeWidget* m_pView;
	QMenu* m_pMenu;
	QMap<qint64, QVariantMap> m_Resources;

    Q_DISABLE_COPY(CResourceWidget)
};

