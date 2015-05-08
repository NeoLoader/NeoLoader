#include "GlobalHeader.h"
#include "NeoRoute.h"
#include "NeoSession.h"
#include "../../NeoCore.h"
#include "../../Interface/InterfaceManager.h"
#include "../../Networking/SocketThread.h"
#include "../../Networking/BandwidthControl/BandwidthManager.h"
#include "../../Common/Variant.h"
#include "../../../Framework/Exception.h"

CNeoSession::CNeoSession(const QByteArray& EntityID, const QByteArray& TargetID, const QByteArray& SessionID, QObject* pObject)
 : QObjectEx(pObject), CBandwidthLimiter(theCore->m_Network)
{
	m_EntityID = EntityID;
	m_TargetID = TargetID;
	m_SessionID = SessionID;

	m_Connected = 0;

	m_bUpload = false;
	m_bDownload = false;

	m_QueuedSize = 0;
	m_LastQueuedSize = 0;
}

void CNeoSession::Dispose()
{
	CBandwidthLimiter::Dispose();

	if(CNeoRoute* pRoute = ((CNeoRoute*)parent()))
		pRoute->RemoveSession(m_SessionID);

	emit Disconnected();
	delete this;
}

const QByteArray& CNeoSession::GetMyEntityID() const
{
	return ((CNeoRoute*)parent())->GetEntityID();
}

void CNeoSession::SendPacket(QString Name, QVariant Data)
{
	MakePacket(Name.toStdString(), Data, m_OutBuffer);
}

void CNeoSession::QueuePacket(QString Name, QVariant Data)
{
	m_QueuedStreams.append(CBuffer());
	CBuffer &Buffer = m_QueuedStreams.last();
	MakePacket(Name.toStdString(), Data, Buffer);
	m_QueuedSize += Buffer.GetSize();
}

void CNeoSession::Process(bool bConnected, const QVariantMap& Session)
{
	if(!bConnected)
		m_Connected = false;
	else if(!m_Connected)
	{
		m_Connected = true;
		emit Connected();
	}

	quint64 SendBytes = 0;
	uint64 QueuedBytes = Session["QueuedBytes"].toULongLong(); // Queued for sending
	while(m_OutBuffer.GetSize() > 0 || !m_QueuedStreams.isEmpty())
	{
		if(m_OutBuffer.GetSize() == 0 && !m_QueuedStreams.isEmpty())
		{
			CBuffer &Entry = m_QueuedStreams.first();
			m_QueuedSize -= Entry.GetSize();
			m_OutBuffer.AppendData(Entry.GetBuffer(), Entry.GetSize());
			m_QueuedStreams.removeFirst();
			//emit NextPacketSend();
		}

		//
		qint64 len = m_OutBuffer.GetSize();

		RequestBandwidth(CBandwidthLimiter::eUpChannel, len);

		qint64 uToWrite = Min(len, GetQuota(CBandwidthLimiter::eUpChannel));
		if(uToWrite <= 0)
			break;
//#ifdef MSS
//		if(theCore->m_Network->UseTransportLimiting() && GetQuota(CBandwidthLimiter::eDownChannel) < 0)
//			break;
//#endif
		ASSERT(uToWrite > 0);
		qint64 uWriten = -1;

		////
		QVariantMap Request;
		Request["MyEntityID"] = ((CNeoRoute*)parent())->GetEntityID();

		Request["EntityID"] = m_EntityID;
		Request["SessionID"] = m_SessionID;

		Request["Data"] = QByteArray((char*)m_OutBuffer.GetBuffer(), uToWrite);

		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueueBytes", Request).toMap();
		if(Response["Result"] == "ok")
			uWriten = uToWrite;
		////

		if(uWriten == -1)
			break;
		ASSERT(uToWrite >= uWriten);

		CountBandwidth(CBandwidthLimiter::eUpChannel, uWriten, IsUpload() ? CBandwidthCounter::ePayload : CBandwidthCounter::eProtocol);

		m_OutBuffer.ShiftData(uWriten);
		//


		SendBytes += uWriten;

		if(m_OutBuffer.GetSize() > 0)
			break; // if we did not send all it means the socket blocked, so we dont try again or else we would set the blocking flag
	}
	if(SendBytes)
	{
		emit Activity();
	}
	m_LastQueuedSize = SendBytes + QueuedBytes;



	quint64 ReceivedBytes = 0;
	uint64 PendingBytes = Session["PendingBytes"].toULongLong(); // Pending for recive
	while(PendingBytes > ReceivedBytes)
	{
		//
		RequestBandwidth(CBandwidthLimiter::eDownChannel, PendingBytes);

		qint64 uToRead = bConnected ? GetQuota(CBandwidthLimiter::eDownChannel) : (PendingBytes - ReceivedBytes); // if thats the last call pull all
		if(uToRead <= 0)
			break;
//#ifdef MSS
//		if(theCore->m_Network->UseTransportLimiting() && pStreamSocket->GetQuota(CBandwidthLimiter::eUpChannel) < 0)
//			break;
//#endif
		ASSERT(uToRead > 0);
		qint64 uRead = -1;

		////
		QVariantMap Request;
		Request["MyEntityID"] = ((CNeoRoute*)parent())->GetEntityID();

		Request["EntityID"] = m_EntityID;
		Request["SessionID"] = m_SessionID;

		Request["MaxBytes"] = uToRead;

		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "PullBytes", Request).toMap();
		if(Response["Result"] == "ok")
		{
			QByteArray Data = Response["Data"].toByteArray();
			m_InBuffer.AppendData(Data.data(), Data.size());
			uRead = Data.size();
		}
		////

		if(uRead == -1)
			break;
		ASSERT(uToRead >= uRead);

		CountBandwidth(CBandwidthLimiter::eDownChannel, uRead, IsDownload() ? CBandwidthCounter::ePayload : CBandwidthCounter::eProtocol);
		//

		
		ReceivedBytes += uRead;
	}
	if(ReceivedBytes)
	{
		emit Activity();
		ProcessStream();
	}

	if(!bConnected)
		emit Disconnected();
	return;
}

void CNeoSession::ProcessStream()
{
	while(m_InBuffer.GetSize() > 0)
	{
		string Name;
		QVariant Packet;
		if(!StreamPacket(m_InBuffer, Name, Packet))
			break; // incomplete

		if(!Packet.isValid())
		{
			LogLine(LOG_ERROR, tr("Recived malformed Variatn, named '%1'").arg(QString::fromStdString(Name)));
			continue;
		}

		emit ProcessPacket(QString::fromStdString(Name), Packet);
	}
}