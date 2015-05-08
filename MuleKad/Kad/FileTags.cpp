//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
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
#include "FileTags.h"
#include "../../Framework/Strings.h"

struct CED2KFileType
{
	map<wstring, EED2KFileType> Exts;
	CED2KFileType()
	{
		Exts[L".aac"]	= ED2KFT_AUDIO;     // Advanced Audio Coding File
		Exts[L".ac3"]	= ED2KFT_AUDIO;     // Audio Codec 3 File
		Exts[L".aif"]	= ED2KFT_AUDIO;     // Audio Interchange File Format
		Exts[L".aifc"]	= ED2KFT_AUDIO;     // Audio Interchange File Format
		Exts[L".aiff"]	= ED2KFT_AUDIO;     // Audio Interchange File Format
		Exts[L".amr"]	= ED2KFT_AUDIO;     // Adaptive Multi-Rate Codec File
		Exts[L".ape"]	= ED2KFT_AUDIO;     // Monkey's Audio Lossless Audio File
		Exts[L".au"]	= ED2KFT_AUDIO;     // Audio File (Sun, Unix)
		Exts[L".aud"]	= ED2KFT_AUDIO;     // General Audio File
		Exts[L".audio"]	= ED2KFT_AUDIO;     // General Audio File
		Exts[L".cda"]	= ED2KFT_AUDIO;     // CD Audio Track
		Exts[L".dmf"]	= ED2KFT_AUDIO;     // Delusion Digital Music File
		Exts[L".dsm"]	= ED2KFT_AUDIO;     // Digital Sound Module
		Exts[L".dts"]	= ED2KFT_AUDIO;     // DTS Encoded Audio File
		Exts[L".far"]	= ED2KFT_AUDIO;     // Farandole Composer Module
		Exts[L".flac"]	= ED2KFT_AUDIO;     // Free Lossless Audio Codec File
		Exts[L".it"]	= ED2KFT_AUDIO;     // Impulse Tracker Module
		Exts[L".m1a"]	= ED2KFT_AUDIO;     // MPEG-1 Audio File
		Exts[L".m2a"]	= ED2KFT_AUDIO;     // MPEG-2 Audio File
		Exts[L".m4a"]	= ED2KFT_AUDIO;     // MPEG-4 Audio File
		Exts[L".mdl"]	= ED2KFT_AUDIO;     // DigiTrakker Module
		Exts[L".med"]	= ED2KFT_AUDIO;     // Amiga MED Sound File
		Exts[L".mid"]	= ED2KFT_AUDIO;     // MIDI File
		Exts[L".midi"]	= ED2KFT_AUDIO;     // MIDI File
		Exts[L".mka"]	= ED2KFT_AUDIO;     // Matroska Audio File
		Exts[L".mod"]	= ED2KFT_AUDIO;     // Amiga Music Module File
		Exts[L".mp1"]	= ED2KFT_AUDIO;     // MPEG-1 Audio File
		Exts[L".mp2"]	= ED2KFT_AUDIO;     // MPEG-2 Audio File
		Exts[L".mp3"]	= ED2KFT_AUDIO;     // MPEG-3 Audio File
		Exts[L".mpa"]	= ED2KFT_AUDIO;     // MPEG Audio File
		Exts[L".mpc"]	= ED2KFT_AUDIO;     // Musepack Compressed Audio File
		Exts[L".mtm"]	= ED2KFT_AUDIO;     // MultiTracker Module
		Exts[L".ogg"]	= ED2KFT_AUDIO;     // Ogg Vorbis Compressed Audio File
		Exts[L".psm"]	= ED2KFT_AUDIO;     // Protracker Studio Module
		Exts[L".ptm"]	= ED2KFT_AUDIO;     // PolyTracker Module
		Exts[L".ra"]	= ED2KFT_AUDIO;     // Real Audio File
		Exts[L".rmi"]	= ED2KFT_AUDIO;     // MIDI File
		Exts[L".s3m"]	= ED2KFT_AUDIO;     // Scream Tracker 3 Module
		Exts[L".snd"]	= ED2KFT_AUDIO;     // Audio File (Sun, Unix)
		Exts[L".stm"]	= ED2KFT_AUDIO;     // Scream Tracker 2 Module
		Exts[L".umx"]	= ED2KFT_AUDIO;     // Unreal Music Package
		Exts[L".wav"]	= ED2KFT_AUDIO;     // WAVE Audio File
		Exts[L".wma"]	= ED2KFT_AUDIO;     // Windows Media Audio File
		Exts[L".xm"]	= ED2KFT_AUDIO;     // Fasttracker 2 Extended Module

		Exts[L".3g2"]	= ED2KFT_VIDEO;     // 3GPP Multimedia File
		Exts[L".3gp"]	= ED2KFT_VIDEO;     // 3GPP Multimedia File
		Exts[L".3gp2"]	= ED2KFT_VIDEO;     // 3GPP Multimedia File
		Exts[L".3gpp"]	= ED2KFT_VIDEO;     // 3GPP Multimedia File
		Exts[L".amv"]	= ED2KFT_VIDEO;     // Anime Music Video File
		Exts[L".asf"]	= ED2KFT_VIDEO;     // Advanced Systems Format File
		Exts[L".avi"]	= ED2KFT_VIDEO;     // Audio Video Interleave File
		Exts[L".bik"]	= ED2KFT_VIDEO;     // BINK Video File
		Exts[L".divx"]	= ED2KFT_VIDEO;     // DivX-Encoded Movie File
		Exts[L".dvr-ms"]= ED2KFT_VIDEO;     // Microsoft Digital Video Recording
		Exts[L".flc"]	= ED2KFT_VIDEO;     // FLIC Video File
		Exts[L".fli"]	= ED2KFT_VIDEO;     // FLIC Video File
		Exts[L".flic"]	= ED2KFT_VIDEO;     // FLIC Video File
		Exts[L".flv"]	= ED2KFT_VIDEO;     // Flash Video File
		Exts[L".hdmov"]	= ED2KFT_VIDEO;     // High-Definition QuickTime Movie
		Exts[L".ifo"]	= ED2KFT_VIDEO;     // DVD-Video Disc Information File
		Exts[L".m1v"]	= ED2KFT_VIDEO;     // MPEG-1 Video File
		Exts[L".m2t"]	= ED2KFT_VIDEO;     // MPEG-2 Video Transport Stream
		Exts[L".m2ts"]	= ED2KFT_VIDEO;     // MPEG-2 Video Transport Stream
		Exts[L".m2v"]	= ED2KFT_VIDEO;     // MPEG-2 Video File
		Exts[L".m4b"]	= ED2KFT_VIDEO;     // MPEG-4 Video File
		Exts[L".m4v"]	= ED2KFT_VIDEO;     // MPEG-4 Video File
		Exts[L".mkv"]	= ED2KFT_VIDEO;     // Matroska Video File
		Exts[L".mov"]	= ED2KFT_VIDEO;     // QuickTime Movie File
		Exts[L".movie"]	= ED2KFT_VIDEO;     // QuickTime Movie File
		Exts[L".mp1v"]	= ED2KFT_VIDEO;     // MPEG-1 Video File        
		Exts[L".mp2v"]	= ED2KFT_VIDEO;     // MPEG-2 Video File
		Exts[L".mp4"]	= ED2KFT_VIDEO;     // MPEG-4 Video File
		Exts[L".mpe"]	= ED2KFT_VIDEO;     // MPEG Video File
		Exts[L".mpeg"]	= ED2KFT_VIDEO;     // MPEG Video File
		Exts[L".mpg"]	= ED2KFT_VIDEO;     // MPEG Video File
		Exts[L".mpv"]	= ED2KFT_VIDEO;     // MPEG Video File
		Exts[L".mpv1"]	= ED2KFT_VIDEO;     // MPEG-1 Video File
		Exts[L".mpv2"]	= ED2KFT_VIDEO;     // MPEG-2 Video File
		Exts[L".ogm"]	= ED2KFT_VIDEO;     // Ogg Media File
		Exts[L".pva"]	= ED2KFT_VIDEO;     // MPEG Video File
		Exts[L".qt"]	= ED2KFT_VIDEO;     // QuickTime Movie
		Exts[L".ram"]	= ED2KFT_VIDEO;     // Real Audio Media
		Exts[L".ratdvd"] = ED2KFT_VIDEO;     // RatDVD Disk Image
		Exts[L".rm"]	= ED2KFT_VIDEO;     // Real Media File
		Exts[L".rmm"]	= ED2KFT_VIDEO;     // Real Media File
		Exts[L".rmvb"]	= ED2KFT_VIDEO;     // Real Video Variable Bit Rate File
		Exts[L".rv"]	= ED2KFT_VIDEO;     // Real Video File
		Exts[L".smil"]	= ED2KFT_VIDEO;     // SMIL Presentation File
		Exts[L".smk"]	= ED2KFT_VIDEO;     // Smacker Compressed Movie File
		Exts[L".swf"]	= ED2KFT_VIDEO;     // Macromedia Flash Movie
		Exts[L".tp"]	= ED2KFT_VIDEO;     // Video Transport Stream File
		Exts[L".ts"]	= ED2KFT_VIDEO;     // Video Transport Stream File
		Exts[L".vid"]	= ED2KFT_VIDEO;     // General Video File
		Exts[L".video"]	= ED2KFT_VIDEO;     // General Video File
		Exts[L".vob"]	= ED2KFT_VIDEO;     // DVD Video Object File
		Exts[L".vp6"]	= ED2KFT_VIDEO;     // TrueMotion VP6 Video File
		Exts[L".wm"]	= ED2KFT_VIDEO;     // Windows Media Video File
		Exts[L".wmv"]	= ED2KFT_VIDEO;     // Windows Media Video File
		Exts[L".xvid"]	= ED2KFT_VIDEO;     // Xvid-Encoded Video File

		Exts[L".bmp"]	= ED2KFT_IMAGE;     // Bitmap Image File
		Exts[L".emf"]	= ED2KFT_IMAGE;     // Enhanced Windows Metafile
		Exts[L".gif"]	= ED2KFT_IMAGE;     // Graphical Interchange Format File
		Exts[L".ico"]	= ED2KFT_IMAGE;     // Icon File
		Exts[L".jfif"]	= ED2KFT_IMAGE;     // JPEG File Interchange Format
		Exts[L".jpe"]	= ED2KFT_IMAGE;     // JPEG Image File
		Exts[L".jpeg"]	= ED2KFT_IMAGE;     // JPEG Image File
		Exts[L".jpg"]	= ED2KFT_IMAGE;     // JPEG Image File
		Exts[L".pct"]	= ED2KFT_IMAGE;     // PICT Picture File
		Exts[L".pcx"]	= ED2KFT_IMAGE;     // Paintbrush Bitmap Image File
		Exts[L".pic"]	= ED2KFT_IMAGE;     // PICT Picture File
		Exts[L".pict"]	= ED2KFT_IMAGE;     // PICT Picture File
		Exts[L".png"]	= ED2KFT_IMAGE;     // Portable Network Graphic
		Exts[L".psd"]	= ED2KFT_IMAGE;     // Photoshop Document
		Exts[L".psp"]	= ED2KFT_IMAGE;     // Paint Shop Pro Image File
		Exts[L".tga"]	= ED2KFT_IMAGE;     // Targa Graphic
		Exts[L".tif"]	= ED2KFT_IMAGE;     // Tagged Image File
		Exts[L".tiff"]	= ED2KFT_IMAGE;     // Tagged Image File
		Exts[L".wmf"]	= ED2KFT_IMAGE;     // Windows Metafile
		Exts[L".wmp"]	= ED2KFT_IMAGE;     // Windows Media Photo File
		Exts[L".xif"]	= ED2KFT_IMAGE;     // ScanSoft Pagis Extended Image Format File

		Exts[L".7z"]	= ED2KFT_ARCHIVE;   // 7-Zip Compressed File
		Exts[L".ace"]	= ED2KFT_ARCHIVE;   // WinAce Compressed File
		Exts[L".alz"]	= ED2KFT_ARCHIVE;   // ALZip Archive
		Exts[L".arc"]	= ED2KFT_ARCHIVE;   // Compressed File Archive
		Exts[L".arj"]	= ED2KFT_ARCHIVE;   // ARJ Compressed File Archive
		Exts[L".bz2"]	= ED2KFT_ARCHIVE;   // Bzip Compressed File
		Exts[L".cab"]	= ED2KFT_ARCHIVE;   // Cabinet File
		Exts[L".cbr"]	= ED2KFT_ARCHIVE;   // Comic Book RAR Archive
		Exts[L".cbz"]	= ED2KFT_ARCHIVE;   // Comic Book ZIP Archive
		Exts[L".gz"]	= ED2KFT_ARCHIVE;   // Gnu Zipped File
		Exts[L".hqx"]	= ED2KFT_ARCHIVE;	// BinHex 4.0 Encoded File
		Exts[L".lha"]	= ED2KFT_ARCHIVE;   // LHARC Compressed Archive
		Exts[L".lzh"]	= ED2KFT_ARCHIVE;   // LZH Compressed File
		Exts[L".msi"]	= ED2KFT_ARCHIVE;   // Microsoft Installer File
		Exts[L".pak"]	= ED2KFT_ARCHIVE;   // PAK (Packed) File
		Exts[L".par"]	= ED2KFT_ARCHIVE;   // Parchive Index File
		Exts[L".par2"]	= ED2KFT_ARCHIVE;   // Parchive 2 Index File
		Exts[L".rar"]	= ED2KFT_ARCHIVE;   // WinRAR Compressed Archive
		Exts[L".sit"]	= ED2KFT_ARCHIVE;   // Stuffit Archive
		Exts[L".sitx"]	= ED2KFT_ARCHIVE;   // Stuffit X Archive
		Exts[L".tar"]	= ED2KFT_ARCHIVE;   // Consolidated Unix File Archive
		Exts[L".tbz2"]	= ED2KFT_ARCHIVE;   // Tar BZip 2 Compressed File
		Exts[L".tgz"]	= ED2KFT_ARCHIVE;   // Gzipped Tar File
		Exts[L".xpi"]	= ED2KFT_ARCHIVE;   // Mozilla Installer Package
		Exts[L".z"]		= ED2KFT_ARCHIVE;   // Unix Compressed File
		Exts[L".zip"]	= ED2KFT_ARCHIVE;   // Zipped File

		Exts[L".bat"]	= ED2KFT_PROGRAM;	// Batch File
		Exts[L".cmd"]	= ED2KFT_PROGRAM;	// Command File
		Exts[L".com"]	= ED2KFT_PROGRAM;	// COM File
		Exts[L".exe"]	= ED2KFT_PROGRAM;	// Executable File
		Exts[L".hta"]	= ED2KFT_PROGRAM;	// HTML Application
		Exts[L".js"]	= ED2KFT_PROGRAM;	// Java Script
		Exts[L".jse"]	= ED2KFT_PROGRAM;	// Encoded  Java Script
		Exts[L".msc"]	= ED2KFT_PROGRAM;	// Microsoft Common Console File
		Exts[L".vbe"]	= ED2KFT_PROGRAM;	// Encoded Visual Basic Script File
		Exts[L".vbs"]	= ED2KFT_PROGRAM;	// Visual Basic Script File
		Exts[L".wsf"]	= ED2KFT_PROGRAM;	// Windows Script File
		Exts[L".wsh"]	= ED2KFT_PROGRAM;	// Windows Scripting Host File

		Exts[L".bin"]	= ED2KFT_CDIMAGE;   // CD Image
		Exts[L".bwa"]	= ED2KFT_CDIMAGE;   // BlindWrite Disk Information File
		Exts[L".bwi"]	= ED2KFT_CDIMAGE;   // BlindWrite CD/DVD Disc Image
		Exts[L".bws"]	= ED2KFT_CDIMAGE;   // BlindWrite Sub Code File
		Exts[L".bwt"]	= ED2KFT_CDIMAGE;   // BlindWrite 4 Disk Image
		Exts[L".ccd"]	= ED2KFT_CDIMAGE;   // CloneCD Disk Image
		Exts[L".cue"]	= ED2KFT_CDIMAGE;   // Cue Sheet File
		Exts[L".dmg"]	= ED2KFT_CDIMAGE;   // Mac OS X Disk Image
		Exts[L".img"]	= ED2KFT_CDIMAGE;   // Disk Image Data File
		Exts[L".iso"]	= ED2KFT_CDIMAGE;   // Disc Image File
		Exts[L".mdf"]	= ED2KFT_CDIMAGE;   // Media Disc Image File
		Exts[L".mds"]	= ED2KFT_CDIMAGE;   // Media Descriptor File
		Exts[L".nrg"]	= ED2KFT_CDIMAGE;   // Nero CD/DVD Image File
		Exts[L".sub"]	= ED2KFT_CDIMAGE;   // Subtitle File
		Exts[L".toast"]	= ED2KFT_CDIMAGE;   // Toast Disc Image

		Exts[L".chm"]	= ED2KFT_DOCUMENT;  // Compiled HTML Help File
		Exts[L".css"]	= ED2KFT_DOCUMENT;  // Cascading Style Sheet
		Exts[L".diz"]	= ED2KFT_DOCUMENT;  // Description in Zip File
		Exts[L".doc"]	= ED2KFT_DOCUMENT;  // Document File
		Exts[L".dot"]	= ED2KFT_DOCUMENT;  // Document Template File
		Exts[L".hlp"]	= ED2KFT_DOCUMENT;  // Help File
		Exts[L".htm"]	= ED2KFT_DOCUMENT;  // HTML File
		Exts[L".html"]	= ED2KFT_DOCUMENT;  // HTML File
		Exts[L".nfo"]	= ED2KFT_DOCUMENT;  // Warez Information File
		Exts[L".pdf"]	= ED2KFT_DOCUMENT;  // Portable Document Format File
		Exts[L".pps"]	= ED2KFT_DOCUMENT;  // PowerPoint Slide Show
		Exts[L".ppt"]	= ED2KFT_DOCUMENT;  // PowerPoint Presentation
		Exts[L".ps"]	= ED2KFT_DOCUMENT;  // PostScript File
		Exts[L".rtf"]	= ED2KFT_DOCUMENT;  // Rich Text Format File
		Exts[L".text"]	= ED2KFT_DOCUMENT;  // General Text File
		Exts[L".txt"]	= ED2KFT_DOCUMENT;  // Text File
		Exts[L".wri"]	= ED2KFT_DOCUMENT;  // Windows Write Document
		Exts[L".xls"]	= ED2KFT_DOCUMENT;  // Microsoft Excel Spreadsheet
		Exts[L".xml"]	= ED2KFT_DOCUMENT;  // XML File

		Exts[L".emulecollection"]	= ED2KFT_EMULECOLLECTION;

		Exts[L".torrent"]	= ED2KFT_TORRENT;
	}
}g_ED2K_Types;

EED2KFileType GetED2KFileTypeID(const wstring& sName)
{
	wstring::size_type Pos = sName.rfind(L'.');
	if (Pos == -1)
		return ED2KFT_ANY;
	wstring Ext = MkLower(sName.substr(Pos));

	map<wstring, EED2KFileType>::iterator I = g_ED2K_Types.Exts.find(Ext);
	if(I == g_ED2K_Types.Exts.end())
		return ED2KFT_ANY;
	return I->second;
}

// Retuns the ed2k file type string ID which is to be used for publishing+searching
wstring GetED2KFileTypeSearchTerm(EED2KFileType iFileID)
{
	if (iFileID == ED2KFT_AUDIO)			return ED2KFTSTR_AUDIO;
	if (iFileID == ED2KFT_VIDEO)			return ED2KFTSTR_VIDEO;
	if (iFileID == ED2KFT_IMAGE)			return ED2KFTSTR_IMAGE;
	if (iFileID == ED2KFT_PROGRAM)			return ED2KFTSTR_PROGRAM;
	if (iFileID == ED2KFT_DOCUMENT)			return ED2KFTSTR_DOCUMENT;
	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	// NOTE: If this gets changed, the function 'GetED2KFileTypeSearchID' also needs to get updated!
	if (iFileID == ED2KFT_ARCHIVE)			return ED2KFTSTR_PROGRAM;
	if (iFileID == ED2KFT_CDIMAGE)			return ED2KFTSTR_PROGRAM;
	if (iFileID == ED2KFT_EMULECOLLECTION)	return ED2KFTSTR_EMULECOLLECTION;
	if (iFileID == ED2KFT_TORRENT)			return ED2KFTSTR_TORRENT;
	return L"";
}
