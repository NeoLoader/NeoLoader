#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "../KadHandler.h"
#include "RouteSession.h"
#include "KadRoute.h"
#include "../KadConfig.h"
#include "../KadNode.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "../../Common/Crypto.h"

IMPLEMENT_OBJECT(CRouteSession, CRouteStats)

CRouteSession::CRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent)
 : CRouteStats(new SSessionStats()
 , pParent->GetParent<CKademlia>()->Cfg()->GetInt("MinFrameTTL")
 , pParent->GetParent<CKademlia>()->Cfg()->GetInt("MaxFrameTTL")
 , pParent->GetParent<CKademlia>()->Cfg()->GetInt("MaxWindowSize")
 , pParent)
{
	m_EntityID = EntityID;
	m_TargetID = TargetID;
	m_SendOffset = 0;
	m_SegmentQueueSize = 0;
	m_RecvOffset = 0;
	m_ConnectionStage = eNone;

	m_uLastUpdate = GetCurTick();

	m_FIDCounter = GetRand64() & 0x0000FFFFFFFFFFFF;
}

void CRouteSession::Destroy()
{
	if(m_ConnectionStage != eClosed && m_ConnectionStage != eBroken)
		Closed(true);
	delete this;
}

/*void TestPending(list<CScoped<CRouteSession::SFrame> >& m_FrameQueue, int Pending)
{
	int Count = 0;
	for(list<CScoped<CRouteSession::SFrame> >::iterator J = m_FrameQueue.begin(); J != m_FrameQueue.end(); J++)
	{
		if((*J)->SendTime != 0)
			Count++;
	}
	ASSERT(Pending == Count);
}*/

bool CRouteSession::Process(UINT Tick)
{
	uint64 CurTick = GetCurTick();
	switch(m_ConnectionStage)
	{
	case eNone:
	case eBroken:
		return false;
	case eClosed: // we must wait for the close ack
		if(m_FrameQueue.empty())
			return false;
	default:
		if((Tick & EPerSec) == 0 && CurTick - m_uLastUpdate > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RouteTimeout")))
			return false;
	}

	if(IsConnected())
	{
		for(map<uint64, CBuffer>::iterator I = m_SegmentQueue.begin(); I != m_SegmentQueue.end(); )
		{
			// K-ToDo-Now: return false if tomany frames queued, make a more precise size based estimation
			if(m_FrameQueue.size() >= GetWindowSize() * 2)
				break;

			CVariant Segment;
			if(I->first < UINT_MAX)
				Segment["OFF"] = (uint32)I->first;
			else
				Segment["OFF"] = I->first;
			Segment["DATA"] = I->second;

			QueueFrame("SEG", Segment);

			m_SegmentQueueSize -= I->second.GetSize();
			ASSERT(m_SegmentQueueSize >= 0);
			I = m_SegmentQueue.erase(I);
		}
	}

	if((Tick & E10PerSec) != 0)
		UpdateControl();

	CKadRoute* pRoute = GetParent<CKadRoute>();
	size_t SizeOnRoute = 0;
	for(TFrameList::iterator J = m_FrameQueue.begin(); J != m_FrameQueue.end(); J++)
	{
		SFrame* pFrame = *J;

		if(pFrame->SendTime != 0)
		{
			if(CurTick - pFrame->SendTime < pFrame->CurTTL * 2)
			{
				SizeOnRoute += pFrame->uSize;
				continue; // this one is on route
			}
			else
			{
				pFrame->SendTime = 0;
				FrameDropped();

				if(pFrame->SendCount > Max((uint32)GetParent<CKademlia>()->Cfg()->GetInt("MaxResend"), GetWindowSize()))
				{
					LogLine(LOG_DEBUG, L"Session %s Broken, Max Resend excided", ToHex(m_EntityID.GetData(), m_EntityID.GetSize()).c_str());
					Closed(true);
					break;
				}
			}
		}

		if(IsWindowFull())
			continue; // we cant send more right now, our window is full, have to loop through all for timrout cleanup

		CVariant Frame = pFrame->Frame;
		if(pFrame->SendCount) // K-ToDo-Now: do not relay frames with a seen ID and already one relayed "RC" field
			Frame["RC"] = pFrame->SendCount;
		if(pFrame->bSign)
			Frame.Sign(pRoute->GetPrivateKey());
		else if(m_SessionKey)
		{
			CHashFunction Hash(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
			Hash.Add(m_SessionKey->GetKey(), m_SessionKey->GetSize());
			Frame.Hash(&Hash);
		}

		Frame.Freeze(); // for get size
		pFrame->uSize = Frame.GetSize();
		SizeOnRoute += pFrame->uSize;

		pFrame->CurTTL = GetTimeOut() * 2				// The frame is allowed to live twice as long as the normal frame needs for its way
						//K-ToDo-Now: randomise++ TTL to make sure we are not the obviuse origin!!!!!!!
						* ((SizeOnRoute / 1024) + 1)	// Note: the average timeout is calculated per KB soe we must multiply here for each started PB
						* (pFrame->SendCount + 1);		// On each try we give the frame more time
		if(pRoute->RelayUp(Frame, pFrame->CurTTL))
		{
			FramePending();
			pFrame->SendTime = CurTick;
			pFrame->SendCount++;
		}
	}

	while(m_Payloads.size() > 0)
	{
		if(IsBussy())
			break;

		SPayload* pPayload = m_Payloads.front();
		
		HandlePayload(pPayload->Frame);
		SendAck(pPayload->Frame, pPayload->From.pNode, pPayload->From.pChannel);

		m_Payloads.pop_front();
	}

	//TestPending(m_FrameQueue, m_pStats->PendingFrames);

	if((Tick & EPer10Sec) != 0 && m_FIDTrace.size() > 1000)
	{
		for(FIDMap::iterator I = m_FIDTrace.begin(); I != m_FIDTrace.end();)
		{
			if(I->second.Age + MIN2MS(5) < CurTick)
				I = m_FIDTrace.erase(I);
			else
				I++;
		}
	}

	return true;
}

void CRouteSession::Encrypt(CVariant& Data, const CVariant& FID)
{
	if(m_SessionKey)
	{
		CAbstractKey IV(FID.GetData(), FID.GetSize());
		Data.Encrypt(m_SessionKey, &IV);
	}
	else if(m_PublicKey)		
		Data.Encrypt(m_PublicKey);
}

bool CRouteSession::Decrypt(CVariant& Data, const CVariant& FID)
{
	switch(Data.IsEncrypted())
	{
		case CVariant::eAsymmetric:
			return Data.Decrypt(GetParent<CKadRoute>()->GetPrivateKey());
		case CVariant::eSymmetric:
		{
			CAbstractKey IV(FID.GetData(), FID.GetSize());
			if(!m_SessionKey)
				return false;
			return Data.Decrypt(m_SessionKey, &IV);
		}
	}
	return true; // not encrypted
}

void CRouteSession::InitSession(const CVariant& SessionID, bool bSendHandShake)
{
	ASSERT(!m_SessionID.IsValid());
	m_SessionID = SessionID;

	SetLastUpdate(GetCurTick());

	if(bSendHandShake)
		SendHandShake(MakeCryptoReq());
}

void CRouteSession::SendHandShake(const CVariant& KeyPkt)
{
	CPublicKey* pPublicKey = GetParent<CKadRoute>()->GetPublicKey();

	CVariant Handshake;
	if(m_TargetID != GetParent<CKadRoute>()->GetID()) // K-ToDo-Now: send always for indistinguishability?
		Handshake["TID"] = GetParent<CKadRoute>()->GetID();
	Handshake["PK"] = CVariant(pPublicKey->GetKey(), pPublicKey->GetSize());
	if((pPublicKey->GetAlgorithm() & CAbstractKey::eHashFunkt) != 0)
		Handshake["HK"] = CAbstractKey::Algorithm2Str((pPublicKey->GetAlgorithm() & CAbstractKey::eHashFunkt));

	if(KeyPkt.IsValid())
		Handshake["SEC"] = KeyPkt;
	
	m_ConnectionStage = eInit;
	QueueFrame("HS", Handshake, false, true); // handshake must always be signed
}

void CRouteSession::CloseSession()
{
	m_ConnectionStage = eClosing;
	QueueFrame("CS", CVariant());
}

void CRouteSession::QueueFrame(const string &Name, const CVariant& Field, bool bEncrypt, bool bSign)
{
	CVariant FID = m_FIDCounter++;

	CVariant Frame;
	Frame["EID"] = GetParent<CKadRoute>()->GetEntityID();
	Frame["RID"] = m_EntityID;
	// Add target ID fiels if this is an bridged route
	if(m_TargetID != GetParent<CKadRoute>()->GetID())
		Frame["TID"] = m_TargetID;
	Frame["FID"] = FID;

	Frame["SID"] = m_SessionID;

	Frame[Name.c_str()] = Field;
	if(bEncrypt)
		Encrypt(Frame[Name.c_str()], FID);

	SFrame* pFrame = new SFrame(Frame);
	pFrame->bSign = bSign;
	m_FrameQueue.push_back(CScoped<SFrame>()); m_FrameQueue.back() = pFrame;
}

void CRouteSession::ProcessFrame(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel)
{
	ASSERT(m_SessionID.IsValid());

	try
	{
		CKadRoute* pRoute = GetParent<CKadRoute>();

		if(m_FIDTrace[Frame["FID"]].Count++) // make sure already recived FIDs will be ignorred
			throw (const char*)NULL;
		((SSessionStats*)m_pStats.Val())->ProcessedFrames++;

		if(Frame.Has("HS"))
		{
			if(!m_PublicKey)
			{
				const CVariant& Handshake = Frame["HS"];

				const CVariant& KeyValue = Handshake["PK"];
				UINT eHashFunkt = Handshake.Has("HK") ? CAbstractKey::Str2Algorithm(Handshake["HK"]) & CAbstractKey::eHashFunkt : CAbstractKey::eUndefined;
				CScoped<CPublicKey> pKey = new CPublicKey();	
				if(!pKey->SetKey(KeyValue.GetData(), KeyValue.GetSize()))
				{
					LogLine(LOG_ERROR, L"Recived Invalid Entity Key");
					throw "InvalidKey";
				}
			
				CVariant TestID((byte*)NULL, m_EntityID.GetSize());
				CKadID::MakeID(pKey, TestID.GetData(), TestID.GetSize(), eHashFunkt);

				if(TestID != m_EntityID)
				{
					LogLine(LOG_ERROR, L"Recived forged Entity Key");
					throw "InvalidKey";
				}

				m_PublicKey = pKey.Detache();
			}
		}

		if(Frame.IsSigned())
		{
			if(!m_PublicKey)
				throw "UnknownKey";
			if(!Frame.Verify(m_PublicKey))
			{
				LogLine(LOG_ERROR, L"Recived frame with an invalid signature");
				throw "InvalidSign";
			}
		}
		else if(m_SessionKey)
		{
			CHashFunction Hash(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
			Hash.Add(m_SessionKey->GetKey(), m_SessionKey->GetSize());
			if(!Frame.Test(&Hash))
			{
				LogLine(LOG_ERROR, L"Recived frame with an invalid authentication");
				throw "InvalidSign";
			}
		}
	
		SetLastUpdate(GetCurTick());

		if(Frame.Has("HS"))
		{
			if(!Frame.IsSigned()) // Handshake must always be signed
				throw "MissingSign";

			const CVariant& Handshake = Frame["HS"];

			if(Handshake.Has("TID")) // if the handshake has a target ID it means the request came in over a bridge and we should target elseware
				m_TargetID = Handshake["TID"];

			CVariant KeyPkt;
			if(!Handshake.Has("SEC"))
				throw "CryptoRequired";
			
			try
			{
				if(m_SessionKey)
					throw "InitConflict";

				if(!m_KeyExchange) // request
					KeyPkt = HandleCryptoReq(Handshake["SEC"]);
				else // response
					HandleCryptoRes(Handshake["SEC"]);
			}
			catch(const char* Err)
			{
				LogLine(LOG_DEBUG, L"Session %s Broken, Local Crypto Error: %S", ToHex(m_EntityID.GetData(), m_EntityID.GetSize()).c_str(), Err);
				Closed(true);
				throw "CryptoError";
			}

			if(!m_SessionKey)
				throw "KeyFailed";
			//LogLine(LOG_SUCCESS, L"Negotiated on Route key: %s", ToHex(pSession->m_SessionKey->GetKey(), pSession->m_SessionKey->GetSize()).c_str());

			CHashFunction OutIV(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
			OutIV.Add(m_SessionID.GetData(), m_SessionID.GetSize());
			OutIV.Add(m_EntityID.GetData(), m_EntityID.GetSize());
			OutIV.Finish();

			CHashFunction InIV(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
			InIV.Add(m_SessionID.GetData(), m_SessionID.GetSize());
			InIV.Add(pRoute->GetEntityID().GetData(), pRoute->GetEntityID().GetSize());
			InIV.Finish();

			m_CryptoKey = NewSymmetricKey(m_SessionKey->GetAlgorithm(), m_SessionKey, &InIV, &OutIV);

			if(m_ConnectionStage == eNone)
			{
				SendHandShake(KeyPkt);
				pRoute->IncomingSession(this);
			}

			m_ConnectionStage = eOpen;
		}
		else if(m_ConnectionStage == eBroken)
			throw "Broken";
		else if(m_ConnectionStage != eOpen && m_ConnectionStage != eClosing) // accept outstanding packets while closing
			throw "NotConnected";
		else if(Frame.Has("CS"))
		{
			if(m_ConnectionStage != eClosing)
				CloseSession();
			Closed(false);
		}
		else if(IsBussy())
		{
			SPayload* pPayload = new SPayload();
			pPayload->Frame = Frame;
			pPayload->From = SKadNode(pFromNode, pChannel);
			m_Payloads.push_back(CScoped<SPayload>()); m_Payloads.back() = pPayload;
			return;
		}
		else
			HandlePayload(Frame);

		SendAck(Frame, pFromNode, pChannel);
	}
	catch(const char* Err)
	{
		if(!Err) // if Err is Null its just a redundant frame, ignore it and ack as usual
			LogLine(LOG_DEBUG, L"Got redundant frame");
		else
		{
			LogLine(LOG_ERROR, L"NACKing Frame due to: %S", Err);
			SendAck(Frame, pFromNode, pChannel, Err);
		}
	}
}

void CRouteSession::HandlePayload(const CVariant& Frame)
{
	if(Frame.Has("PKT"))
	{
		CVariant Packet = Frame["PKT"];
		if(!Decrypt(Packet, Frame["FID"]))
			throw "CryptoError";
		CVariant Data = Packet["DATA"];
		HandleBytes(Data, false);
	}
	else if(Frame.Has("SEG"))
	{
		CVariant Segment = Frame["SEG"];
		ReassemblyStream(Segment);
	}
}

void CRouteSession::SendAck(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel, const char* pError)
{
	CKadRoute* pRoute = GetParent<CKadRoute>();

	CVariant Ack;
	if(Frame.Has("TID"))
		Ack["TID"] = Frame["TID"];
	Ack["EID"] = Frame["EID"];
	Ack["RID"] = Frame["RID"];
	Ack["FID"] = Frame["FID"];
	Ack["SID"] = Frame["SID"];

	//CVariant Load;
	//Load["PF"] = ;
	//Ack["LOAD"] = Load;

	//bool bSign = !m_SessionKey;

	if(pError)
		Ack["ERR"] = pError;

	/*if(bSign)
		Ack.Sign(pRoute->GetPrivateKey());
	else*/ if(m_SessionKey)
	{
		CHashFunction Hash(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
		Hash.Add(m_SessionKey->GetKey(), m_SessionKey->GetSize());
		Ack.Hash(&Hash);
	}
	pRoute->QueueAck(Ack, pFromNode, pChannel);
}

bool CRouteSession::AckFrame(const CVariant& Ack)
{
	/*if(Ack.IsSigned() && m_PublicKey)
	{
		if(!Ack.Verify(m_PublicKey))
		{
			LogLine(LOG_ERROR, L"Recived ack with an invalid signature");
			return false;
		}
	}
	else*/ if(m_SessionKey) // Note: we wont be verifying the ack for the handshake - as we need to wait the answer to setup the key
	{
		CHashFunction Hash(m_SessionKey->GetAlgorithm() & CAbstractKey::eHashFunkt);
		Hash.Add(m_SessionKey->GetKey(), m_SessionKey->GetSize());
		if(!Ack.Test(&Hash))
		{
			LogLine(LOG_ERROR, L"Recived ack with an invalid authentication");
			return false;
		}
	}

	uint64 CurTick = GetCurTick();
	SetLastUpdate(CurTick);

	//const CVariant& Load = Ack["LOAD"];

	if(Ack.Has("ERR"))
	{
		LogLine(LOG_ERROR, L"Recived NACK, error: %s", Ack.At("ERR").To<wstring>().c_str());
		if(Ack["ERR"] != "InvalidSign") // this error could be caused by a malicius node so we keep the session
		{
			LogLine(LOG_DEBUG, L"Session %s Broken", ToHex(m_EntityID.GetData(), m_EntityID.GetSize()).c_str());
			Closed(true);
		}
	}
	else
	{
		for(TFrameList::iterator J = m_FrameQueue.begin(); J != m_FrameQueue.end(); J++)
		{
			SFrame* pFrame = *J;
			if(pFrame->Frame["FID"] == Ack["FID"])
			{
				if(pFrame->SendTime != 0) // ack may arive after we counted the frame as lost
				{
					FrameRelayed();
					AddSample(CurTick - pFrame->SendTime, pFrame->Frame.GetSize());
					//if(Ack.Has("LOAD"))
					//	UpdateLoad(Ack["LOAD"]);
				}
				m_FrameQueue.erase(J);
				break;
			}
		}
	}
	return true;
}

void CRouteSession::QueueBytes(const CBuffer& Buffer, bool bStream)
{
	if(bStream)
	{
		m_SegmentQueueSize += Buffer.GetSize();

		// split the packet into segments
		for(Buffer.SetPosition(0); Buffer.GetSizeLeft() > 0;)
		{
			size_t ToGo = Min(Buffer.GetSizeLeft(), (size_t)GetParent<CKademlia>()->Cfg()->GetInt("SegmentSize"));
			byte* pData = Buffer.GetData(ToGo);
			if(m_CryptoKey)
				m_CryptoKey->Encrypt(pData, pData, ToGo);
			m_SegmentQueue[m_SendOffset].SetData(0, pData, ToGo);
			m_SendOffset += ToGo;
		}
	}
	else // here we send a peace of bytes out of the stream as a packet
	{
		QueueFrame("PKT", Buffer, true);
	}
}

void CRouteSession::ReassemblyStream(CVariant& Segment)
{
	uint64 Offset = Segment["OFF"];
	m_SegmentBuffer[Offset].SetData(0, Segment["DATA"].GetData(), Segment["DATA"].GetSize());
	
	//map<uint64, CBuffer>::iterator I = m_Receiving.lower_bound(0);
	for(map<uint64, CBuffer>::iterator I = m_SegmentBuffer.begin(); I != m_SegmentBuffer.end(); I = m_SegmentBuffer.erase(I))
	{
		if(I->first > m_RecvOffset)
			break; // we are missign some segment
		if(I->first + I->second.GetSize() <= m_RecvOffset)
			continue; // we have this one already
		if(I->first != m_RecvOffset) // we have an overlaping segment :/
			I->second.SetPosition(m_RecvOffset - I->first);

		uint64 ToGo = I->second.GetSizeLeft();
		byte* pData = I->second.GetData(ToGo);
		if(m_CryptoKey)
			m_CryptoKey->Decrypt(pData, pData, ToGo);
		HandleBytes(CBuffer(pData, ToGo, true), true);
		m_RecvOffset += ToGo;
	}
}

CVariant CRouteSession::MakeCryptoReq(UINT eAlgorithm)
{
	CVariant Request(CVariant::EMap);

	string Param;
	UINT eExAlgorithm = PrepKA(eAlgorithm, Param);
	m_KeyExchange = NewKeyExchange(eExAlgorithm, Param);
	if(eAlgorithm != 0)
		Request["KA"] = CAbstractKey::Algorithm2Str(eExAlgorithm) + "-" + Param;

	CScoped<CAbstractKey> pKey = m_KeyExchange->InitialsieKeyExchange();
	Request["EK"] = CVariant(pKey->GetKey(), pKey->GetSize());

	UINT eSymAlgorithm = PrepSC(eAlgorithm);
	CAbstractKey* pIV = new CAbstractKey(KEY_128BIT, true);
	pIV->SetAlgorithm(eSymAlgorithm);
	if(eAlgorithm)
		Request["SC"] = CAbstractKey::Algorithm2Str(eSymAlgorithm);

	m_KeyExchange->SetAlgorithm(eSymAlgorithm);

	return Request;
}

CVariant CRouteSession::HandleCryptoReq(const CVariant& Request)
{
	CVariant Response(CVariant::EMap);

	if(!Request.Has("EK"))
		throw "MissingEK";

	UINT eExAlgorithm = 0;
	string Param;
	if(Request.Has("KA"))
		eExAlgorithm = ParseKA(Request["KA"], Param);
	UINT eKaAlgorithm = PrepKA(eExAlgorithm, Param);
	CScoped<CKeyExchange> pKeyExchange = NewKeyExchange(eKaAlgorithm, Param);

	CScoped<CAbstractKey> pKey = pKeyExchange->InitialsieKeyExchange();
	Response["EK"] = CVariant(pKey->GetKey(), pKey->GetSize());

	CScoped<CAbstractKey> pRemKey = new CAbstractKey(Request["EK"].GetData(), Request["EK"].GetSize());
	m_SessionKey = pKeyExchange->FinaliseKeyExchange(pRemKey);
	if(!m_SessionKey)
		throw "ExchangeFailed";


	UINT eSymAlgorithm = 0;
	if(Request.Has("SC"))
		eSymAlgorithm = ParseSC(Request["SC"]);
	UINT eAlgorithm = PrepSC(eSymAlgorithm);

	m_SessionKey->SetAlgorithm(eAlgorithm);

	m_KeyExchange = NULL;
	return Response;
}

void CRouteSession::HandleCryptoRes(const CVariant& Response)
{
	if(!Response.Has("EK"))
		throw "MissingEK";
	CScoped<CAbstractKey> pRemKey = new CAbstractKey(Response["EK"].GetData(), Response["EK"].GetSize());
	m_SessionKey = m_KeyExchange->FinaliseKeyExchange(pRemKey);
	if(!m_SessionKey)
		throw "ExchangeFailed";

	m_SessionKey->SetAlgorithm(m_KeyExchange->GetAlgorithm());

	m_KeyExchange = NULL;
}
