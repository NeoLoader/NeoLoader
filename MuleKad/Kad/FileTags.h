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

#ifndef FILETAGS_H
#define FILETAGS_H

// ED2K search + known.met + .part.met
#define	FT_FILENAME			0x01	// <string>
#define	FT_FILESIZE			0x02	// <uint32>
#define	FT_FILESIZE_HI			0x3A	// <uint32>
#define	FT_FILETYPE			0x03	// <string> or <uint32>
#define	FT_FILEFORMAT			0x04	// <string>
#define	FT_LASTSEENCOMPLETE		0x05	// <uint32>
#define	FT_TRANSFERRED			0x08	// <uint32>
#define	FT_GAPSTART			0x09	// <uint32>
#define	FT_GAPEND			0x0A	// <uint32>
#define	FT_PARTFILENAME			0x12	// <string>
#define	FT_OLDDLPRIORITY		0x13	// Not used anymore
#define	FT_STATUS			0x14	// <uint32>
#define	FT_SOURCES			0x15	// <uint32>
#define	FT_PERMISSIONS			0x16	// <uint32>
#define	FT_OLDULPRIORITY		0x17	// Not used anymore
#define	FT_DLPRIORITY			0x18	// Was 13
#define	FT_ULPRIORITY			0x19	// Was 17
#define	FT_KADLASTPUBLISHKEY		0x20	// <uint32>
#define	FT_KADLASTPUBLISHSRC		0x21	// <uint32>
#define	FT_FLAGS			0x22	// <uint32>
#define	FT_DL_ACTIVE_TIME		0x23	// <uint32>
#define	FT_CORRUPTEDPARTS		0x24	// <string>
#define	FT_DL_PREVIEW			0x25
#define	FT_KADLASTPUBLISHNOTES		0x26	// <uint32> 
#define	FT_AICH_HASH			0x27
#define	FT_COMPLETE_SOURCES		0x30	// nr. of sources which share a
						// complete version of the
						// associated file (supported
						// by eserver 16.46+) statistic

#define	FT_PUBLISHINFO			0x33	// <uint32>
#define	FT_ATTRANSFERRED		0x50	// <uint32>
#define	FT_ATREQUESTED			0x51	// <uint32>
#define	FT_ATACCEPTED			0x52	// <uint32>
#define	FT_CATEGORY			0x53	// <uint32>
#define	FT_ATTRANSFERREDHI		0x54	// <uint32>
#define	FT_MEDIA_ARTIST			0xD0	// <string>
#define	FT_MEDIA_ALBUM			0xD1	// <string>
#define	FT_MEDIA_TITLE			0xD2	// <string>
#define	FT_MEDIA_LENGTH			0xD3	// <uint32> !!!
#define	FT_MEDIA_BITRATE		0xD4	// <uint32>
#define	FT_MEDIA_CODEC			0xD5	// <string>
#define	FT_FILERATING			0xF7	// <uint8>


// Kad search + some unused tags to mirror the ed2k ones.
#define	TAG_FILENAME			L"\x01"	// <string>
#define	TAG_FILESIZE			L"\x02"	// <uint32>
#define	TAG_FILESIZE_HI			L"\x3A"	// <uint32>
#define	TAG_FILETYPE			L"\x03"	// <string>
#define	TAG_FILEFORMAT			L"\x04"	// <string>
#define	TAG_COLLECTION			L"\x05"
#define	TAG_PART_PATH			L"\x06"	// <string>
#define	TAG_PART_HASH			L"\x07"
#define	TAG_COPIED			L"\x08"	// <uint32>
#define	TAG_GAP_START			L"\x09"	// <uint32>
#define	TAG_GAP_END			L"\x0A"	// <uint32>
#define	TAG_DESCRIPTION			L"\x0B"	// <string>
#define	TAG_PING			L"\x0C"
#define	TAG_FAIL			L"\x0D"
#define	TAG_PREFERENCE			L"\x0E"
#define	TAG_PORT			L"\x0F"
#define	TAG_IP_ADDRESS			L"\x10"
#define	TAG_VERSION			L"\x11"	// <string>
#define	TAG_TEMPFILE			L"\x12"	// <string>
#define	TAG_PRIORITY			L"\x13"	// <uint32>
#define	TAG_STATUS			L"\x14"	// <uint32>
#define	TAG_SOURCES			L"\x15"	// <uint32>
#define	TAG_AVAILABILITY		L"\x15"	// <uint32>
#define	TAG_PERMISSIONS			L"\x16"
#define	TAG_QTIME			L"\x16"
#define	TAG_PARTS			L"\x17"
#define	TAG_PUBLISHINFO			L"\x33"	// <uint32>
#define	TAG_MEDIA_ARTIST		L"\xD0"	// <string>
#define	TAG_MEDIA_ALBUM			L"\xD1"	// <string>
#define	TAG_MEDIA_TITLE			L"\xD2"	// <string>
#define	TAG_MEDIA_LENGTH		L"\xD3"	// <uint32> !!!
#define	TAG_MEDIA_BITRATE		L"\xD4"	// <uint32>
#define	TAG_MEDIA_CODEC			L"\xD5"	// <string>
#define	TAG_KADMISCOPTIONS		L"\xF2"	// <uint8>
#define	TAG_ENCRYPTION			L"\xF3"	// <uint8>
#define	TAG_FILERATING			L"\xF7"	// <uint8>
#define	TAG_BUDDYHASH			L"\xF8"	// <string>
#define	TAG_CLIENTLOWID			L"\xF9"	// <uint32>
#define	TAG_SERVERPORT			L"\xFA"	// <uint16>
#define	TAG_SERVERIP			L"\xFB"	// <uint32>
#define	TAG_SOURCEUPORT			L"\xFC"	// <uint16>
#define	TAG_SOURCEPORT			L"\xFD"	// <uint16>
#define	TAG_SOURCEIP			L"\xFE"	// <uint32>
#define	TAG_SOURCETYPE			L"\xFF"	// <uint8>
#define	TAG_KADAICHHASHPUB		L"\x36"	// <AICH Hash>
#define TAG_KADAICHHASHRESULT	L"\x37"	// <Count 1>{<Publishers 1><AICH Hash> Count}
#define	TAG_KADTORREN			L"btih"	// <Torrent InfoHash>
#define	TAG_KADNEOHASHPUB		L"NH"	// <Neo Hash>
#define	TAG_IPv6				L"ip6"	// Unfirewalled IPv6

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

// Additional media meta data tags from eDonkeyHybrid (note also the uppercase/lowercase)
#define	FT_ED2K_MEDIA_ARTIST		"Artist"	// <string>
#define	FT_ED2K_MEDIA_ALBUM		"Album"		// <string>
#define	FT_ED2K_MEDIA_TITLE		"Title"		// <string>
#define	FT_ED2K_MEDIA_LENGTH		"length"	// <string> !!!
#define	FT_ED2K_MEDIA_BITRATE		"bitrate"	// <uint32>
#define	FT_ED2K_MEDIA_CODEC		"codec"		// <string>

enum EED2KFileType
{
	ED2KFT_ANY				= 0,
	ED2KFT_AUDIO			= 1,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_VIDEO			= 2,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_IMAGE			= 3,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_PROGRAM			= 4,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_DOCUMENT			= 5,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_ARCHIVE			= 6,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_CDIMAGE			= 7,	// ED2K protocol value (eserver 17.6+)
	ED2KFT_EMULECOLLECTION,
	ED2KFT_TORRENT
};

EED2KFileType GetED2KFileTypeID(const wstring& sName);
wstring GetED2KFileTypeSearchTerm(EED2KFileType iFileID);


#endif // FILETAGS_H
