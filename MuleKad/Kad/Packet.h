
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

#ifndef PACKET_H
#define PACKET_H

#include "Types.h"		// Needed for int8_t, int32_t, uint8_t and uint32_t

class CBuffer;

//			CLIENT TO SERVER

//			PACKET CLASS
// TODO some parts could need some work to make it more efficient

class CPacket {
public:
	CPacket(CPacket &p);
	CPacket(uint8_t protocol);
	CPacket(byte* header, byte *buf); // only used for receiving packets
	CPacket(const CBuffer& datafile, uint8_t protocol, uint8_t ucOpcode);
	CPacket(int8_t in_opcode, uint32_t in_size, uint8_t protocol, bool bFromPF = true);
	CPacket(byte* pPacketPart, uint32_t nSize, bool bLast, bool bFromPF = true); // only used for splitted packets!

	~CPacket();
	
	byte*			GetHeader();
	byte*			GetUDPHeader();
	byte*			GetPacket();
	byte*			DetachPacket();
	uint32_t 			GetRealPacketSize() const	{ return size + 6; }
	static uint32_t		GetPacketSizeFromHeader(const byte* rawHeader);
	bool			IsSplitted()		{ return m_bSplitted; }
	bool			IsLastSplitted()	{ return m_bLastSplitted; }
	void			PackPacket();
	bool			UnPackPacket(uint32_t uMaxDecompressedSize = 50000);
	// -khaos--+++> Returns either -1, 0 or 1.  -1 is unset, 0 is from complete file, 1 is from part file
	bool			IsFromPF()		{ return m_bFromPF; }
	
	uint8_t			GetOpCode() const	{ return opcode; }
	void			SetOpCode(uint8_t oc)	{ opcode = oc; }
	uint32_t			GetPacketSize() const	{ return size; }
	uint8_t			GetProtocol() const	{ return prot; }
	void			SetProtocol(uint8_t p)	{ prot = p; }
	const byte* 	GetDataBuffer(void) const { return pBuffer; }
	void 			CopyToDataBuffer(unsigned int offset, const byte* data, unsigned int n);
	
private:
	//! CPacket is not assignable.
	CPacket& operator=(const CPacket&);
	
	uint32_t		size;
	uint8_t		opcode;
	uint8_t		prot;
	bool		m_bSplitted;
	bool		m_bLastSplitted;
	bool		m_bPacked;
	bool		m_bFromPF;
	byte		head[6];
	byte*		tempbuffer;
	byte*		completebuffer;
	byte*		pBuffer;
};

#endif // PACKET_H
// File_checked_for_headers
