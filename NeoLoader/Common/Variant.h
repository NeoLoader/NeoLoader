#pragma once

/**************************************************************
*
*	Operation:	[uint8 - NameLen][bytes - Name]
*	Variant:	[uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
*					0 - Extended					Map with Paylaod and extensions
*
*					1 - Map							[uint8 - NameLen][bytes - Name][Variant]
*													[uint8 - NameLen][bytes - Name][Variant]
*													...
*													
*					2 - List						[Variant]
*													[Variant]
*													...
*						
*					7 - ByteArray					[byte n]
*
*					8 - StringUtf8					[byte n]
*													
*					16 - Int						[intn]
*					17 - UInt						[uintn]
*					18 - Double/Float				[double/float]
*
*					32,		// xx0x xxxx			reserved must be 0
*
*							// 00xx xxxx			flag Len uint32 compresses
*					64,		// 01xx xxxx			flag Len uint8
*					128,	// 10xx xxxx			flag Len uint16
*					192,	// 11xx xxxx			flag Len uint32
*
*	Note: On loading from binary index all list items and map items
*
*/

#include "../../Framework/Buffer.h"

class CVariant
{
public:
	enum EType
	{
		EExtended	= 0,	// An extended variant signed or encrypted
		EMap		= 1,	// A Map
		EList		= 2,	// A List
							// 3 4 5 6
		EBytes		= 7,	// Binary BLOB of arbitrary length
							//
		EUtf8		= 8,	// String UTF8 Encoded
		EAscii		= 9,	// Strongm ASCII Encoded
							// 10 11 12 13 14 15
		ESInt		= 16,	// Signed Integer 8 16 32 or 64 bit
		EUInt		= 17,	// unsigned Integer 8 16 32 or 64 bit
		EDouble		= 18,	// Floating point Number 32 or 64 bit precision
							// 19 20 21 22 23 24 25 26 27 28 29 30 31
		//ECustom	= 32,	// 0010 0000
							// 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62
		EInvalid	= 63,	// 0011 1111
							//
		ELen8		= 64,	// 0100 0000
		ELen16		= 128,	// 1000 0000
		ELen32		= 192,	// 1100 0000
	};

	static QVariant			FromPacket(const CBuffer* pPacket);
	static void				ToPacket(const QVariant& Variant, CBuffer* pPacket, bool bPack = false);
};

void MakePacket(const string& Name, const QVariant& Packet, CBuffer& Buffer);
bool StreamPacket(CBuffer& Buffer, string& Name, QVariant& Packet);
