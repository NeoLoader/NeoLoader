//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

#ifndef PROTOCOLS_H
#define PROTOCOLS_H

// Known protocols
enum Protocols {
	OP_EDONKEYHEADER		= 0xE3,
	OP_EDONKEYPROT			= OP_EDONKEYHEADER,
	OP_PACKEDPROT			= 0xD4,
	OP_EMULEPROT			= 0xC5,

	// Reserved for later UDP headers (important for EncryptedDatagramSocket)	
	OP_UDPRESERVEDPROT1 = 0xA3,
	OP_UDPRESERVEDPROT2 = 0xB2,

	// Kademlia 1/2
	OP_KADEMLIAHEADER		= 0xE4,
	OP_KADEMLIAPACKEDPROT	= 0xE5,
	
	OP_MLDONKEYPROT			= 0x00
};

enum Ed2kUDPOpcodesForKademliaV2 {
	OP_DIRECTCALLBACKREQ		= 0x95	// <TCPPort 2><Userhash 16><ConnectionOptions 1>
};

enum Kademlia2Opcodes {
	KADEMLIA2_BOOTSTRAP_REQ		= 0x01,
	KADEMLIA2_BOOTSTRAP_RES		= 0x09,
	KADEMLIA2_HELLO_REQ		= 0x11,
	KADEMLIA2_HELLO_RES		= 0x19,
	KADEMLIA2_REQ			= 0x21,
	KADEMLIA2_HELLO_RES_ACK		= 0x22,	// <NodeID><uint8 tags>
	KADEMLIA2_RES			= 0x29,
	KADEMLIA2_SEARCH_KEY_REQ	= 0x33,
	KADEMLIA2_SEARCH_SOURCE_REQ	= 0x34,
	KADEMLIA2_SEARCH_NOTES_REQ	= 0x35,
	KADEMLIA2_SEARCH_RES		= 0x3B,
	KADEMLIA2_PUBLISH_KEY_REQ	= 0x43,
	KADEMLIA2_PUBLISH_SOURCE_REQ	= 0x44,
	KADEMLIA2_PUBLISH_NOTES_REQ	= 0x45,
	KADEMLIA2_PUBLISH_RES		= 0x4B,
	KADEMLIA2_PUBLISH_RES_ACK	= 0x4C,	// (null)
	KADEMLIA_FIREWALLED2_REQ	= 0x53,	// <TCPPORT (sender) [2]><userhash><connectoptions 1>
	KADEMLIA2_PING			= 0x60,	// (null)
	KADEMLIA2_PONG			= 0x61,	// (null)
	KADEMLIA2_FIREWALLUDP		= 0x62	// <errorcode [1]><UDPPort_Used [2]>
};

enum KademliaV1OPcodes {
	KADEMLIA_BOOTSTRAP_REQ_DEPRECATED	= 0x00,	// <PEER (sender) [25]>
	KADEMLIA_BOOTSTRAP_RES_DEPRECATED	= 0x08,	// <CNT [2]> <PEER [25]>*(CNT)

	KADEMLIA_HELLO_REQ_DEPRECATED		= 0x10,	// <PEER (sender) [25]>
	KADEMLIA_HELLO_RES_DEPRECATED		= 0x18,	// <PEER (receiver) [25]>

	KADEMLIA_REQ_DEPRECATED			= 0x20,	// <TYPE [1]> <HASH (target) [16]> <HASH (receiver) 16>
	KADEMLIA_RES_DEPRECATED			= 0x28,	// <HASH (target) [16]> <CNT> <PEER [25]>*(CNT)

	KADEMLIA_SEARCH_REQ			= 0x30,	// <HASH (key) [16]> <ext 0/1 [1]> <SEARCH_TREE>[ext]
	// UNUSED				= 0x31,	// Old Opcode, don't use.
	KADEMLIA_SEARCH_NOTES_REQ		= 0x32,	// <HASH (key) [16]>
	KADEMLIA_SEARCH_RES			= 0x38,	// <HASH (key) [16]> <CNT1 [2]> (<HASH (answer) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
	// UNUSED				= 0x39,	// Old Opcode, don't use.
	KADEMLIA_SEARCH_NOTES_RES		= 0x3A,	// <HASH (key) [16]> <CNT1 [2]> (<HASH (answer) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)

	KADEMLIA_PUBLISH_REQ			= 0x40,	// <HASH (key) [16]> <CNT1 [2]> (<HASH (target) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
	// UNUSED				= 0x41,	// Old Opcode, don't use.
	KADEMLIA_PUBLISH_NOTES_REQ_DEPRECATED	= 0x42,	// <HASH (key) [16]> <HASH (target) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
	KADEMLIA_PUBLISH_RES			= 0x48,	// <HASH (key) [16]>
	// UNUSED				= 0x49,	// Old Opcode, don't use.
	KADEMLIA_PUBLISH_NOTES_RES_DEPRECATED	= 0x4A,	// <HASH (key) [16]>

	KADEMLIA_FIREWALLED_REQ			= 0x50,	// <TCPPORT (sender) [2]>
	KADEMLIA_FINDBUDDY_REQ			= 0x51,	// <TCPPORT (sender) [2]>
	KADEMLIA_CALLBACK_REQ			= 0x52,	// <TCPPORT (sender) [2]>
	KADEMLIA_FIREWALLED_RES			= 0x58,	// <IP (sender) [4]>
	KADEMLIA_FIREWALLED_ACK_RES		= 0x59,	// (null)
	KADEMLIA_FINDBUDDY_RES			= 0x5A	// <TCPPORT (sender) [2]>
};

enum ED2KExtendedClientUDP {
	OP_REASKCALLBACKUDP			= 0x94,
};

#define OLD_MAX_EMULE_FILE_SIZE	4290048000ui64	// (4294967295/PARTSIZE)*PARTSIZE = ~4GB

#endif // PROTOCOLS_H
