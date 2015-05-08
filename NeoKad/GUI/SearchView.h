#pragma once

#include<QWidget>
#include<QVBoxLayout>

#include "../Framework/ObjectEx.h"
#include "Common/Pointer.h"
#include "Common/Variant.h"

class CKadLookup;

class CRouteGraph;

class CSearchView: public QWidget
{
	Q_OBJECT
public:
	CSearchView(QWidget *parent = 0);

	void				DumpSearch(QVariant ID, map<CVariant, CPointer<CKadLookup> >& AuxMap);

protected:
	QVariant			m_CurID;

	QVBoxLayout			*m_pMainLayout;

	CRouteGraph 		*m_pRouteGraph;
};