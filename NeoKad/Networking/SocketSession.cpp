#include "GlobalHeader.h"
#include "SocketSession.h"
#include "SmartSocket.h"
#include "../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Exception.h"
#include "./BandwidthControl/BandwidthLimit.h"

IMPLEMENT_OBJECT(CMessageSession, CComChannel)

void CMessageSession::SendPacket(const string& Name, const CVariant& Packet)
{
	m_pListner->SendPacket(Name, Packet, m_Address);
}

IMPLEMENT_OBJECT(CSocketListner, CObject)

CSocketListner::CSocketListner(CSmartSocket* pSocket) 
: CObject(pSocket) 
{
}

bool CSocketListner::SendPacket(const string& Name, const CVariant& Packet, const CSafeAddress& Address)
{
	CBuffer Buffer;
	MakePacket(Name, Packet, Buffer);
	return SendTo(Buffer, Address);
}

void CSocketListner::ReceiveFrom(const CBuffer& Buffer, const CSafeAddress& Address)
{
	try
	{
		uint8 Len = Buffer.ReadValue<uint8>();
		if(Buffer.GetSizeLeft() < Len)
			throw CException(LOG_ERROR | LOG_DEBUG, L"invalid variant name");
		string Name = string((char*)Buffer.ReadData(Len), Len);

		CVariant Packet;
		Packet.FromPacket(&Buffer);

		//bool bEncrypted = false;
		switch(Packet.IsEncrypted())
		{
			case CVariant::eAsymmetric:
				//bEncrypted = true;
				if(!Packet.Decrypt(GetParent<CSmartSocket>()->GetPrivateKey()))
					throw CException(LOG_ERROR, L"Recived encrypted packet that failed to decrypt");
				break;
			case CVariant::eSymmetric:
				ASSERT(0);
				break;
		}

		CPointer<CMessageSession> pChannel = new CMessageSession(this, Address/*, bEncrypted*/);
		GetParent<CSmartSocket>()->ProcessPacket(Name, Packet, pChannel);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), L"Recived malformated packet from %s, error %s", Address.ToString().c_str(), Exception.GetLine().c_str());
	}
}

///////////////////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CSocketSession, CComChannel)

CSocketSession::CSocketSession(CSocketListner* pListener, const CSafeAddress& Address) 
 : CComChannel(pListener), CBandwidthLimiter(pListener->GetParent()->Cast<CSmartSocket>())
{
	m_Address = Address;

	m_LockQueue = false;

	m_pCryptoKey = NULL;

#ifdef _LOG_STREAMS_
	m_InBound.setFileName("InBound_" + GetRand64Str() + ".bin");
	m_InBound.open(QFile::WriteOnly);
	m_OutBound.setFileName("OutBound_" + GetRand64Str() + ".bin");
	m_OutBound.open(QFile::WriteOnly);
#endif

	m_LastActivity = GetCurTick();

	// Install default local bandwidth limit
	//m_UpLimit = new CBandwidthLimit(this);
	//m_DownLimit = new CBandwidthLimit(this);
	//AddUpLimit(m_UpLimit);
	//AddDownLimit(m_DownLimit);
}

void CSocketSession::Process()
{
	// Note: m_Receiving is to be filled by the defived class
	RequestBandwidth(eDownChannel, (int)m_Receiving.GetSize()); // may update m_DownQuota
	while(m_Quota[eDownChannel] > 0 && m_Receiving.GetSize() > 0)
	{
		string Name;
		CVariant Packet;
		size_t uRead = m_Receiving.GetSize();
		if(!StreamPacket(m_Receiving, Name, Packet))
			break; // incomplete
		uRead -= m_Receiving.GetSize();

		CountBandwidth(eDownChannel, (int)uRead); // this updates m_DownQuota

		try
		{
			if(Packet.GetType() != CVariant::EMap)
				throw CException(LOG_ERROR, L"Recived unexpected packet type");
			ProcessPacket(Name, Packet);
		}
		catch(const CException& Exception)
		{
			LogLine(Exception.GetFlag(), L"Recived malformated packet from: %s; error %s", m_Address.ToString().c_str(), Exception.GetLine().c_str());
		}
	}

	if(m_LockQueue)
		return;

	RequestBandwidth(eUpChannel, (int)m_PacketQueue.GetSize());  // may update m_UpQuota
	SVarPacket* pPacket;
	while(m_Quota[eUpChannel] > 0 && (pPacket = m_PacketQueue.Front()) != NULL)
	{
		SendPacket(pPacket->Name, pPacket->Data);
		m_PacketQueue.Pop();
	}
}

UINT CSocketSession::QueuePacket(const string& Name, const CVariant& Packet, int iPriority/*, uint32 uTTL*/)
{
	return m_PacketQueue.Push(Name, Packet, iPriority/*, uTTL*/);
}

bool CSocketSession::IsQueued(UINT uID) const
{
	return m_PacketQueue.IsQueued(uID);
}

void CSocketSession::SendPacket(const string& Name, const CVariant& Packet)
{
	CBuffer Buffer;
	MakePacket(Name, Packet, Buffer);
	size_t uWriten = Buffer.GetSize();
	StreamOut(Buffer.GetBuffer(),Buffer.GetSize());

	CountBandwidth(eUpChannel, (int)uWriten); // this updates m_UpQuota
}

void CSocketSession::ProcessPacket(const string& Name, const CVariant& Packet)
{
	ASSERT(Packet.IsValid());
	if(Name.empty())
	{
		ASSERT(0);
		return;
	}

	if(Name[0] == L':')
	{
#ifdef _DEBUG
		if(Name == ":ECHO")
		{
			LogLine(LOG_INFO, L"Recived ECHO");
			QueuePacket(":echo", Packet);
			return;
		}
		else if(Name == ":echo")
		{
			LogLine(LOG_INFO, L"Recived echo");
			return;
		}
#endif

		LogLine(LOG_ERROR, L"unsupported socket suffix: %S", Name.c_str());
	}
	else
	{
		GetParent<CSmartSocket>()->ProcessPacket(Name, Packet, this);
	}
}

void CSocketSession::Encrypt(CSymmetricKey* pCryptoKey)
{
	ASSERT(m_LockQueue != false);

	ASSERT(m_pCryptoKey == NULL);
	m_pCryptoKey = pCryptoKey;

	// Note: we may got more now encrypted packets after this one, we must decrypt them
	if(m_Receiving.GetSizeLeft() > 0) // decrypt pending packets
		m_pCryptoKey->Decrypt(m_Receiving.GetData(0), m_Receiving.GetData(0), m_Receiving.GetSizeLeft());
}

void CSocketSession::StreamIn(byte* Data, size_t Length)
{
	byte* Ptr = m_Receiving.SetData(-1, Data, Length);
	if(m_pCryptoKey)
		m_pCryptoKey->Decrypt(Ptr, Ptr, Length);
#ifdef _LOG_STREAMS_
	m_InBound.write((char*)Ptr, Length);
#endif

/*#ifdef _LOG_STREAMS_
	static int Counter = 0;
	QFile InPacket;
	InPacket.setFileName("InStream_" + QString::number(Counter++) + ".bin");
	InPacket.open(QFile::WriteOnly);
	InPacket.write((char*)Ptr, Length);
	InPacket.close();
#endif*/
}

void CSocketSession::StreamOut(byte* Data, size_t Length)
{
#ifdef _LOG_STREAMS_
	m_OutBound.write((char*)Data, Length);
#endif
	byte* Ptr = m_Sending.SetData(-1, Data, Length);
	if(m_pCryptoKey)
		m_pCryptoKey->Encrypt(Ptr, Ptr, Length);
}

uint64 CSocketSession::GetIdleTime()
{
	return GetCurTick() - m_LastActivity;
}

void CSocketSession::KeepAlive()
{
	if(m_LastActivity) // if its 0 it means it failed and must timeout imminetly
		m_LastActivity = GetCurTick();
}