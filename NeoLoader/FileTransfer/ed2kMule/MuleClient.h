#pragma once
//#include "GlobalHeader.h"

#include "../P2PClient.h"
#include "../../../Framework/Buffer.h"
#include "MuleSource.h"
#include "MuleSocket.h"
#include "MuleTags.h"

class CFile;

class CMuleClient : public CP2PClient
{
    Q_OBJECT

public:
	CMuleClient(const SMuleSource& Mule, QObject* qObject = 0);
	CMuleClient(CMuleSocket* pSocket, QObject* qObject = 0);
	~CMuleClient();

	virtual void		Process(UINT Tick);

	virtual QString		GetUrl();

	virtual QString		GetConnectionStr();

	virtual CFile*		GetFile();

	void				SetUserHash(const QByteArray& UserHash);//	{m_Mule.UserHash = UserHash;}
	const TEd2kID&		GetUserHash()						{return m_Mule.UserHash;}

	void				AttacheSource(CMuleSource* pSource);
	void				DettacheSource(CMuleSource* pSource);
	const QMultiMap<QByteArray, CMuleSource*>& GetAllSources()		{return m_AllSources;}
	CMuleSource*		GetUpSource()						{return m_UpSource;}
	CMuleSource*		GetDownSource()						{return m_DownSource;}
	void				SetDownSource(CMuleSource* pSource);

	struct SRequestedBlock
	{
		SRequestedBlock()
		{
			uWriten = 0;

			zStream = NULL;
		}
		uint64			uBegin;
		uint64			uEnd;
		CFile*			pFile;
		uint64			uWriten;

		struct z_stream_s* zStream;
	};
	struct SPendingBlock
	{
		uint64			uBegin;
		uint64			uEnd;
		byte			Hash[16];
	};

	void				SendHelloPacket(bool bAnswer = false);
	void 				SendCrumbComplete(CFile* pFile, uint32 Index);
	void				SendRankingInfo(uint16 uRank);
	void				SendAcceptUpload();
	void				SendOutOfPartReqs();
	void				SendFileRequest(CFile* pFile);
	void				SendUploadRequest();
	void				SendHashSetRequest(CFile* pFile);
	void				SendAICHRequest(CFile* pFile, uint16 uPart);
	void				SendComment(CFile* pFile, QString Description, uint8 Rating);
	void				SendBlockRequest(CFile* pFile, uint64 Begins[3], uint64 Ends[3]);

	void				SendPublicIPRequest();
	void				SendBuddyPing();
	bool				RequestCallback();
	void				RelayKadCallback(const CAddress& Address, uint16 uPort, const QByteArray& BuddyID, const QByteArray& FileID);

	void				RequestFWCheckUDP(uint16 IntPort, uint16 ExtPort, uint32 UDPKey);
	void				SendFWCheckACK();

	bool				SendUDPPingRequest();
	bool				IsUDPPingRequestPending()			{return m_Pending.Ops.UDPPingReq == true;}
	void				ClearUDPPingRequest()				{m_Pending.Ops.UDPPingReq = false;}


	void				CancelUpload();
	void				CancelDownload();
	void				ClearUpload();
	void				ClearDownload();
	
	__inline const QString&	GetNick() const					{return m_NickName;}

	__inline bool		MuleProtSupport() const				{return m_MuleProtocol;}
	__inline int		ProtocolRevision() const			{return m_ProtocolRevision;}

	//SupportsPreview - not supported
	__inline bool		MultiPacket() const					{return m_MiscOptions1.Fields.MultiPacket;}
	//NoViewSharedFiles - not supported
	//PeerCache - not supported
	__inline UINT		AcceptCommentVer() const			{return m_MiscOptions1.Fields.AcceptCommentVer;}
	__inline UINT		ExtendedRequestsVer() const			{return m_MiscOptions1.Fields.ExtendedRequestsVer;}
	//SourceExchange1Ver - deprecated
	//SupportSecIdent - not supported
	__inline UINT		DataCompVer() const					{return m_MiscOptions1.Fields.DataCompVer;}
	__inline UINT		UDPPingVer() const					{return m_MiscOptions1.Fields.UDPPingVer;}
	__inline bool		UnicodeSupport() const				{return m_MiscOptions1.Fields.UnicodeSupport;}
	__inline bool		SupportsAICH() const				{return m_MiscOptions1.Fields.SupportsAICH;}

	__inline UINT		KadVersion() const					{return m_MiscOptions2.Fields.KadVersion;}
	__inline bool		SupportsLargeFiles() const			{return m_MiscOptions2.Fields.SupportsLargeFiles;}
	__inline bool		ExtMultiPacket() const				{return m_MiscOptions2.Fields.ExtMultiPacket;}
	// ModBit - we ain't a mod
	__inline bool		SupportsCryptLayer() const			{return m_Mule.ConOpts.Fields.SupportsCryptLayer;}
	__inline bool		RequestsCryptLayer() const			{return m_Mule.ConOpts.Fields.RequestsCryptLayer;}
	__inline bool		RequiresCryptLayer() const			{return m_Mule.ConOpts.Fields.RequiresCryptLayer;}
	__inline bool		SupportsSourceEx2() const			{return m_MiscOptions2.Fields.SupportsSourceEx2;}
	// SupportsCaptcha - not supported
	__inline bool		DirectUDPCallback() const			{return m_Mule.ConOpts.Fields.DirectUDPCallback;}
	__inline bool		SupportsFileIdent() const			{return m_MiscOptions2.Fields.SupportsFileIdent;}

	__inline bool		ExtendedSourceEx() const			{return m_MiscOptionsN.Fields.ExtendedSourceEx;}
	__inline bool		SupportsNatTraversal() const		{return m_Mule.ConOpts.Fields.SupportsNatTraversal;}
	__inline bool		SupportsIPv6() const				{return m_MiscOptionsN.Fields.SupportsIPv6;}
	__inline bool		SupportsExtendedComments() const	{return m_MiscOptionsN.Fields.ExtendedComments;}
	//__inline UINT		NeoKadVersion() const				{return m_MiscOptionsNL.Fields.NeoKadVersion;}
	__inline bool		SupportsHostCache() const			{return m_MiscOptionsNL.Fields.HostCache != 0;}

	int					IncomingBlockCount()				{return m_IncomingBlocks.size();}

	bool				Connect(bool bUTP = false);
	bool				ConnectSocket(bool bUTP);
	bool				MakeRendezvous();
	void				ExpectConnection();
	bool				IsConnected() const					{return m_Socket && m_bHelloRecived;}
	bool				WasConnected() const				{return m_bHelloRecived;}
	bool				HasTimmedOut() const				{return m_uTimeOut < GetCurTick();}
	void				DisableTimedOut()					{m_uTimeOut = -1;}
	bool				IsDisconnected() const				{return !m_bExpectConnection && !m_Socket;}
	bool				IsExpectingCallback() const			{return m_bExpectConnection;}
	void				Disconnect(const QString& Error = "");
	const SMuleSource&	GetMule()							{return m_Mule;}
	uint16				GetPort() const						{return m_Mule.TCPPort;}
	uint32				GetClientID() const					{return m_Mule.ClientID;}
	bool				IsFirewalled(CAddress::EAF eAF) const;
	uint16				GetUDPPort() const					{return m_Mule.UDPPort;}
	uint16				GetKadPort() const					{return m_Mule.KadPort;}
	const QByteArray&	GetBuddyID() const					{return m_Mule.BuddyID;}
	void				SwapSocket(CMuleClient* pClient);

	uint64				GetSentPartSize()					{return m_SentPartSize;}
	uint64				GetRecivedPartSize()				{return m_RecivedPartSize;}
	//uint64				GetRequestedSize();

	enum EHorde
	{
		eNone,
		eRequested,
		eAccepted,
		eRejected
	};
	EHorde				GetHordeState()						{return m_HordeState;}

	CMuleSocket*		GetSocket()							{return m_Socket;}

	virtual void		AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

	QString				IdentifySoftware(bool);

private slots:
	void				OnConnected();
	void				OnDisconnected(int Error);

	void				OnReceivedPacket(QByteArray Packet, quint8 Prot);

	void				OnDataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux);

	void				OnPacketSend();

signals:
	void				HelloRecived();
	void				SocketClosed();

	void				BytesWritten(qint64 Bytes);
    void				BytesReceived(qint64 Bytes);

protected:
	friend class		CMuleManager;
	friend class		CMuleUDP;
	friend class		CMuleKad;

	void				Init();

	void				ProcessHelloHead(const CBuffer& Packet);
	void				ReadHelloInfo(const CBuffer& Packet);

	void				SendFNF(const CFileHash* pHash);

	void				SendSXAnswer(CFile* pFile, uint8 uXSVersion, uint16 uXSOptions);
	void				ProcessSXAnswer(const CBuffer& Packet);

	void				WriteFileStatus(CBuffer& Packet, CFile* pFile);
	void				ReadFileStatus(const CBuffer& Packet, CMuleSource* pSource);

	void				ProcessBlockRequest(const CBuffer& Packet, bool bI64 = false);
	void				ProcessBlock(const CBuffer& Packet, bool bPacked = false, bool bI64 = false);

	void				SendHordeRequest(CFile* pFile);

	CMuleSource*		GetSource(const CFileHash* pHash, uint64 uSize = 0);
	void				SetUpSource(CMuleSource* pSource);

	void				ProcessPacket(CBuffer& Packet, uint8 Prot);
	void				ProcessUDPPacket(CBuffer& Packet, uint8 Prot);
	void				SendPacket(CBuffer& Packet, uint8 Prot);

	uint64				m_uTimeOut;
	bool				m_bExpectConnection;
	bool				m_bHelloRecived;
	int					m_HolepunchTry;

	QString				m_NickName;
	uint32				m_Ed2kVersion;
	bool				m_MuleProtocol;
	UMuleVer			m_MuleVersion;
	UMuleMisc1			m_MiscOptions1;
	UMuleMisc2			m_MiscOptions2;
	UMuleMiscN			m_MiscOptionsN;
	UMuleMiscNL			m_MiscOptionsNL;
	int					m_ProtocolRevision;
	
	SMuleSource			m_Mule;

	union UPending
	{
		UINT	Bits;
		struct SPending
		{
			UINT
			PublicIPReq		:1,
			UDPPingReq		:1;
		}
		Ops;
	}					m_Pending;

	CMuleSource*		m_UpSource;
	CMuleSource*		m_DownSource;
	QMultiMap<QByteArray, CMuleSource*>	m_AllSources;

	uint64				m_SentPartSize;
	uint64				m_RecivedPartSize;
	//uint64				m_TempPartSize;
	QList<SPendingBlock*>	m_OutgoingBlocks;
	QList<SRequestedBlock*>	m_IncomingBlocks;

	CMuleSocket*		m_Socket;

	EHorde				m_HordeState;
	uint64				m_HordeTimeout;

	// SUI
	void				SendSignature();
	uint32				m_uRndChallenge;
	QByteArray			m_SUIKey;
	//
};
