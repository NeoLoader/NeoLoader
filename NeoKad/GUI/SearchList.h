#pragma once

#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>

#include "../Framework/ObjectEx.h"
#include "Common/Pointer.h"
#include "Common/Variant.h"

class CKadLookup;
class CFrameRelay;
class CKadRoute;

class CSearchList: public QWidget
{
	Q_OBJECT
public:
	CSearchList(QWidget *parent = 0);
	~CSearchList();

	void				ClearSearchList() {m_pLookupList->clear();m_Lookups.clear();}
	void				DumpSearchList(map<CVariant, CPointer<CKadLookup> >& AuxMap);

	QVariant			GetCurID();

signals:
	void				itemDoubleClicked(QTreeWidgetItem*, int);

private slots:
	void				OnItemClicked(QTreeWidgetItem*, int);

protected:
	int					DumpLinks(CFrameRelay* pLink, QTreeWidgetItem* pItem);
	int					DumpSessions(CKadRoute* pRoute, QTreeWidgetItem* pItem);
	void				DumpTrace(CKadRoute* pRoute, QTreeWidgetItem* pItem);


	enum EColumns
	{
		eNumber = 0,
		eKey,
		eType,
		eName,
		eStatus,
		eRoutes,
		eLoad,
		eHops,
		eShares,
		eResults,
		eNodes,
		eUpload,
		eDownload
	};

	enum EColumns_Trace
	{
		eDirection = 0,
		eFID,
		eXID
	};

	enum EColumns_Link
	{
		eNumber_ = 0,
		eKey_,
		eType_,
		eName_,
		eStatus_,
		eStreamStats,
		eTotalFrames,
		ePendingFrames,
		eRelayedFrames,
		eDroppedFrames,
		eFramesStats,
	};

	enum EDetails
	{
		eUpLinks,
		eDownLinks,
		eSessions,
		eFrames,
	};

	struct SLookup
	{
		QTreeWidgetItem*	pItem;
		QMap<EDetails, QTreeWidgetItem*> Sub;
	};
	QMap<CVariant, SLookup*>		m_Lookups;

	enum ELevels
	{
		eLookupLine,	// Lookups
		eLinksLine,		// UpLink groupe / DownLink groupe
		eLinkLine,		// Individual Link
		eTableLine,		// Line for each target entity in upstream
		eListLine,		// Session groupe
		eConLine,		// Session Line
		eTraceLine,		// Trace Line
	};
	QMap<ELevels, QStringList>	m_Headers;

	QVBoxLayout*			m_pMainLayout;
	QTreeWidget*			m_pLookupList;
};