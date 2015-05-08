#pragma once

#include "UIntX.h"
#include "../Common/Object.h"
#include "../Common/Pointer.h"
#include "../../Framework/Scope.h"
#include "../Common/Variant.h"
#include "../Networking/SafeAddress.h"
#include "../../Framework/Exception.h"

class CKadNode;
class CRoutingZone;
class CKadLookup;
class CKadPayload;

typedef map<CUInt128, CPointer<CKadNode> >		NodeMap;
typedef vector<CPointer<CKadNode> >				NodeList;
typedef vector<CPointer<CRoutingZone> >			ZoneList;
typedef map<CVariant, CPointer<CKadLookup> >	LookupMap;

enum EFWStatus
{
	eFWOpen,
	eFWNATed,
	eFWClosed
};

#define	NODE_ZERO_CLASS		0
#define	NODE_1ST_CLASS		1
#define	NODE_2ND_CLASS		2
#define	NODE_DEFAULT_CLASS	3
#define	NODE_OFFLINE_CLASS	4


												/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
												//	Firewall Handling																										/
												/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


												//	Sender									|								Reciver	|								Helper	|
												//	----------------------------------------+---------------------------------------+---------------------------------------+
#define FW_CHECK_RQ				"FW:CRQ"		//	FW:CRQ									|										|										|
												//	{										|										|										|
												//	}									-->	|										|										|
#define FW_CHECK_RS				"FW:CRS"		//											|FW:CRS									|										|
												//											|{										|										|
												//											|	ADDR: Seen Address					|										|
												//										<--	|}										|										|
#define FW_TEST_RQ				"FW:TRQ"		//	FW:TRQ									|										|										|
												//	{										|										|										|
												//		ADDR: Address to test				|										|										|
												//	}									-->	|										|										|
#define FW_TEST_REL_RQ			"FW:RQR"		//											|FW:RQR									|										|
												//											|{										|										|
												//											|	ADDR: Address to test				|										|
												//											|}									-->	|										|
#define FW_TEST_REL_ACK			"FW:ACR"		//											|										|FW:ACR									|
												//											|										|{										|
												//											|										|	ADDR: Address to test				|
												//											|										|	(ERR: true)							|
												//											|									<--	|}										|
#define FW_TEST_ACK				"FW:TAC"		//											|FW:TAC									|										| // Note: the node address must be cached, the test adress is not to be used for the relay
												//											|{										|										|
												//											|	ADDR: Address to test				|										|
												//											|	HLPR: Address of helper				|										|
												//											|	(ERR: )								|										|
												//										<--	|}										|										|
#define FW_TEST_RES				"FW:TRS"		//											|										|FW:TRS									|
												//											|										|{										|
												//											|										|	ADDR: Address to test				|
												//										<--	|									<--	|}										|
												//											|										|										|




												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define FW_REQUEST_ASSISTANCE	"FW:ARQ"		//	FW:ARQ									|										|
												//	{										|										|
												//											|										|
												//	}										|										|
												//											|										|
#define FW_ASSISTANCE_RESPONSE	"FW:ARS"		//											|RW:ARS									|
												//											|{										|
												//											|										|
												//											|}										|
												//											|										|


												//	Sender									|								Reciver	|								Helper	|
												//	----------------------------------------+---------------------------------------+---------------------------------------+
#define FW_RELAY_REQUEST		"FW:RRQ"		//	FW:RRQ									|										|										|
												//	{										|										|										|
												//		ADDR: Target Address				|										|										|
												//		NAME: Name							|										|										|
												//		DATA: Packet						|										|										|
												//	}									-->	|									-->	|										|
#define FW_RELAYED_PACKET		"FW:RPK"		//											|										|FW:RPK									|
												//											|										|{										|
												//											|										|	ADDR: Sender Address				|
												//											|										|	NAME: Name							|
												//											|										|	DATA: Packet						|
												//											|									<-- |}										|
//#define FW_RELAY_RESULT		"FW:RRS"		//										<-- |RW:RRS									|										|
												//											|{										|										|
												//											|										|										|
												//											|}										|										|
												//											|										|										|
												//											|										|										|


												//	Sender									|								Reciver	|								Helper	|
												//	----------------------------------------+---------------------------------------+---------------------------------------+
#define FW_TUNNEL_REQUEST		"FW:STUN"		//	FW:STUN									|										|										|
												//	{										|										|										|
												//		ADDR: Target Address				|										|										|
												//		MODE: RDVZ/CLBK						|										|										|
												//	}									-->	|									-->	|										|
												//											|										| * Relay*								|
												//											|									<-- |										|
												//											|										|										|




												/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
												//	Neo Kademlia																											/
												/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_INIT			"KAD:HS"			//	KAD:HS	 								|										|
												//	{										|										|
												//		KAD: Kad Version					|										| (KAD_VERSION)
												//		NID: NodeID							|										|
												//	}										|										|
												//											|KAD:HS	 								|
												//											|{										|
												//											|	KAD: Kad Version					| (KAD_VERSION)
												//											|	NID: NodeID							|
												//											|}										|
												//											|										|

#define KAD_CRYPTO_REQUEST	"KAD:XRQ"			//	KAD:XRQ (signed)						|										|
												//	{										|										|
												//		KA: Key Agreement					|										| // key exchange details
												//		EK: Negotiation Key					|										| // public part
												//		(HK: SH256)							|										| // hash scheme for node ID
												//		(PK: Public Key)					|										| // if we need an authenticated key we authenticate ourselvs first
												//	or										|										| // Note: if we want to encrypt using a old key or negotiate a new one
												//		FP: KeyHash							|										| // fingerprint of the previusly negotiated hash
												//											|										|
												//		(SC: Session Cipher)				|										| // request encryption
												//		(IV: Sender IV)						|										|
												//	}										|										|
#define KAD_CRYPTO_RESPONSE	"KAD:XRS"			//											|KAD:XRS (signed)						|
												//											|{										|
												//											|	EK: Negotiation Key					| // public part
												//											|	(HK: SH256)							| // hash scheme for node ID
												//											|	(PK: Public Key)					| // nodes key if authenticated exchange was required
												//											|										|
												//											|	(IV: Reciver IV)					|
												//											|										|
												//											|or ERR: error							| // reply with error if choosen key is unknown
												//											|}										|
												//											|										|



												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_HELLO_REQUEST	"KAD:HRQ"			//	KAD:HRQ 								|										| // Note: this packet is used as ping to test if nodes are still online
												//	{										|										|
												//		(ADDR: 								|										|
												//		[{									|										|
												//			...								|										|
												//		},...])								|										|
												//	}										|										|
#define KAD_HELLO_RESPONSE	"KAD:HRS"			//											|KAD:HRS								|
												//											|{										|
												//											|	(ADDR: List of Addresses			| // if we have more than one address we send in the hello answer our known auxyliary addresses
												//											|	[{									|
												//											|		IP4/IP6: 						|
												//											|		UTP:  							| // UTP Port - we dont support other transports yet
												//											|		(PSK: Passkey) 					| // never send we always derive it
												//											|		(REL:	// Relay Node			|
												//											|		[{ 								|
												//											|			IP4/IP6:  					|
												//											|			UTP: 						|
												//											|			PSK: 						| // here we dont have the ndoe ID we need the PSK
												//											|		},...]) 						|
												//											|	},...])
												//											|}										|
												//											|										|



												//
												// Node List Request
												//

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_NODE_REQUEST	"KAD:NRQ"			//	KAD:NRQ 								|										|
												//	{										|										|
												//		(TID: TargetID not on bootstrap)	|										|
												//		RCT: Amout of requested nodes		|										|
												//	}										|										|
#define KAD_NODE_RESPONSE	"KAD:NRS"			//											|KAD:NRS								|
												//											|{										|
												//											|	LIST: [ {							|
												//											|		KAD: Kad Version				| (KAD_VERSION)
												//											|		NID: NodeID						|
												//											|		ADDR:							| // See Hello Packet - same format there
												//											|		[{								|
												//											|			...							|
												//											|		}, ...]							|
												//											|	},...]								|
												//											|}										|
												//											|										|



												//
												// Lookup Handling
												//

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_PROXY_REQUEST	"KAD:PRQ"			//	KAD:PRQ 								|										|
												//	{		 								|										|
												//		TID: TargetID						|										|
												//											|										|
												//		(TIMO: timeout)						|										|
												//		(JMPS: randum jump )				|										|
												//		(HOPS: recursion hop limit)			|										|
												//		(RET: Return to)					|										| // if return address is set and JMPS > 0 addr is set to 0 for next relay
												//			ADDR: return Address			|										|
												//			LID: retrun ID					|										|
												//											|										|
												//		(SPRD: Spread count)				|										|
												//		(SHRE: Spread Share count) 			|										|
												//											|										|
												//		LID: LookupID						|										|
												//											|										|
												//		(CID: Code ID)						|										| // Code ID if its a smart lookup
												//		(VER: Minimum Code Version)			|										| // if this version is not available dotn execute and wait for code update
												//											|										|
												//		(AK: Access Key)					|										| // Releaser public key if storring
												//											|										|
												//		TRACE: empty						|										|
												//	}										|										|
#define KAD_PROXY_RESPONSE	"KAD:PRS"			//											|KAD:PRS								|
												//											|{										|
												//											|	(VER: Installed Code Version)		| // 0 for UnknownCode
												//											|										|
												//											|	ERR: Error							| // to "far away"
												//											|										|
												//											|	TRACE: [NID, ...]					|
												//											|}										|
												//											|										|

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_CODE_REQUEST		"KAD:CRQ"		//	KAD:CRQ 								|										|
												//	{		 								|										|
												//		CID: Script ID (Auth Key FP)		|										|
												//	}										|										|
#define KAD_CODE_RESPONSE		"KAD:CRS"		//											|KAD:CRS								|
												//											|{										|
												//											|	AUTH: authenticatio					| // not needed on a refresh
												//											|		PK: PublicKey					|
												//											|		(HK: SH256) 					|
												//											|		SIG: signature					|
												//											|	SRC: Java Script Code				|
												//											|}										|
												//											|										|

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_LOOKUP_MESSAGE		"KAD:LMP"		//	KAD:LMP 								|										| // Note: Lookup messages are not valid with stateless lookup
												//	{		 								|										|
												//		NAME: Lookup Message Name			|										|
												//		DATA: Lookup Message Data			|										|
												//	}										|										|

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_LOOKUP_REPORT		"KAD:LRP"		//											|KAD:LRP 								| // Note: Lookup Reports are text messages used for debug purpose only
												//											|{		 								|
												//											|	NID: NodeID							|
												//											|	TYPE: "ERR/WARN/.."					|
												//											|	TEXT: LogLine						|
												//											|}										|

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_EXECUTE_REQUEST		"KAD:ERQ"		//	KAD:ERQ 								|										|
												//	{		 								|										|
												//		(TID: TargetID)						|										| // Send target ID if this is a stateless lookup request
												//		(CID: Code ID)						|										|
												//		(VER: Minimum Code Version)			|										|
												//											|										| 
												//		RUN:								|										| 
												//		[{									|										|
												//			XID: Transaction ID				|										|
												//			FX: function name to call		|										|
												//			ARG: Arguments					|										|
												//		},...]								|										|
												//	}										|										|
#define KAD_EXECUTE_RESPONSE	"KAD:ERS"		//											|KAD:ERS								|
												//											|{										|
												//											|	(VER: Installed Code Version)		| // 0 for UnknownCode
												//											|	RET:								|
												//											|	[{									|
												//											|		XID: Transaction ID				|
												//											|		RET: Return						|
												//											|		(MORE: true)					| // are there more responses expected
												//											|		(ERR: Error)					| // script crashed
												//											|	},...]								|
												//											|	(ERR: Error)						|
												//											|}										|
												//											|										|

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_STORE_REQUEST		"KAD:SRQ"		//	KAD:SRQ 								|										|
												//	{										|										|
												//		(TID: TargetID)						|										| // Send target ID if this is a stateless lookup request
												//		(AK: Access Key)					|										|
												//											|										|
												//		(TTL: time to live)					|										|
												//											|										|
												//		REQ: 								|										|
												//		[{									|										|
												//			XID: Transaction ID				|										|
												//			PLD: (signed)					|										|
												//			{								|										|
												//				PATH: Payload Path			|										|
												//				DATA: Paylaod Data			|										| // empty payload tag for delete, no payload tag for refresh
												//				RELD: ReleaseDate			|										|
												//			}								|										|
												//		}, ...]								|										|
												//	}										|										|
#define KAD_STORE_RESPONSE		"KAD:SRS"		//											|KAD:SRS								|
												//											|{										|
												//											|	RES:								|
												//											|	[{									|
												//											|		XID: Transaction ID				|
												//											|		EXP: expiration time			| // note: the storring node may chose different expiration times for different entries based on their age
												//											|		(MORE: true)					| // are there more responses expected
												//											|		(ERR: Error)					|
												//											|	}, ...]								|
												//											|	LOAD: ...							|
												//											|	(ERR: Error)						|
												//											|}										|
												//											|										|
												
												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_LOAD_REQUEST		"KAD:LRQ"		//	KAD:LRQ 								|										|
												//	{										|										|
												//		(TID: TargetID)						|										| // Send target ID if this is a stateless lookup request
												//											|										|
												//		(CNT: max result count)				|										|
												//											|										|
												//		REQ:								|										|
												//		[{									|										|
												//			XID: Transaction ID				|										|
												//			PATH: Payload Path				|										|
												//		}, ...]								|										|
												//	}										|										|
#define KAD_LOAD_RESPONSE		"KAD:LRS"		//											|KAD:LRS								|
												//											|{										|
												//											|	RES:								|
												//											|	[{									|
												//											|		XID: Transaction ID				|
												//											|		PLD:							|
												//											|		[{ 								|
												//											|			PATH: Payload Path			|
												//											|			DATA: Paylaod Data			|
												//											|			RELD: ReleaseDate			|
												//											|		},...]							|
												//											|		(MORE: true)					| // are there more responses expected
												//											|		(ERR: Error)					|
												//											|	},...]								|
												//											|	LOAD: ...							|
												//											|	(ERR: Error)						|
												//											|}										|
												//											|										|






												//
												// Routing Handling
												//

												//	Sender									|								Reciver	|
												//	----------------------------------------+---------------------------------------+
#define KAD_ROUTE_REQUEST	"KAD:RRQ"			//	KAD:RRQ 								|										|
												//	{		 								|										|
												//		TID: TargetID						|										|
												//											|										|
												//		(JMPS: random jump )				|										|
												//		(HOPS: recursion hop limit)			|										|
												//											|										|
												//		(BRCH: branche count on every hop)	|										|
												//											|										|
												//		EID: Emiter ID						|										|
												//		PK: PublicKey						|										|
												//		(HK: SH256)							|										|
												//		SM: "PS"/""	SignatureMode			|										| // determins if frames and ACK's must be signed or not
												//											|										|
												//		TRACE: empty						|										|
												//	}										|										|
#define KAD_ROUTE_RESPONSE	"KAD:RRS"			//											|KAD:RRS								|
												//											|{										|
												//											|	LOAD: ...							|
												//											|	(ERR: Error)						|
												//											|										|
												//											|	TRACE: [NID, ...]					|
												//											|}										|
												//											|										|


												//	----------------------------------------+---------------------------------------+
#define KAD_RELAY_REQUEST	"KAD:TRQ"			//	KAD:TRQ 								|										|
												//	{		 								|										|
												//		TTL: time to live					|										|
												//			 								|										|
												//		FRM: (signed)						|										|
												//		{									|										|
												//			(TID: Target ID)				|										|
												//			EID: Emiter ID					|										|
												//			RID: Reciver ID					|										|
												//			FID: Frame ID					|										|
												//			(RC:  ResendCount)				|										|
												//											|										|
												//			SID: Session ID					|										|
												//											|										|
												//			PKT: Packet (encrypted)			|										| // symetric or asymetricencrypted
												//				NAME: Name					|										|
												//				DATA: Packet				|										|
												//		or									|										|
												//			SEG: Stream Segment	(encrypted)	|										| // smytric encrpyted
												//				OFF: Offset					|										|
												//				DATA: Stream Data			|										|
												//		or									|										|
												//			HS: Handshake					|										| // empty handshake is used to obtain entity key
												//				(TID: Target ID)			|										|
												//				PK: PublicKey				|										|
												//				(HK: SH256)					|										|
												//				SEC: ...					|										|
												//		or									|										|
												//			CS: Close Session				|										|
												//		}									|										|
												//											|										|
												//		LOAD: ...							|										|
												//			 								|										|
												//		TRACE: [NID, ...]					|										| // if enabled during route lookup
												//	}										|										|
#define KAD_RELAY_RESPONSE	"KAD:TRS"			//											|KAD:TRS								|
												//											|{										|
												//											|	FRM: (ack/nack betwen hops)			|
												//											|	{									|
												//											|		(TID: Target ID)				|
												//											|		EID: Emiter ID					|
												//											|		RID: Reciver ID					|
												//											|		FID: Frame ID					|
												//											|		(ERR: Error)					|
												//											|	}									|
												//											|										|
												//											|	LOAD: ...							|
												//											|										|
												//											|	TRACE: [NID, ...]					| // if enabled during route lookup
												//											|}										|
												//											|										|
#define KAD_RELAY_RETURN	"KAD:TRT"			//											|KAD:TRT								|
												//											|{										|
												//											|	ACK: (signed)						|
												//											|	{									|
												//											|		(TID: Target ID)				|
												//											|		EID: Emiter ID					|
												//											|		RID: Reciver ID					|
												//											|		FID: Frame ID					|
												//											|		(ERR: Error)					|
												//											|	}									|
												//											|}										|
												//											|										|

												//	----------------------------------------+---------------------------------------+
#define KAD_RELAY_CONTROL	"KAD:MRQ"			//	KAD:MRQ 								|										|
												//	{		 								|										|
												//		CTRL: (signed)						|										|
												//		{									|										|
												//			EID: Emiter ID					|										|
												//			...								|										|
												//		}									|										|
												//	}										|										|
#define KAD_RELAY_STATUS	"KAD:MRS"			//											|KAD:MRS								|
												//											|{										|
												//											|	STAT: (Signed)						|
												//											|	{									|
												//											|		EID: Emiter ID					|
												//											|		(NID: NodeID)					|
												//											|		(ERR: Error)					|
												//											|	}									|
												//											|}										|
