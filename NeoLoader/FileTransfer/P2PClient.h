#pragma once

#include "../../Framework/ObjectEx.h"
class CBandwidthLimit;
class CFile;

class CP2PClient : public QObjectEx
{
    Q_OBJECT

public:
	CP2PClient(QObject* qObject = 0);
	virtual ~CP2PClient();

	virtual void					Process(UINT Tick) = 0;

	virtual QString					GetSoftware()				{return m_Software;}

	virtual QString					GetUrl() = 0;

	virtual QString					GetConnectionStr() = 0;

	virtual QString					GetTypeStr();

	virtual CFile*					GetFile() = 0;

	virtual bool					HasError() const			{return !m_Error.isEmpty();}
	virtual const QString&			GetError() const			{return m_Error;}

	virtual CBandwidthLimit*		GetUpLimit()				{return m_UpLimit;}
	virtual CBandwidthLimit*		GetDownLimit()				{return m_DownLimit;}

protected:
	CBandwidthLimit*				m_UpLimit;
	CBandwidthLimit*				m_DownLimit;

	QString							m_Software;

	QString							m_Error;
};