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
#include "GlobalHeader.h"
#include "../../zlib/zlib.h"

#include "Packet.h"				// Interface declarations
#include "Protocols.h"

#include "../../Framework/Buffer.h"

#if defined(_MSC_VER) || defined(__SUNPRO_CC)
#pragma pack(1)
#endif
struct Header_Struct{
	int8_t	eDonkeyID;
	int32_t	packetlength;
	int8_t	command;
}
#if defined(_MSC_VER) || defined(__SUNPRO_CC)
;
#pragma pack()
#else
__attribute__((__packed__));
#endif

#if defined(_MSC_VER) || defined(__SUNPRO_CC)
#pragma pack(1)
#endif
struct UDP_Header_Struct {
	int8_t	eDonkeyID;
	int8_t	command;
}
#if defined(_MSC_VER) || defined(__SUNPRO_CC)
;
#pragma pack()
#else
__attribute__((__packed__));
#endif


// Copy constructor
CPacket::CPacket(CPacket &p)
{
	size 		= p.size;
	opcode		= p.opcode;
	prot 		= p.prot;
	m_bSplitted 	= p.m_bSplitted;
	m_bLastSplitted = p.m_bLastSplitted;
	m_bPacked 	= p.m_bPacked;
	m_bFromPF 	= p.m_bFromPF;
	memcpy(head, p.head, sizeof head);
	tempbuffer	= NULL;
	if (p.completebuffer) {
		completebuffer 	= new byte[size + 10];;
		pBuffer 	= completebuffer + sizeof(Header_Struct);
	} else {
		completebuffer 	= NULL;
		if (p.pBuffer) {
			pBuffer = new byte[size];
		} else {
			pBuffer = NULL;
		}
	}
	if (pBuffer)
		memcpy( pBuffer, p.pBuffer, size );
}

CPacket::CPacket(uint8_t protocol)
{
	size 		= 0;
	opcode		= 0;
	prot 		= protocol;
	m_bSplitted 	= false;
	m_bLastSplitted = false;
	m_bPacked 	= false;
	m_bFromPF 	= false;
	memset(head, 0, sizeof head);
	tempbuffer	= NULL;
	completebuffer 	= NULL;
	pBuffer 	= NULL;
}

// only used for receiving packets
CPacket::CPacket(byte* rawHeader, byte *buf)
{
	memset(head, 0, sizeof head);
	Header_Struct* header = (Header_Struct*)rawHeader;
	size 		= header->packetlength - 1;
	opcode		= header->command;
	prot		= header->eDonkeyID;
	m_bSplitted 	= false;
	m_bLastSplitted = false;
	m_bPacked 	= false;
	m_bFromPF 	= false;
	tempbuffer	= NULL;
	completebuffer 	= NULL;
	pBuffer 	= buf;
}

CPacket::CPacket(const CBuffer& datafile, uint8_t protocol, uint8_t ucOpcode)
{
	size		= datafile.GetSize();
	opcode		= ucOpcode;
	prot		= protocol;
	m_bSplitted 	= false;
	m_bLastSplitted = false;
	m_bPacked 	= false;
	m_bFromPF 	= false;
	memset(head, 0, sizeof head);
	tempbuffer = NULL;
	completebuffer = new byte[size + sizeof(Header_Struct)/*Why this 4?*/];
	pBuffer = completebuffer + sizeof(Header_Struct);
	
	// Write contents of buffer (while keeping original position in file)
	off_t position = datafile.GetPosition();
	datafile.SetPosition(0);
	memcpy(pBuffer, datafile.ReadData(size), size);
	datafile.SetPosition(position);
}

CPacket::CPacket(int8_t in_opcode, uint32_t in_size, uint8_t protocol, bool bFromPF)
{
	size		= in_size;
	opcode		= in_opcode;
	prot		= protocol;
	m_bSplitted 	= false;
	m_bLastSplitted = false;
	m_bPacked 	= false;
	m_bFromPF	= bFromPF;
	memset(head, 0, sizeof head);
	tempbuffer	= NULL;
	if (in_size) {
		completebuffer = new byte[in_size + sizeof(Header_Struct) + 4 /*Why this 4?*/];
		pBuffer = completebuffer + sizeof(Header_Struct);
		memset(completebuffer, 0, in_size + sizeof(Header_Struct) + 4 /*Why this 4?*/);
	} else {
		completebuffer = NULL;
		pBuffer = NULL;
	}
}

// only used for splitted packets!
CPacket::CPacket(byte* pPacketPart, uint32_t nSize, bool bLast, bool bFromPF)
{
	size		= nSize - sizeof(Header_Struct);
	opcode		= 0;
	prot		= 0;
	m_bSplitted	= true;
	m_bLastSplitted	= bLast;
	m_bPacked	= false;
	m_bFromPF	= bFromPF;
	memset(head, 0, sizeof head);
	tempbuffer	= NULL;
	completebuffer	= pPacketPart;
	pBuffer		= NULL;
}

CPacket::~CPacket()
{
	// Never deletes pBuffer when completebuffer is not NULL
	if (completebuffer) {
		delete [] completebuffer;
	} else if (pBuffer) {
	// On the other hand, if completebuffer is NULL and pBuffer is not NULL 
		delete [] pBuffer;
	}
	
	if (tempbuffer) {
		delete [] tempbuffer;
	}
}

uint32_t CPacket::GetPacketSizeFromHeader(const byte* rawHeader)
{
	Header_Struct* header = (Header_Struct*)rawHeader;
	uint32_t size = header->packetlength;
	if (size < 1 || size >= 0x7ffffff0u)
		return 0;
	return size - 1;
}

void CPacket::CopyToDataBuffer(unsigned int offset, const byte* data, unsigned int n)
{
	ASSERT(offset + n <= size + 1);
	memcpy(pBuffer + offset, data, n);
}

byte* CPacket::GetPacket() {
	if (completebuffer) {
		if (!m_bSplitted) {
			memcpy(completebuffer, GetHeader(), sizeof(Header_Struct));
		}
		return completebuffer;
	} else {
		if (tempbuffer){
			delete [] tempbuffer;
			tempbuffer = NULL;
		}
		tempbuffer = new byte[size + sizeof(Header_Struct) + 4 /* why this 4?*/];
		memcpy(tempbuffer    , GetHeader(), sizeof(Header_Struct));
		memcpy(tempbuffer + sizeof(Header_Struct), pBuffer    , size);
		return tempbuffer;
	}
}

byte* CPacket::DetachPacket() {
	if (completebuffer) {
		if (!m_bSplitted) {
			memcpy(completebuffer, GetHeader(), sizeof(Header_Struct));
		}
		byte* result = completebuffer;
		completebuffer = pBuffer = NULL;
		return result;
	} else{
		if (tempbuffer){
			delete[] tempbuffer;
			tempbuffer = NULL;
		}
		tempbuffer = new byte[size+sizeof(Header_Struct)+4 /* Why this 4?*/];
		memcpy(tempbuffer,GetHeader(),sizeof(Header_Struct));
		memcpy(tempbuffer+sizeof(Header_Struct),pBuffer,size);
		byte* result = tempbuffer;
		tempbuffer = 0;
		return result;
	}
}

byte* CPacket::GetHeader() {
	ASSERT( !m_bSplitted );

	Header_Struct* header = (Header_Struct*) head;
	header->command = opcode;
	header->eDonkeyID =  prot;
	header->packetlength = size + 1;

	return head;
}

byte* CPacket::GetUDPHeader() {
	ASSERT( !m_bSplitted );

	memset(head, 0, 6);
	UDP_Header_Struct* header = (UDP_Header_Struct*) head;
	header->eDonkeyID =  prot;
	header->command = opcode;

	return head;
}


void CPacket::PackPacket()
{
	ASSERT(!m_bSplitted);

	uLongf newsize = size + 300;
	byte* output = new byte[newsize];

	uint16 result = compress2(output, &newsize, pBuffer, size, Z_BEST_COMPRESSION);

	if (result != Z_OK || size <= newsize) {
		delete[] output;
		return;
	}

	if (prot == OP_KADEMLIAHEADER) {
		prot = OP_KADEMLIAPACKEDPROT;
	} else {
		prot = OP_PACKEDPROT;
	}
	
	memcpy(pBuffer, output, newsize);
	delete[] output;
	m_bPacked = true;
	
	size = newsize;
}


bool CPacket::UnPackPacket(uint32_t uMaxDecompressedSize) {
	ASSERT( prot == OP_PACKEDPROT);

	uint32_t nNewSize = size * 10 + 300;

	if (nNewSize > uMaxDecompressedSize){
		nNewSize = uMaxDecompressedSize;
	}

	byte* unpack = new byte[nNewSize];
	uLongf unpackedsize = nNewSize;
	uint16 result = uncompress(unpack, &unpackedsize, pBuffer, size);

	if (result == Z_OK) {
		ASSERT( completebuffer == NULL );
		ASSERT( pBuffer != NULL );

		size = unpackedsize;
		delete[] pBuffer;
		pBuffer = unpack;
		prot = OP_EMULEPROT;
		return true;
	}

	delete[] unpack;
	return false;
}

// File_checked_for_headers
