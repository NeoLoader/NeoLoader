#pragma once
//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#include "utils/UInt128.h"
#include "kademlia/Entry.h"
#include "KeywordHelpers.h"

class CUDPSocket;
class CPacket;

namespace Kademlia
{
class CContact;
class CPrefs;
}

#include "Tag.h"

struct SFileInfo
{
	SFileInfo()
	{
		lastPublishTimeKadSrc = 0;
		lastBuddyIP = 0;
		lastPublishTimeKadNotes = 0;
	}
	~SFileInfo() {ClearTags();}

	void ClearTags()
	{
		for(TagPtrList::iterator I = TagList.begin(); I != TagList.end(); I++)
			delete *I;
		TagList.clear();
	}

	Kademlia::CUInt128	FileID;

	wstring				sName;
	uint64_t			uSize;

	bool				bShared;

	TagPtrList			TagList;

	uint8_t				uRating;
	wstring				sComment;

	uint16_t			uCompleteSourcesCount;

	time_t				lastPublishTimeKadSrc;
	uint32_t			lastBuddyIP;
	time_t				lastPublishTimeKadNotes;
};

struct SPendingFWCheck
{
	uint32_t	uIP;
	uint64_t	uTime;
};

struct SQueuedFWCheck
{
	uint32_t	uIP;
	uint16_t	uTCPPort;
	byte		UserHash[16];
	uint8_t		uConOpts;
	bool		bTestUDP;
};

struct SBufferedCallback
{
	uint32_t	uIP;
	uint16_t	uTCPPort;
	byte		BuddyID[16];
	byte		FileID[16];
};

struct SBufferedPacket
{
	uint32_t	uIP;
	uint16_t	uUDPPort;
	QByteArray	Data;
};

struct SPendingCallback
{
	uint32_t	uIP;
	uint16_t	uTCPPort;
	byte		UserHash[16];
	uint8_t		uConOpts;
};

struct SPendingBuddy
{
	uint32_t	uIP;
	uint16_t	uTCPPort;
	uint16_t	uKadPort;
	uint8_t		uConOpts;
	byte		UserHash[16];
	byte		BuddyID[16];
	bool		bIncoming;
};

struct SSearch
{
	enum EType
	{
		eSource,
		eFile,
		eNote
	}		Type;
	struct SSource
	{
		//Kademlia::CUInt128	UserHash;
		Kademlia::CUInt128	BuddyID;
		enum EType
		{
			eOpen,
			eFirewalled,
			eUDPOpen
		}					eType;
		uint32_t			uIP;
		uint16_t			uTCPPort;
		uint16_t			uKADPort;
		uint32_t			uBuddyIP;
		uint16_t			uBuddyPort;
		uint8_t				uCryptOptions;
		Kademlia::CUInt128	IPv6;
	};
	struct SFile
	{
		~SFile()
		{
			for(TagPtrList::iterator I = TagList.begin(); I != TagList.end(); I++)
				delete (*I);
		}
		//Kademlia::CUInt128	FileID;
		wstring				sName;
		uint64_t			uSize;
		wstring				sType;
		TagPtrList			TagList;
		uint32_t			uAvailability;
		uint32_t			uDifferentNames;
		uint32_t			uPublishersKnown;
		uint32_t			uTrustValue;
	};
	struct SNote
	{
		//Kademlia::CUInt128	SourceID;
		wstring				sName;
		uint8_t				uRating;
		wstring				sComment;
	};
	union
	{
		map<Kademlia::CUInt128, SSource*>*	Sources;
		map<Kademlia::CUInt128, SFile*>*	Files;
		map<Kademlia::CUInt128, SNote*>*	Notes;
	}		Result;
	uint64	SearchedSize;
	uint64	Stopped;

	SSearch(EType eType, uint64 uSearchedSize = 0)
	{
		Type = eType;
		switch(Type)
		{
			case eSource:	Result.Sources = new map<Kademlia::CUInt128, SSource*>;	break;
			case eFile:		Result.Files = new map<Kademlia::CUInt128, SFile*>;		break;
			case eNote:		Result.Notes = new map<Kademlia::CUInt128, SNote*>;		break;
		}
		SearchedSize = uSearchedSize;
		Stopped = 0;
	}

	~SSearch()
	{
		switch(Type)
		{
			case eSource:
			{
				for (map<Kademlia::CUInt128, SSource*>::iterator I = Result.Sources->begin(); I != Result.Sources->end(); I++)
					delete I->second;
				delete Result.Sources;
				break;
			}
			case eFile:
			{
				for (map<Kademlia::CUInt128, SFile*>::iterator I = Result.Files->begin(); I != Result.Files->end(); I++)
					delete I->second;
				delete Result.Files;
				break;
			}
			case eNote:
			{
				for (map<Kademlia::CUInt128, SNote*>::iterator I = Result.Notes->begin(); I != Result.Notes->end(); I++)
					delete I->second;
				delete Result.Notes;
				break;
			}
		}
	}
};

class CPublishKeywordList;

class CKadHandler: public QObject
{
	Q_OBJECT

public:
	CKadHandler(const QByteArray& KadID, uint16_t ClientPort, const QByteArray& Ed2kHash);
	~CKadHandler();

	static CKadHandler*	Instance() {return m_Instance;}

	void				SetupProxy(uint16_t UDPPort, QObject* pProxy);
	void				SetupSocket(uint16_t UDPPort, uint32_t UDPKey);

	void				Process();

	const Kademlia::CUInt128&	GetEd2kHash() const			{ASSERT(m_ClientPort); return m_Ed2kHash;}

	void				EnableLanMode(bool bEnable)			{m_LanMode = bEnable;}
	bool				UseLanMode()						{return m_LanMode;}
	void				SetupCryptLayer(bool Supports, bool Requests, bool Requires) {m_SupportsCryptLayer = Supports; m_RequestsCryptLayer = Requests; m_RequiresCryptLayer = Requires;}
	void				SetupNatTraversal(bool Supports)	{m_SupportsNatTraversal = Supports;}
	bool				SupportsCryptLayer()				{return m_SupportsCryptLayer;}
	bool				RequestsCryptLayer()				{return SupportsCryptLayer() && m_RequestsCryptLayer;}
	bool				RequiresCryptLayer()				{return SupportsCryptLayer() && m_RequiresCryptLayer;}
	bool				SupportsNatTraversal()				{return m_SupportsNatTraversal;}

	uint32_t			GetPublicIP();
	bool				IsFirewalled();
	uint16_t			GetTCPPort()						{return m_ClientPort;}
	void				SetPublicIP(uint32_t PublicIP)		{m_PublicIP = PublicIP;}
	void				SetFirewalled(bool Firewalled)		{m_Firewalled = Firewalled;}

	uint32_t			GetKadIP();
	bool				IsFirewalledTCP();
	bool				IsFirewalledUDP();
	uint16_t			GetKadPort(bool bForceInternal = false);

	// IPv6
	bool				HasIPv6()							{return m_IPv6 != 0;}
	void				SetIPv6(const Kademlia::CUInt128& IPv6)	{m_IPv6 = IPv6;}
	const Kademlia::CUInt128&	GetIPv6() const				{return m_IPv6;}
	//

	void				SendPacket(CPacket* packet, uint32_t ip, uint16_t port, bool bEncrypt, const uint8_t* pachTargetClientHashORKadID, bool bKad, uint32_t nReceiverVerifyKey);

	void				SetBuddy(uint32_t BuddyIP, uint16_t BuddyPort) {m_BuddyIP = BuddyIP; m_BuddyPort = BuddyPort;}
	void				ClearBuddy();
	bool				HasBuddy()							{return IsFirewalled() && m_BuddyIP != 0;}
	uint32_t			GetBuddyIP()						{return m_BuddyIP;}
	uint16_t			GetBuddyPort()						{return m_BuddyPort;}

	bool				IsRunning();
	void				Bootstrap(const QHostAddress& Address, uint16 Port);

	void				AddKadFirewallRequest(uint32_t uIP);
	bool				IsKadFirewallCheckIP(uint32_t uIP);

	bool				IssueFirewallCheck(const Kademlia::CContact& Contact, uint8 uConOpts, bool bTestUDP = false);
	void				FWCheckUDPRequested(uint32_t IP, uint16 IntPort, uint16 ExtPort, uint32 UDPKey, bool Biased);
	void				FWCheckACKRecived(uint32_t IP);
	void				SendFWCheckACK(uint32_t IP, uint16 KadPort);

	void				RequestBuddy(Kademlia::CContact* Contact, uint8 uConOpts);
	bool				IncomingBuddy(Kademlia::CContact* Contact, Kademlia::CUInt128* BuddyID);

	void				RelayKadCallback(uint32_t uIP, uint16_t uUDPPort, const Kademlia::CUInt128& buddyID, const Kademlia::CUInt128& fileID);
	bool				RequestKadCallback(const QByteArray& BuddyID, const QByteArray& FileID);
	void				DirectCallback(uint32_t uIP, uint16_t uTCPPort, const QByteArray& UserHash, uint8_t uConOpts);
	void				RelayUDPPacket(uint32_t uIP, uint16_t uUDPPort, byte* pData, size_t uLen);

	list<SPendingFWCheck>*	GetPendingFWChecks()	{return &m_PendingFWChecks;}
	list<SQueuedFWCheck>*	GetQueuedFWChecks()		{return &m_QueuedFWChecks;}
	list<SBufferedCallback>*GetBufferedCallbacks()	{return &m_BufferedCallbacks;}
	list<SBufferedPacket>*	GetBufferedPackets()	{return &m_BufferedPackets;}
	list<SPendingCallback>*	GetPendingCallbacks()	{return &m_PendingCallbacks;}
	list<SPendingBuddy>*	GetPendingBuddys()		{return &m_PendingBuddys;}

	void				AddFile(SFileInfo* File);
	void				RemoveFile(SFileInfo* File);

	SFileInfo*			GetFile(const Kademlia::CUInt128& target);
	SFileInfo*			GetFileByIndex(unsigned int index);
	map<Kademlia::CUInt128, SFileInfo*>& GetFileMap()					{return m_FileList;}

	uint64_t			GetSearchedSize(uint32_t searchID);

	uint32				FindSources(const QByteArray& FileID, uint64 uFileSize, const wstring& sName);
	void				SourceFound(uint32 searchID, const Kademlia::CUInt128* pcontactID, const Kademlia::CUInt128* pbuddyID, uint8 type, uint32 ip, uint16 tcp, uint16 udp, uint32 dwBuddyIP, uint16 dwBuddyPort, uint8 byCryptOptions, const Kademlia::CUInt128* pIPv6);
	bool				GetFoundSources(uint32 searchID, map<Kademlia::CUInt128, SSearch::SSource*>& Sources);

	uint32				FindFiles(const SSearchRoot& SearchRoot, QString& ErrorStr);
	void				FilesFound(uint32_t searchID, const Kademlia::CUInt128 *fileID, const wstring& name, uint64_t size, const wstring& type, uint32_t kadPublishInfo, uint32_t availability, const TagPtrList& taglist);
	bool				GetFoundFiles(uint32 searchID, map<Kademlia::CUInt128, SSearch::SFile*>& Files);

	uint32				FindNotes(const QByteArray& FileID, uint64 uFileSize, const wstring& sName);
	bool				NoteFounds(uint32 searchID, Kademlia::CEntry* entry);
	bool				GetFoundNotes(uint32 searchID, map<Kademlia::CUInt128, SSearch::SNote*>& Notes);

	void				StopSearch(uint32_t searchID);

	void				SearchFinished(uint32_t searchID);

	void				LoadNodes(CBuffer& Buffer);

	bool				IsFiltered(uint32_t ip);
	void				FilterIP(uint32_t ip);

signals:
	void				SendPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool Encrypt, QByteArray TargetKadID, quint32 UDPKey);

public slots:
	void				ProcessPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool validKey, quint32 UDPKey);

protected:
	void				LoadNodes();
	void				SaveNodes();

	Kademlia::CPrefs*	m_KadPrefs;
	uint16_t			m_ClientPort;
	Kademlia::CUInt128	m_Ed2kHash;
	uint16_t			m_UDPPort;

	Kademlia::CUInt128	m_IPv6;

	bool				m_LanMode;
	bool				m_SupportsCryptLayer;
	bool				m_RequestsCryptLayer;
	bool				m_RequiresCryptLayer;
	bool				m_SupportsNatTraversal;

	uint32_t			m_PublicIP;
	bool				m_Firewalled;

	uint32_t			m_BuddyIP;
	uint16_t			m_BuddyPort;

	list<SPendingFWCheck>		m_PendingFWChecks;
	list<SQueuedFWCheck>		m_QueuedFWChecks;
	list<SBufferedCallback>		m_BufferedCallbacks;
	list<SBufferedPacket>		m_BufferedPackets;
	list<SPendingCallback>		m_PendingCallbacks;
	list<SPendingBuddy>			m_PendingBuddys;

	map<Kademlia::CUInt128, SFileInfo*>	m_FileList;

	CPublishKeywordList*m_Keywords;
	int					m_currFileSrc;
	int					m_currFileNotes;
	int					m_currFileKey;
	uint32				m_lastPublishKadSrc;
	uint32				m_lastPublishKadNotes;

	map<uint32_t, SSearch*>				m_Searches;

	CUDPSocket*			m_UDPSocket;

	static CKadHandler*	m_Instance;
};

extern bool g_bLogKad;
void LogKadLine(uint32 uFlag, const wchar_t* sLine, ...);
