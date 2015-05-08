#pragma once
class CBuffer;
class CFile;
#include "../../FileList/Hashing/FileHash.h"
#include "../../Common/ValueMap.h"

class CMuleHash
{ 
public:
	CMuleHash()							{memset(m_Hash,0,16);}
	CMuleHash(const byte* pHash)		{memcpy(m_Hash,pHash,16);}
	CMuleHash(const CMuleHash& Other)	{memcpy(m_Hash,Other.m_Hash,16);}
	~CMuleHash()						{}

	byte*			GetData()			{return m_Hash;}
	static size_t	GetSize()			{return 16;}
	static int		GetVariantType()	{return m_MyType;}
	QByteArray		ToByteArray()		{return QByteArray((char*)m_Hash, 16);}

private:
    byte			m_Hash[16];
	static	int		m_MyType;
};
Q_DECLARE_METATYPE(CMuleHash);


class CMuleTags
{
public:
	static QVariantMap		ReadTags(const CBuffer* Data, bool bShort = false);
	static void				WriteTags(const QVariantMap& Tags, CBuffer* Data, bool bAllowNew = false, bool bShort = false);

	static void				WriteUInt128(const QByteArray& uUInt128, CBuffer* Data);
	static QByteArray		ReadUInt128(CBuffer* Data);

	static void				WriteHashSet(CFileHashEx* pHashSet, CBuffer* Data);
	static bool				ReadHashSet(const CBuffer* Data, CFileHashEx* pHashSet, QByteArray Hash = QByteArray());

	static void				WriteAICHRecovery(CFileHashTree* pBranche, CBuffer* Data);
	static void				ReadAICHRecovery(const CBuffer* Data, CFileHashTree* pBranche);

	static void				WriteIdentifier(CFile* pFile, CBuffer* Data);
	static QMap<EFileHashType,CFileHashPtr>	ReadIdentifier(const CBuffer* Data, uint64 &uFileSize);

private:
	static uint8			GetMuleTagType(const QVariant& Value);
};

#define TO_NUM(uTag)	QString::number(uTag)
#define FROM_NUM(sTag)	sTag.toUInt()

//////////////////////////////////////////////////////////////////////////////////////////
//

union UMuleVer
{
	UINT	Bits;
	struct SMuleVer
	{
		UINT
		VersionBld	: 7,
		VersionUpd	: 3,
		VersionMin	: 7,
		VersionMjr	: 7,
		Compatible	: 8;
	}		Fields;
};

union UMuleMisc1
{
	UINT	Bits;
	struct SMuleMisc1
	{
		UINT
		SupportsPreview		: 1, // Preview
		MultiPacket			: 1, //	MultiPacket - deprecated with FileIdentifiers/MultipacketExt2, though needed for ExtMultiPacket :/
		NoViewSharedFiles	: 1, //	No 'View Shared Files' supported
		PeerCache			: 1, //	PeerChache supported
		AcceptCommentVer	: 4, // Comments
		ExtendedRequestsVer	: 4, // Ext. Requests
		SourceExchange1Ver	: 4, // Source Exchange - deprecated
		SupportSecIdent		: 4, // Secure Ident
		DataCompVer			: 4, // Data compression version
		UDPPingVer			: 4, // UDP version
		UnicodeSupport		: 1, // Unicode
		SupportsAICH		: 3; // AICH Version (0 = not supported)
	}		Fields;
};

union UMuleMisc2
{
	UINT	Bits;
	struct SMuleMisc2
	{
		UINT
		KadVersion			: 4, // Kad Version - will go up to version 15 only
		SupportsLargeFiles	: 1, // Large Files (includes support for 64bit tags)
		ExtMultiPacket		: 1, // Ext Multipacket (Hash+Size instead of Hash) - deprecated with FileIdentifiers/MultipacketExt2
		ModBit				: 1, // Reserved (ModBit)
		SupportsCryptLayer	: 1, // Supports CryptLayer
		RequestsCryptLayer	: 1, // Requests CryptLayer
		RequiresCryptLayer	: 1, // Requires CryptLayer
		SupportsSourceEx2	: 1, // Supports SourceExachnge2 Packets, ignores SX1 Packet Version
		SupportsCaptcha		: 1, // Supports ChatCaptchas
		DirectUDPCallback	: 1, // Direct UDP Callback supported and available
		SupportsFileIdent	: 1, // Supports new FileIdentifiers/MultipacketExt2
		Reserved			: 18;
	}		Fields;
};

union UMuleMiscN
{
	UINT	Bits;
	struct SMuleMiscN
	{
		UINT
		ExtendedSourceEx	: 1, // Extended Source Exchange with variable source info
		SupportsNatTraversal: 1, // NAT-T Simple Traversal UDP through NATs 
		SupportsIPv6		: 1, // IPv6 Support
		ExtendedComments	: 1,
		Reserved			: 28;
	}		Fields;
};

union UMuleMiscNL
{
	UINT	Bits;
	struct SMuleMiscNL
	{
		UINT
		HostCache			: 1,
		Reserved			: 31;
	}		Fields;
};

union UMuleIdentOpt
{
	uint8	Bits;
	struct SMuleIdentOpt
	{
		uint8
		uIncludesMD4		: 1,
		uIncludesSize		: 1,
		uIncludesAICH		: 1,
		uMandatoryOptions	: 2,
		uOptions			: 3;
	}		Fields;
};

union UMuleIdentReq
{
	uint8	Bits;
	struct SMuleIdentReq
	{
		uint8
		uRequestingMD4		: 1,
		uRequestingAICH		: 1,
		uOptions			: 6;
	}		Fields;
};

union UMuleConOpt
{
	uint8	Bits;
	struct SMuleConOpt
	{
		uint8
		SupportsCryptLayer	: 1, // 1 CryptLayer Supported
		RequestsCryptLayer	: 1, // 1 CryptLayer Requested
		RequiresCryptLayer	: 1, // 1 CryptLayer Required
		DirectUDPCallback	: 1, // 1 DirectCallback Supported/Available 
		Reserved			: 3, // 3 Reserved (!)
		SupportsNatTraversal: 1; // 1 NAT-Traversal Support
	}		Fields;
};


// Comment copyed form eMule (Fair Use):

enum MuleTagTypes 
{
	TAGTYPE_HASH16		= 0x01,
	TAGTYPE_STRING		= 0x02,
	TAGTYPE_UINT32		= 0x03,
	TAGTYPE_FLOAT32		= 0x04,
	TAGTYPE_BOOL		= 0x05,
	TAGTYPE_BOOLARRAY	= 0x06,
	TAGTYPE_BLOB		= 0x07,
	TAGTYPE_UINT16		= 0x08,
	TAGTYPE_UINT8		= 0x09,
	//TAGTYPE_BSOB		= 0x0A, // BSOB is a kad tag like BLOB just with a uint8 length
	TAGTYPE_UINT64		= 0x0B,

	// Compressed string types
	TAGTYPE_STR1		= 0x11,
	TAGTYPE_STR2,
	TAGTYPE_STR3,
	TAGTYPE_STR4,
	TAGTYPE_STR5,
	TAGTYPE_STR6,
	TAGTYPE_STR7,
	TAGTYPE_STR8,
	TAGTYPE_STR9,
	TAGTYPE_STR10,
	TAGTYPE_STR11,
	TAGTYPE_STR12,
	TAGTYPE_STR13,
	TAGTYPE_STR14,
	TAGTYPE_STR15,
	TAGTYPE_STR16,
	TAGTYPE_STR17,
	TAGTYPE_STR18,
	TAGTYPE_STR19,
	TAGTYPE_STR20,
	TAGTYPE_STR21,
	TAGTYPE_STR22
};

enum EMuleHelloTags
{
	CT_NAME					= 0x01,
	CT_PORT					= 0x0F,
	CT_VERSION				= 0x11,

	CT_SERVER_FLAGS			= 0x20,	// currently only used to inform a server about supported features

	CT_EMULE_RESERVED1		= 0xF0,
	CT_EMULE_RESERVED2		= 0xF1,
	CT_EMULE_RESERVED3		= 0xF2,
	CT_EMULE_RESERVED4		= 0xF3,
	CT_EMULE_RESERVED5		= 0xF4,
	CT_EMULE_RESERVED6		= 0xF5,
	CT_EMULE_RESERVED7		= 0xF6,
	CT_EMULE_RESERVED8		= 0xF7,
	CT_EMULE_RESERVED9		= 0xF8,
	CT_EMULE_UDPPORTS		= 0xF9,
	CT_EMULE_MISCOPTIONS1	= 0xFA,
	CT_EMULE_VERSION		= 0xFB,
	CT_EMULE_BUDDYIP		= 0xFC,
	CT_EMULE_BUDDYUDP		= 0xFD,
	CT_EMULE_MISCOPTIONS2	= 0xFE,
	CT_EMULE_RESERVED13		= 0xFF,

	CT_MOD_VERSION			= 0x55,

	CT_EMULECOMPAT_OPTIONS	= 0xEF, // its an amule thing

	CT_EMULE_ADDRESS		= 0xB0, // NEO SX
	CT_EMULE_SERVERIP		= 0xBA, // NEO SX
	CT_EMULE_SERVERTCP		= 0xBB, // NEO SX
	CT_EMULE_CONOPTS		= 0xBE, // NEO SX
	CT_EMULE_BUDDYID		= 0xBF, // NEO SX and addition to Hello

	// Note: as of now non of the 0xA? tags is banned by any major MOD so we can safely use them 
	CT_NEOMULE_RESERVED_0	= 0xA0,
	CT_NEOMULE_RESERVED_1	= 0xA1,
	CT_NEOMULE_RESERVED_2	= 0xA2,
	CT_NEOMULE_RESERVED_3	= 0xA3,
	CT_NEOMULE_RESERVED_4	= 0xA4,
	CT_NEOMULE_RESERVED_5	= 0xA5,
	CT_NEOMULE_RESERVED_6	= 0xA6,
	CT_NEOMULE_RESERVED_7	= 0xA7,
	CT_NEOMULE_RESERVED_8	= 0xA8,
	CT_NEOMULE_RESERVED_9	= 0xA9,
	CT_NEOMULE_MISCOPTIONS	= 0xAA,
	CT_NEOMULE_RESERVED_B	= 0xAB,
	CT_NEOMULE_RESERVED_C	= 0xAC,
	CT_NEOMULE_YOUR_IP 		= 0xAD,
	CT_NEOMULE_IP_V6		= 0xAE,
	CT_NEOMULE_SVR_IP_V6	= 0xAF,
};

#define CT_PROTOCOL_REVISION		"pr"
#define CT_REVISION_HYBRID			1
#define CT_REVISION_NETFWARP		2

#define CT_NEOLOADER_MISCOPTIONS	"nl"

#define	EDONKEYVERSION			0x3c // This is only used to server login. It has no real "version" meaning anymore.

#define KADEMLIA_VERSION5_48a	0x05 // -0.48a
#define KADEMLIA_VERSION7_49a	0x07 // -0.49a needs to support OP_KAD_FWTCPCHECK_ACK, KADEMLIA_FIREWALLED2_REQ
#define KADEMLIA_VERSION		0x09 // Change CT_EMULE_MISCOPTIONS2 if Kadversion becomes >= 15 (0x0F)

// Known protocols
enum EMuleProtocols {
	OP_EDONKEYPROT			= 0xE3,
	OP_PACKEDPROT			= 0xD4,
	OP_EMULEPROT			= 0xC5,

	// Reserved for later UDP headers (important for EncryptedDatagramSocket)	
	OP_UDPRESERVEDPROT1		= 0xA3,
	OP_UDPRESERVEDPROT2		= 0xB2,

	// Kademlia 1/2
	OP_KADEMLIAHEADER		= 0xE4,
	OP_KADEMLIAPACKEDPROT	= 0xE5,
};

// Client <-> Client
enum ED2KStandardClientTCP {	
	OP_HELLO					= 0x01,	// 0x10<HASH 16><ID 4><PORT 2><1 Tag_set>
	OP_SENDINGPART				= 0x46,	// <HASH 16><von 4><bis 4><Daten len:(von-bis)>
	OP_REQUESTPARTS				= 0x47,	// <HASH 16><von[3] 4*3><bis[3] 4*3>
	OP_FILEREQANSNOFIL			= 0x48,	// <HASH 16>
	OP_END_OF_DOWNLOAD     		= 0x49,	// <HASH 16> // Unused for sending
	OP_ASKSHAREDFILES			= 0x4A,	// (null)
	OP_ASKSHAREDFILESANSWER 	= 0x4B,	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
	OP_HELLOANSWER				= 0x4C,	// <HASH 16><ID 4><PORT 2><1 Tag_set><SERVER_IP 4><SERVER_PORT 2>
//	OP_CHANGE_CLIENT_ID 		= 0x4D,	// <ID_old 4><ID_new 4> // Unused for sending
	OP_MESSAGE					= 0x4E,	// <len 2><Message len>
	OP_REQUESTFILESTATUS		= 0x4F,	// <HASH 16>	// OP_SETREQFILEID
	OP_FILESTATUS				= 0x50,	// <HASH 16><count 2><status(bit array) len:((count+7)/8)>
	OP_HASHSETREQUEST			= 0x51,	// <HASH 16>
	OP_HASHSETANSWER			= 0x52,	// <count 2><HASH[count] 16*count>
	OP_STARTUPLOADREQ			= 0x54,	// <HASH 16>
	OP_ACCEPTUPLOADREQ			= 0x55,	// (null)
	OP_CANCELTRANSFER			= 0x56,	// (null)	
	OP_OUTOFPARTREQS			= 0x57,	// (null)
	OP_REQUESTFILENAME			= 0x58,	// <HASH 16>	(more correctly file_name_request)
	OP_REQFILENAMEANSWER		= 0x59,	// <HASH 16><len 4><NAME len>
//	OP_CHANGE_SLOT				= 0x5B,	// <HASH 16> // Not used for sending
	OP_QUEUERANK				= 0x5C,	// <wert  4> (slot index of the request) // Not used for sending
	OP_ASKSHAREDDIRS			= 0x5D,	// (null)
	OP_ASKSHAREDFILESDIR		= 0x5E,	// <len 2><Directory len>
	OP_ASKSHAREDDIRSANS			= 0x5F,	// <count 4>(<len 2><Directory len>)[count]
	OP_ASKSHAREDFILESDIRANS		= 0x60,	// <len 2><Directory len><count 4>(<HASH 16><ID 4><PORT 2><1 T
	OP_ASKSHAREDDENIEDANS		= 0x61,	// (null)

	// eDonkey hybrid op-codes
	OP_HORDESLOTREQ				= 0x65,		// <HASH (file) [16]>
	OP_HORDESLOTREJ				= 0x66,		// <HASH (file) [16]>
	OP_HORDESLOTANS				= 0x67,		// <HASH (file) [16]>

	OP_CRUMBSETREQ				= 0x69,		// <HASH (file) [16]>
	OP_CRUMBSETANS				= 0x68,		// <HASH (file) [16]>													 <HasCrumbSet [1]> <HASH (crumb) [16]>*(CrumbCNT)  for files containing only one part
											// <HASH (file) [16]> <HasPartHashSet [1]> <HASH (part) [16]>*(PartCNT)  <HasCrumbSet [1]> <HASH (crumb) [16]>*(CrumbCNT)  for larger files

	OP_CRUMBCOMPLETE			= 0x6A,		// <HASH (file) [16]> <Crumb [4]>

	//OP_PUBLICIPNOTIFY			= 0x6B		// <IP (receiver) [4]>   Hybrids sends this when IP in Hello packet doesn't match
};

// Extended prot client <-> Extended prot client
enum ED2KExtendedClientTCP {
//	OP_EMULEINFO				= 0x01,	// now done in the hello
//	OP_EMULEINFOANSWER			= 0x02,	// 
	OP_COMPRESSEDPART			= 0x40,	//
	OP_QUEUERANKING				= 0x60,	// <RANG 2>
	OP_FILEDESC					= 0x61,	// 
//	OP_REQUESTSOURCES			= 0x81,	// <HASH 16>
//	OP_ANSWERSOURCES			= 0x82,	//
	OP_REQUESTSOURCES2			= 0x83,	// <HASH 16>
	OP_ANSWERSOURCES2			= 0x84,	//	
	OP_PUBLICKEY				= 0x85,	// <len 1><pubkey len>
	OP_SIGNATURE				= 0x86,	// v1:<len 1><signature len>
										// v2:<len 1><signature len><sigIPused 1>
	OP_SECIDENTSTATE			= 0x87,	// <state 1><rndchallenge 4>
//	OP_REQUESTPREVIEW			= 0x90,	// <HASH 16> // Never used for sending on aMule
//	OP_PREVIEWANSWER			= 0x91,	// <HASH 16><frames 1>{frames * <len 4><frame len>} // Never used for sending on aMule
	OP_MULTIPACKET				= 0x92,
	OP_MULTIPACKETANSWER		= 0x93,
//	OP_PEERCACHE_QUERY			= 0x94,
//	OP_PEERCACHE_ANSWER			= 0x95,
//	OP_PEERCACHE_ACK			= 0x96,
	OP_PUBLICIP_REQ				= 0x97,
	OP_PUBLICIP_ANSWER			= 0x98,
	OP_CALLBACK					= 0x99,	// <HASH 16><HASH 16><uint 16>
	OP_REASKCALLBACKTCP			= 0x9A,
	OP_AICHREQUEST				= 0x9B,	// <HASH 16><uint16><HASH aichhashlen>
	OP_AICHANSWER				= 0x9C,	// <HASH 16><uint16><HASH aichhashlen> <data>
	OP_AICHFILEHASHANS			= 0x9D,	  
	OP_AICHFILEHASHREQ			= 0x9E,
	OP_BUDDYPING				= 0x9F,
	OP_BUDDYPONG				= 0xA0,
	OP_COMPRESSEDPART_I64		= 0xA1,	// <HASH 16><von 8><size 4><Data len:size>
	OP_SENDINGPART_I64			= 0xA2,	// <HASH 16><start 8><end 8><Data len:(end-start)>
	OP_REQUESTPARTS_I64			= 0xA3,	// <HASH 16><start[3] 8*3><end[3] 8*3>
	OP_MULTIPACKET_EXT			= 0xA4,	
//	OP_CHATCAPTCHAREQ			= 0xA5,
//	OP_CHATCAPTCHARES			= 0xA6,
	OP_FWCHECKUDPREQ			= 0xA7,	// <Inter_Port 2><Extern_Port 2><KadUDPKey 4> *Support required for Kadversion >= 6
	OP_KAD_FWTCPCHECK_ACK		= 0xA8,	// (null/reserved), replaces KADEMLIA_FIREWALLED_ACK_RES, *Support required for Kadversion >= 7
	OP_MULTIPACKET_EXT2			= 0xA9,	// <FileIdentifier> ...
	OP_MULTIPACKETANSWER_EXT2	= 0xB0,	// <FileIdentifier> ...
	OP_HASHSETREQUEST2			= 0xB1,	// <FileIdentifier><Options 1>
	OP_HASHSETANSWER2			= 0xB2,	// <FileIdentifier><Options 1>[<HashSets> Options]
};

// Extended prot client <-> Extended prot client UDP
enum ED2KExtendedClientUDP {
	OP_REASKFILEPING			= 0x90,	// <HASH 16>
	OP_REASKACK					= 0x91,	// <RANG 2>
	OP_FILENOTFOUND				= 0x92,	// (null)
	OP_QUEUEFULL				= 0x93,	// (null)
	OP_REASKCALLBACKUDP			= 0x94,
	OP_RENDEZVOUS				= 0xA0,
	OP_HOLEPUNCH				= 0xA1,
};

enum Ed2kUDPOpcodesForKademliaV2 {
	OP_DIRECTCALLBACKREQ		= 0x95	// <TCPPort 2><Userhash 16><ConnectionOptions 1>
};

#define	SOURCEEXCHANGE2_VERSION			4		// replaces the version sent in MISC_OPTIONS flag from SX1
#define	SOURCEEXCHANGEEXT_VERSION		1

#define OLD_MAX_EMULE_FILE_SIZE	((uint64)4290048000) // (4294967295/PARTSIZE)*PARTSIZE = ~4GB

__inline bool IsLargeEd2kMuleFile(uint64 uSize)	{return uSize > OLD_MAX_EMULE_FILE_SIZE;}

// crypto stuff
#define CRYPT_HEADER_WITHOUTPADDING		    8
#define	MAGICVALUE_UDP						91
#define MAGICVALUE_UDP_SYNC_CLIENT			0x395F2EC1
#define MAGICVALUE_UDP_SYNC_SERVER			0x13EF24D5
#define	MAGICVALUE_UDP_SERVERCLIENT			0xA5
#define	MAGICVALUE_UDP_CLIENTSERVER			0x6B

#define CRYPT_HEADER_REQUESTER				12
#define CRYPT_HEADER_SERVER					6
#define	MAGICVALUE_REQUESTER				34							// modification of the requester-send and server-receive key
#define	MAGICVALUE_SERVER					203							// modification of the server-send and requester-send key
#define	MAGICVALUE_SYNC						0x835E6FC4					// value to check if we have a working encrypted stream 



#define IS_UNAVAILABLE			0
#define IS_SIGNATURENEEDED		1
#define IS_KEYANDSIGNEEDED		2

#define CRYPT_CIP_REMOTECLIENT	10
#define CRYPT_CIP_LOCALCLIENT	20
#define CRYPT_CIP_NONECLIENT	30



enum OP_ClientToServerTCP {
	OP_LOGINREQUEST				= 0x01,	// <HASH 16><ID 4><PORT 2><1 Tag_set>
	OP_REJECT					= 0x05,	// (null)
	OP_GETSERVERLIST			= 0x14,	// (null)client->server
	OP_OFFERFILES				= 0x15,	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
	OP_SEARCHREQUEST			= 0x16,	// <Query_Tree>
	OP_DISCONNECT				= 0x18,	// (not verified)
	OP_GETSOURCES				= 0x19,	// <HASH 16>
										// v2 <HASH 16><SIZE_4> (17.3) (mandatory on 17.8)
										// v2large <HASH 16><FILESIZE 4(0)><FILESIZE 8> (17.9) (large files only)
	OP_SEARCH_USER				= 0x1A,	// <Query_Tree>
	OP_CALLBACKREQUEST			= 0x1C,	// <ID 4>
//	OP_QUERY_CHATS				= 0x1D,	// (deprecated, not supported by server any longer)
//	OP_CHAT_MESSAGE				= 0x1E,	// (deprecated, not supported by server any longer)
//	OP_JOIN_ROOM				= 0x1F,	// (deprecated, not supported by server any longer)
	OP_QUERY_MORE_RESULT		= 0x21,	// (null)
	OP_GETSOURCES_OBFU			= 0x23,	
	OP_SERVERLIST				= 0x32,	// <count 1>(<IP 4><PORT 2>)[count] server->client
	OP_SEARCHRESULT				= 0x33,	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
	OP_SERVERSTATUS				= 0x34,	// <USER 4><FILES 4>
	OP_CALLBACKREQUESTED		= 0x35,	// <IP 4><PORT 2>
	OP_CALLBACK_FAIL			= 0x36,	// (null notverified)
	OP_SERVERMESSAGE			= 0x38,	// <len 2><Message len>
//	OP_CHAT_ROOM_REQUEST		= 0x39,	// (deprecated, not supported by server any longer)
//	OP_CHAT_BROADCAST			= 0x3A,	// (deprecated, not supported by server any longer)
//	OP_CHAT_USER_JOIN			= 0x3B,	// (deprecated, not supported by server any longer)
//	OP_CHAT_USER_LEAVE			= 0x3C,	// (deprecated, not supported by server any longer)
//	OP_CHAT_USER				= 0x3D,	// (deprecated, not supported by server any longer)
	OP_IDCHANGE					= 0x40,	// <NEW_ID 4>
	OP_SERVERIDENT				= 0x41,	// <HASH 16><IP 4><PORT 2>{1 TAG_SET}
	OP_FOUNDSOURCES				= 0x42,	// <HASH 16><count 1>(<ID 4><PORT 2>)[count]
	OP_USERS_LIST				= 0x43,	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
	OP_FOUNDSOURCES_OBFU		= 0x44, // <HASH 16><count 1>(<ID 4><PORT 2><obf settings 1>(UserHash16 if obf&0x08))[count]
	OP_NAT_CALLBACKREQUEST		= 0xA6, // <target (B) ID 4><requester (A) ID 4> 
	OP_NAT_CALLBACKREQUESTED_UDP= 0xA7,	// <IP of A 4><UDP PORT of A 2><requester (A) ID>(<obf settings 1><UserHash16>)
	OP_NAT_CALLBACKREQUESTED	= 0x37	// <IP of A 4><UDP PORT of A 2><requester (A) ID>(<obf settings 1><UserHash16>)
};

enum OP_ClientToServerUDP {
	OP_GLOBSEARCHREQ3			= 0x90, // <1 tag set><search_tree>
	OP_GLOBSEARCHREQ2			= 0x92, // <search_tree>
	OP_GLOBGETSOURCES2			= 0x94,	// <HASH 16><FILESIZE 4>
										// largefiles only: <HASH 16><FILESIZE 4(0)><FILESIZE 8> (17.8)		
	OP_GLOBSERVSTATREQ			= 0x96,	// (null)
	OP_GLOBSERVSTATRES			= 0x97,	// <USER 4><FILES 4>
	OP_GLOBSEARCHREQ			= 0x98,	// <search_tree>
	OP_GLOBSEARCHRES			= 0x99,	// 
	OP_GLOBGETSOURCES			= 0x9A,	// <HASH 16>
	OP_GLOBFOUNDSOURCES			= 0x9B,	//
	OP_GLOBCALLBACKREQ			= 0x9C,	// <IP 4><PORT 2><client_ID 4>
	OP_INVALID_LOWID			= 0x9E,	// <ID 4>
	OP_SERVER_LIST_REQ			= 0xA0,	// <IP 4><PORT 2>
	OP_SERVER_LIST_RES			= 0xA1,	// <count 1> (<ip 4><port 2>)[count]
	OP_SERVER_DESC_REQ			= 0xA2,	// (null)
	OP_SERVER_DESC_RES			= 0xA3,	// <name_len 2><name name_len><desc_len 2 desc_en>
	OP_SERVER_LIST_REQ2			= 0xA4	// (null)
};

// Server capabilities, values for CT_SERVER_FLAGS
union USvrFlags
{
	uint32	Bits;
	struct SSvrFlags
	{
		uint32
		uCompression		: 1,	// 0x0001 - SRVCAP_ZLIB
		uIPinLogin			: 1,	// 0x0002 - SRVCAP_IP_IN_LOGIN
		uAuxPort			: 1,	// 0x0004 - SRVCAP_AUXPORT
		uNewTags			: 1,	// 0x0008 - SRVCAP_NEWTAGS
		uUnicode			: 1,	// 0x0010 - SRVCAP_UNICODE
		uReserved_20			: 1,// 0x0020 - 
		uReserved_40			: 1,// 0x0040 - 
		uReserved_80			: 1,// 0x0080 - 
		uLargeFiles			: 1,	// 0x0100 - SRVCAP_LARGEFILES
		uSupportCrypto		: 1,	// 0x0200 - SRVCAP_SUPPORTCRYPT
		uRequestCrypto		: 1,	// 0x0400 - SRVCAP_REQUESTCRYPT
		uRequireCrypto		: 1,	// 0x0800 - SRVCAP_REQUIRECRYPT
		uNatTraversal		: 1,	// 0x1000 - SRVCAP_NATTRAVERSAL
		uIPv6				: 1,	// 0x2000 - SRVCAP_IPV6
		uReserved				: 18;
	}		Fields;
};;

// Server TCP flags
union USvrFlagsTCP
{
	uint32	Bits;
	struct SSvrFlagsTCP
	{
		uint32
		uCompression		: 1,	// 0x00000001 - SRV_TCPFLG_COMPRESSION
		uReserved_2				: 1,// 0x00000002 -  
		uReserved_4				: 1,// 0x00000004 - 
		uNewTags			: 1,	// 0x00000008 - SRV_TCPFLG_NEWTAGS
		uUnicode			: 1,	// 0x00000010 - SRV_TCPFLG_UNICODE
		uReserved_20			: 1,// 0x00000020 - 
		uRelatedSearch		: 1,	// 0x00000040 - SRV_TCPFLG_RELATEDSEARCH
		uTypeTagInteger		: 1,	// 0x00000080 - SRV_TCPFLG_TYPETAGINTEGER
		uLargeFiles			: 1,	// 0x00000100 - SRV_TCPFLG_LARGEFILES
		uReserved_200			: 1,// 0x00000200 - 
		uTcpObfuscation		: 1,	// 0x00000400 - SRV_TCPFLG_TCPOBFUSCATION
		uReserved_800			: 1,// 0x00000800 - 
		uNatTraversal		: 1,	// 0x00001000 - SRV_TCPFLG_NATTRAVERSAL
		uIPv6				: 1,	// 0x00002000 - SRV_TCPFLG_IPV6
		uReserved				: 18;
	}		Fields;
};

// Server UDP flags
union USvrFlagsUDP
{
	uint32	Bits;
	struct SSvrFlagsUDP
	{
		uint32
		uExtGetSources		: 1,	// 0x00000001 - SRV_UDPFLG_EXT_GETSOURCES
		uExtGetFiles		: 1,	// 0x00000002 - SRV_UDPFLG_EXT_GETFILES
		uReserved_4				: 1,// 0x00000004 - 
		uNewTags			: 1,	// 0x00000008 - SRV_UDPFLG_NEWTAGS
		uUnicode			: 1,	// 0x00000010 - SRV_UDPFLG_UNICODE
		uExtGetSources2		: 1,	// 0x00000020 - SRV_UDPFLG_EXT_GETSOURCES2
		uReserved_40			: 1,// 0x00000040 - 
		uReserved_80			: 1,// 0x00000080 - 
		uLargeFiles			: 1,	// 0x00000100 - SRV_UDPFLG_LARGEFILES
		uUdpObfuscation		: 1,	// 0x00000200 - SRV_UDPFLG_UDPOBFUSCATION
		uTcpObfuscation		: 1,	// 0x00000400 - SRV_UDPFLG_TCPOBFUSCATION
		uReserved_800			: 1,// 0x00000800 - 
		uNatTraversal		: 1,	// 0x00001000 - SRV_UDPFLG_NATTRAVERSAL
		uIPv6				: 1,	// 0x00002000 - SRV_UDPFLG_IPV6
		uReserved				: 18;
	}		Fields;
};

#define ST_SERVERNAME			0x01	// <string>
#define ST_DESCRIPTION			0x0B	// <string>
#define	ST_DYNIP				0x85	// <string>
#define	ST_VERSION				0x91	// <string>|<uint32>
#define	ST_AUXPORTSLIST			0x93	// <string>


#define CT_SERVER_UDPSEARCH_FLAGS 0x0E
// values for CT_SERVER_UDPSEARCH_FLAGS
#define SRVCAP_UDP_NEWTAGS_LARGEFILES	0x01
#define SRVCAP_UDP_NEWTAGS_IPv6			0x02

#define MAX_UDP_PACKET_DATA			510
#define MAX_REQUESTS_PER_SERVER		35





//colelction file stuff
#define COLLECTION_FILE_VERSION1_INITIAL		0x01
#define COLLECTION_FILE_VERSION2_LARGEFILES		0x02

#define  FT_FILENAME			 0x01	// <string>
#define  FT_COLLECTIONAUTHOR	 0x31
#define  FT_COLLECTIONAUTHORKEY  0x32

#define  FT_FILESIZE			 0x02	// <uint32> (or <uint64> when supported)
#define  FT_FILESIZE_HI			 0x3A	// <uint32>
#define  FT_FILEHASH			 0x28
#define  FT_AICH_HASH			 0x27
#define  FT_FILETYPE			 0x03	// <string>

#define  FT_FILECOMMENT			 0xF6	// <string>
#define  FT_FILERATING			 0xF7	// <uint8>

#define  FT_GAPSTART			 0x09	// <uint32>
#define  FT_GAPEND				 0x0A	// <uint32>


#define	 FT_FILEFORMAT			 0x04	// <string>
#define  FT_SOURCES				 0x15	// <uint32>
#define	 FT_COMPLETE_SOURCES	 0x30	// nr. of sources which share a complete version of the associated file (supported by eserver 16.46+)

#define	 FT_MEDIA_ARTIST		 0xD0	// <string>
#define	 FT_MEDIA_ALBUM			 0xD1	// <string>
#define	 FT_MEDIA_TITLE			 0xD2	// <string>
#define	 FT_MEDIA_LENGTH		 0xD3	// <uint32> !!!
#define	 FT_MEDIA_BITRATE		 0xD4	// <uint32>
#define	 FT_MEDIA_CODEC			 0xD5	// <string>

#define PARTFILE_VERSION				0xe0
#define PARTFILE_SPLITTEDVERSION		0xe1
#define PARTFILE_VERSION_LARGEFILE		0xe2


// Media values for FT_FILETYPE
#define	ED2KFTSTR_AUDIO			L"Audio"	
#define	ED2KFTSTR_VIDEO			L"Video"	
#define	ED2KFTSTR_IMAGE			L"Image"	
#define	ED2KFTSTR_DOCUMENT		L"Doc"	
#define	ED2KFTSTR_PROGRAM		L"Pro"	
#define	ED2KFTSTR_ARCHIVE		L"Arc"	// *Mule internal use only
#define	ED2KFTSTR_CDIMAGE		L"Iso"	// *Mule internal use only
#define ED2KFTSTR_EMULECOLLECTION	L"EmuleCollection" // Value for eD2K tag FT_FILETYPE
#define ED2KFTSTR_TORRENT		L"Torrent"
