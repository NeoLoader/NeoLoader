#include "GlobalHeader.h"
#include "FileTypes.h"

// Archives and HDD Images
QHash<QString, QString> InitArchiveEXTs();
QHash<QString, QString> InitDiskEXTs();

QHash<QString, QString> InitExecutableEXTs();

// Documents
QHash<QString, QString> InitDocumentsEXTs();
QHash<QString, QString> InitPresentationEXTs();
QHash<QString, QString> InitSpreadsheetEXTs();

// Pictures
QHash<QString, QString> InitImagesEXTs();
QHash<QString, QString> InitVImagesEXTs();
QHash<QString, QString> Init3DImagesEXTs();

// Audio
QHash<QString, QString> InitAudioEXTs();

// Video
QHash<QString, QString> InitVideoEXTs();



QHash<QString, QString> g_ExecutableEXTs;
QHash<QString, QString> g_ArchiveEXTs;
QHash<QString, QString> g_ArchiveDocumentEXTs;
QHash<QString, QString> g_ArchivePictureEXTs;
QHash<QString, QString> g_ArchiveVideoEXTs;
QHash<QString, QString> g_ArchiveAudioEXTs;
bool InitFileTypes()
{
	g_ExecutableEXTs.unite(InitExecutableEXTs());
	g_ArchiveEXTs.unite(InitArchiveEXTs());
	g_ArchiveEXTs.unite(InitDiskEXTs());

	g_ArchiveDocumentEXTs.unite(InitDocumentsEXTs());
	g_ArchiveDocumentEXTs.unite(InitPresentationEXTs());
	g_ArchiveDocumentEXTs.unite(InitSpreadsheetEXTs());

	g_ArchivePictureEXTs.unite(InitImagesEXTs());
	g_ArchivePictureEXTs.unite(InitVImagesEXTs());
	g_ArchivePictureEXTs.unite(Init3DImagesEXTs());

	g_ArchiveVideoEXTs.unite(InitVideoEXTs());

	g_ArchiveAudioEXTs.unite(InitAudioEXTs());

	return 0;
}
bool g_FileTypes_init_ = InitFileTypes();

bool IsExecutableExt(const QString& Ext)
{
	return g_ExecutableEXTs.contains(Ext);
}
bool IsArchiveExt(const QString& Ext)
{
	return g_ArchiveEXTs.contains(Ext);
}
bool IsDocumentExt(const QString& Ext)
{
	return g_ArchiveDocumentEXTs.contains(Ext);
}
bool IsPictureExt(const QString& Ext)
{
	return g_ArchivePictureEXTs.contains(Ext);
}
bool IsVideoExt(const QString& Ext)
{
	return g_ArchiveVideoEXTs.contains(Ext);
}
bool IsAudioExt(const QString& Ext)
{
	return g_ArchiveAudioEXTs.contains(Ext);
}

EFileTypes GetFileTypeByExt(const QString& Ext)
{
	EFileTypes Type = eUnknownExt;
	if(IsVideoExt(Ext))				Type = eVideoExt;
	else if(IsAudioExt(Ext))		Type = eAudioExt;
	else if(IsPictureExt(Ext))		Type = ePictureExt;
	else if(IsDocumentExt(Ext))		Type = eDocumentExt;
	else if(IsArchiveExt(Ext))		Type = eArchiveExt;
	else if(IsExecutableExt(Ext))	Type = eProgramExt;
	return Type;
}

/* Generator: http://en.wikipedia.org/wiki/List_of_file_formats

	QStringList Lines = Text.split("\n");
	Text.clear();

	foreach(QString Line, Lines)
	{
		Line = Line.replace(QRegExp("[ \t\r\n]+"), " ").trimmed();
		if(Line.isEmpty())
			continue;

		int IndexOf = Line.indexOf("-");
		if(IndexOf == -1)
			IndexOf = Line.size();

		QString Ext = Line.left(IndexOf).trimmed();
		if(Ext.left(1) == ".")
			Ext.remove(0, 1);
		QString Descr = Line.mid(IndexOf+1).trimmed();
		Descr.replace("\"","\'");
		if(Descr.isEmpty())
			Descr = Ext;

		Text += "\tExtMap[\"" + Ext.toLower() + "\"] = \"" + Descr + "\";\r\n";
	}
*/

////////////////////////////////////////////////////////////////////////////
// Archives and HDD Images
//

QHash<QString, QString> InitArchiveEXTs()
{
	QHash<QString, QString> ExtMap;
	//ExtMap["?Q?"] = "files compressed by the SQ program";
	ExtMap["7z"] = "7-Zip compressed file";
	ExtMap["aac"] = "Advanced Audio Coding";
	ExtMap["ace"] = "ACE compressed file";
	ExtMap["alz"] = "ALZip compressed file";
	ExtMap["at3"] = "Sony's UMD Data compression";
	ExtMap["bke"] = "BackupEarth.com Data compression";
	ExtMap["arc"] = "ARC";
	ExtMap["arj"] = "ARJ compressed file";
	ExtMap["ba"] = "Scifer Archive (.ba), Scifer External Archive Type";
	ExtMap["big"] = "Special file compression format used by Electronic Arts for compressing the data for many of EA's games";
	ExtMap["bik"] = "Bink Video file. A video compression system developed by RAD Game Tools";
	ExtMap["bin"] = "compressed Archive. can be read and used by cd-roms and java, Extractable by 7-zip and WINRAR";
	ExtMap["bkf"] = "Microsoft backup created by NTBACKUP.EXE";
	ExtMap["bzip2"] = "bz2";
	ExtMap["bz2"] = "bz2";
	ExtMap["bld"] = "Skyscraper Simulator Building";
	ExtMap["c4"] = "JEDMICS image files, a DOD system";
	ExtMap["cab"] = "Microsoft Cabinet";
	ExtMap["cals"] = "JEDMICS image files, a DOD system";
	ExtMap["cpt"] = "Compact Pro (Macintosh)";
	ExtMap["sea"] = "Compact Pro (Macintosh)";
	ExtMap["daa"] = "Closed-format, Windows-only compressed disk image";
	ExtMap["dmg"] = "an Apple compressed/encrypted format";
	ExtMap["ddz"] = "a file which can only be used by the 'daydreamer engine' created by 'fever-dreamer', a program similar to RAGS, it's mainly used to make somewhat short games.";
	ExtMap["dpe"] = "Package of AVE documents made with Aquafadas digital publishing tools.";
	ExtMap["egg"] = "Alzip Egg Edition compressed file";
	ExtMap["egt"] = "EGT Universal Document also used to create compressed cabinet files replaces .ecab";
	ExtMap["ecab"] = "EGT Compressed Folder used in advanced systems to compress entire system folders, replaced by EGT Universal Document";
	ExtMap["ezip"] = "EGT Compressed Folder used in advanced systems to compress entire system folders, replaced by EGT Universal Document";
	ExtMap["ess"] = "EGT SmartSense File, detects files compressed using the EGT compression system.";
	ExtMap["gho"] = "Norton Ghost";
	ExtMap["ghs"] = "Norton Ghost";
	ExtMap["gzip"] = "Compressed file";
	ExtMap["gz"] = "Compressed file";
	ExtMap["ipg"] = "Format in which Apple Inc. packages their iPod games. can be extracted through Winrar";
	ExtMap["lbr"] = "Lawrence Compiler Type file";
	ExtMap["lbr"] = "Library file";
	ExtMap["lqr"] = "LBR Library file compressed by the SQ program.";
	ExtMap["lha"] = "Lempel, Ziv, Huffman";
	ExtMap["lzh"] = "Lempel, Ziv, Huffman";
	ExtMap["lzip"] = "Compressed file";
	ExtMap["lz"] = "Compressed file";
	ExtMap["lzo"] = "lzo";
	ExtMap["lzma"] = "lzma";
	ExtMap["lzx"] = "LZX (algorithm)";
	ExtMap["mbw"] = "MBRWizard archive (.mbw)";
	ExtMap["mpq"] = "Used by Blizzard games";
	ExtMap["nth"] = "Nokia Theme Used by Nokia Series 40 Cellphones";
	ExtMap["pak"] = "Enhanced type of .ARC archive";
	ExtMap["par"] = "Parchive";
	ExtMap["par2"] = "Parchive";
	ExtMap["pyk"] = "PYK (.pyk) Compressed File";
	ExtMap["rar"] = "RAR Rar Archive, for multiple file archive (rar to .r01-.r99 to s01 and so on)";
	ExtMap["rpm"] = "Red Hat package/installer for Fedora, RHEL, and similar systems.";
	ExtMap["sen"] = "Scifer Internal Archive Type";
	ExtMap["sit"] = "StuffIt (Macintosh)";
	ExtMap["sitx"] = "StuffIt (Macintosh)";
	ExtMap["skb"] = "Google Sketchup Backup File";
	ExtMap["tar"] = "group of files, packaged as one file";
	ExtMap["tar.gz"] = "gzipped tar file";
	ExtMap["tgz"] = "gzipped tar file";
	ExtMap["tb"] = "Tabbery Virtual Desktop Tab file";
	ExtMap["uha"] = "Ultra High Archive Compression";
	ExtMap["uue"] = "user unifile element";
	ExtMap["viv"] = "Archive format used to compress data for several video games, including Need For Speed: High Stakes.";
	ExtMap["vol"] = "unknown archive";
	ExtMap["wax"] = "Wavexpress - A ZIP alternative optimized for packages containing video, allowing multiple packaged files to be all-or-none delivered with near-instantaneous unpacking via NTFS file system manipulation.";
	ExtMap["tar.xz"] = "an xz compressed tar file. (Based on LZMA2.)";
	ExtMap["z"] = "Unix compress file";
	ExtMap["zoo"] = "based on LZW";
	ExtMap["zip"] = "popular compression format";

	return ExtMap;
}

QHash<QString, QString> InitDiskEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["tib"] = "Acronis True Image backup";
	ExtMap["iso"] = "The generic file format for most optical media, including CD-ROM, DVD-ROM, Blu-ray Disc, HD DVD and UMD.";
	ExtMap["nrg"] = "The proprietary optical media archive format used by Nero applications.";
	ExtMap["img"] = "For archiving MS-DOS formatted floppy disks.";
	ExtMap["adf"] = "Amiga Disk Format, for archiving Amiga floppy disks";
	ExtMap["adz"] = "The GZip-compressed version of ADF.";
	ExtMap["dms"] = "Disk Masher System, a disk-archiving system native to the Amiga.";
	ExtMap["dsk"] = "For archiving floppy disks from a number of other platforms, including the ZX Spectrum and Amstrad CPC.";
	ExtMap["d64"] = "An archive of a Commodore 64 floppy disk.";
	ExtMap["sdi"] = "System Deployment Image, used for archiving and providing 'virtual disk' functionality.";
	ExtMap["mds"] = "DAEMON tools native disc image file format used for making images from optical CD-ROM, DVD-ROM, HD DVD or Blu-ray Disc. It comes together with MDF file and can be mounted with DAEMON Tools.";
	ExtMap["mdx"] = "New DAEMON Tools file format that allows to get one MDX disc image file instead of two (MDF and MDS).";
	ExtMap["dmg"] = "Macintosh disk image files";
	ExtMap["cdi"] = "DiscJuggler image file";
	ExtMap["cue"] = "CDRWrite CUE image file";
	ExtMap["cif"] = "Easy CD Creator .cif format";
	ExtMap["c2d"] = "Roxio / WinOnCD .c2d format";
	ExtMap["daa"] = "PowerISO .daa format";
	ExtMap["ccd"] = "CloneCD image file";
	ExtMap["sub"] = "CloneCD image file";
	ExtMap["img"] = "CloneCD image file";
	return ExtMap;
}

QHash<QString, QString> InitExecutableEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["exe"] = "Applications for windows";
	ExtMap["apk"] = "Applications installable on Android";
	ExtMap["deb"] = "Debian install package";
	ExtMap["vsa"] = "Altiris Virtual Software Archive";
	ExtMap["paf"] = "Portable Application File";
	ExtMap["jar"] = "ZIP file with manifest for use with Java applications.";
	return ExtMap;
}

////////////////////////////////////////////////////////////////////////////
// Documents
//

QHash<QString, QString> InitDocumentsEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["602"] = "Text602 document";
	ExtMap["abw"] = "AbiWord Document";
	ExtMap["acl"] = "MS Word AutoCorrect List";
	ExtMap["afp"] = "Advanced Function Presentation - IBc";
	ExtMap["ami"] = "Lostus Ami Pro";
	ExtMap["ans"] = "American National Standards Institute (ANSI) text";
	ExtMap["asc"] = "ASCII text";
	ExtMap["aww"] = "Ability Write";
	ExtMap["ccf"] = "Color Chat 1.0";
	ExtMap["csv"] = "ASCII text as comma-separated values, used in spreadsheets and database management systems";
	ExtMap["cwk"] = "ClarisWorks / AppleWorks document";
	ExtMap["dbk"] = "DocBook XML sub-format";
	ExtMap["doc"] = "Microsoft Word document";
	ExtMap["docm"] = "Microsoft Word for Mac document";
	ExtMap["docx"] = "Office Open XML document";
	ExtMap["dot"] = "Microsoft Word document template";
	ExtMap["dotx"] = "Office Open XML text document template";
	ExtMap["egt"] = "EGT Universal Document";
	ExtMap["epub"] = "EPUB open standard for e-books";
	ExtMap["ezw"] = "Reagency Systems easyOFFER document[2]";
	ExtMap["fdx"] = "Final Návrh";
	ExtMap["ftm"] = "Fielded Text Meta";
	ExtMap["ftx"] = "Fielded Text (Declared)";
	ExtMap["gdoc"] = "Google Drive Document";
	ExtMap["html"] = "HyperText Markup Language (.html, .htm)";
	ExtMap["hwp"] = "Haansoft (Hancom) Hangul Word Processor document";
	ExtMap["hwpml"] = "Haansoft (Hancom) Hangul Word Processor Markup Language document";
	ExtMap["log"] = "Text log file";
	ExtMap["lwp"] = "Lotus Word Pro";
	ExtMap["mbp"] = "metadata for Mobipocket documents";
	ExtMap["md"] = "Markdown text document";
	ExtMap["mcw"] = "Microsoft Word for Macintosh (versions 4.0–5.1)";
	ExtMap["mobi"] = "Mobipocket documents";
	ExtMap["nb"] = "Mathematica Notebook";
	ExtMap["nbp"] = "Mathematica Player Notebook";
	ExtMap["odm"] = "OpenDocument master document";
	ExtMap["odt"] = "OpenDocument text document";
	ExtMap["ott"] = "OpenDocument text document template";
	ExtMap["omm"] = "OmmWriter text document";
	ExtMap["pages"] = "Apple Pages document";
	ExtMap["pap"] = "Papyrus word processor document";
	ExtMap["pdax"] = "Portable Document Archive (PDA) document index file";
	ExtMap["pdf"] = "Portable Document Format";
	ExtMap["rtf"] = "Rich Text document";
	ExtMap["quox"] = "Question Object File Format for Quobject Designer or Quobject Explorer";
	ExtMap["rpt"] = "Crystal Reports";
	ExtMap["sdw"] = "StarWriter text document, used in earlier versions of StarOffice";
	ExtMap["se"] = "Shuttle Document";
	ExtMap["stw"] = "OpenOffice.org XML (obsolete) text document template";
	ExtMap["sxw"] = "OpenOffice.org XML (obsolete) text document";
	ExtMap["tex"] = "TeX";
	ExtMap["info"] = "Texinfo";
	ExtMap["troff"] = "Troff";
	ExtMap["txt"] = "ASCII nebo Unicode plaintext Text file";
	ExtMap["uof"] = "Uniform Office Format";
	ExtMap["uoml"] = "Unique Object Markup Language";
	ExtMap["via"] = "Revoware VIA Document Project File";
	ExtMap["wpd"] = "WordPerfect document";
	ExtMap["wps"] = "Microsoft Works document";
	ExtMap["wpt"] = "Microsoft Works document template";
	ExtMap["wrd"] = "WordIt! document";
	ExtMap["wrf"] = "ThinkFree Write";
	ExtMap["wri"] = "Microsoft Write document";
	ExtMap["xhtml"] = "eXtensible Hyper-Text Markup Language";
	ExtMap["xml"] = "eXtensible Markup Language";
	ExtMap["xps"] = "Open XML Paper Specification";
	return ExtMap;
}


QHash<QString, QString> InitPresentationEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["gslides"] = "Google Drive Presentation";
	ExtMap["key"] = "Apple Keynote Presentation";
	ExtMap["keynote"] = "Apple Keynote Presentation";
	ExtMap["nb"] = "Mathematica Slideshow";
	ExtMap["nbp"] = "Mathematica Player slideshow";
	ExtMap["odp"] = "OpenDocument Presentation";
	ExtMap["otp"] = "OpenDocument Presentation template";
	ExtMap["pez"] = "Prezi Desktop Presentation";
	ExtMap["pot"] = "Microsoft PowerPoint template";
	ExtMap["pps"] = "Microsoft PowerPoint Show";
	ExtMap["ppt"] = "Microsoft PowerPoint Presentation";
	ExtMap["pptx"] = "Office Open XML Presentation";
	ExtMap["prz"] = "Lotus Freelance Graphics";
	ExtMap["sdd"] = "StarOffice's StarImpress";
	ExtMap["shf"] = "ThinkFree Show";
	ExtMap["show"] = "Haansoft(Hancom) Presentation software document";
	ExtMap["shw"] = "Corel Presentations slide show creation";
	ExtMap["slp"] = "Logix-4D Manager Show Control Project";
	ExtMap["sspss"] = "SongShow Plus Slide Show";
	ExtMap["sti"] = "OpenOffice.org XML (obsolete) Presentation template";
	ExtMap["sxi"] = "OpenOffice.org XML (obsolete) Presentation";
	ExtMap["thmx"] = "Microsoft PowerPoint theme template";
	ExtMap["watch"] = "Dataton Watchout Presentation";
	return ExtMap;
}

QHash<QString, QString> InitSpreadsheetEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["123"] = "Lotus 1-2-3";
	ExtMap["ab2"] = "Abykus worksheet";
	ExtMap["ab3"] = "Abykus workbook";
	ExtMap["aws"] = "Ability Spreadsheet";
	ExtMap["clf"] = "ThinkFree Calc";
	ExtMap["cell"] = "Haansoft(Hancom) SpreadSheet software document";
	ExtMap["csv"] = "Comma-Separated Values";
	ExtMap["gsheet"] = "Google Drive Spreadsheet";
	ExtMap["numbers"] = "An Apple Numbers Spreadsheet file";
	ExtMap["gnumeric"] = "Gnumeric spreadsheet, a gziped XML file";
	ExtMap["ods"] = "OpenDocument spreadsheet";
	ExtMap["ots"] = "OpenDocument spreadsheet template";
	ExtMap["qpw"] = "Quattro Pro spreadsheet";
	ExtMap["sdc"] = "StarOffice StarCalc Spreadsheet";
	ExtMap["slk"] = "SYLK (SYmbolic LinK)";
	ExtMap["stc"] = "OpenOffice.org XML (obsolete) Spreadsheet template";
	ExtMap["sxc"] = "OpenOffice.org XML (obsolete) Spreadsheet";
	ExtMap["tab"] = "tab delimited columns; also TSV (Tab-Separated Values)";
	ExtMap["txt"] = "text file";
	ExtMap["vc"] = "Visicalc";
	ExtMap["wk1"] = "Lotus 1-2-3 up to version 2.01";
	ExtMap["wk3"] = "Lotus 1-2-3 version 3.0";
	ExtMap["wk4"] = "Lotus 1-2-3 version 4.0";
	ExtMap["wks"] = "Lotus 1-2-3";
	ExtMap["wks"] = "Microsoft Works";
	ExtMap["wq1"] = "Quattro Pro DOS version";
	ExtMap["xlk"] = "Microsoft Excel worksheet backup";
	ExtMap["xls"] = "Microsoft Excel worksheet sheet (97–2003)";
	ExtMap["xlsb"] = "Microsoft Excel binary workbook";
	ExtMap["xlsm"] = "Microsoft Excel Macro-enabled workbook";
	ExtMap["xlsx"] = "Office Open XML worksheet sheet";
	ExtMap["xlr"] = "Microsoft Works version 6.0";
	ExtMap["xlt"] = "Microsoft Excel worksheet template";
	ExtMap["xltm"] = "Microsoft Excel Macro-enabled worksheet template";
	ExtMap["xlw"] = "Microsoft Excel worksheet workspace (version 4.0)";
	return ExtMap;
}

////////////////////////////////////////////////////////////////////////////
// Pictures
//

QHash<QString, QString> InitImagesEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["ase"] = "Adobe Swatch";
	ExtMap["art"] = "America Online proprietary format";
	ExtMap["bmp"] = "Microsoft Windows Bitmap formatted image";
	ExtMap["blp"] = "Blizzard Entertainment proprietary texture format";
	ExtMap["cd5"] = "Chasys Draw IES image";
	ExtMap["cit"] = "Intergraph is a monochrome bitmap format";
	ExtMap["cpt"] = "Corel PHOTO-PAINT image";
	ExtMap["cr2"] = "Canon camera raw format. Photos will have this format on some Canon cameras if the quality 'RAW' is selected in camera settings.";
	ExtMap["cut"] = "Dr. Halo image file";
	ExtMap["dds"] = "DirectX texture file";
	ExtMap["dib"] = "Device-Independent Bitmap graphic";
	ExtMap["djvu"] = "DjVu for scanned documents";
	ExtMap["egt"] = "EGT Universal Document, used in EGT SmartSense to compress PNG files to yet a smaller file";
	ExtMap["exif"] = "Exchangeable image file format (Exif) is a specification for the image file format used by digital cameras";
	ExtMap["gif"] = "CompuServe's Graphics Interchange Format";
	ExtMap["gpl"] = "GIMP Palette, using a textual representation of color names and RGB values";
	ExtMap["grf"] = "Zebra Technologies proprietary format";
	ExtMap["icns"] = "file format use for icons in Mac OS X. Contains bitmap images at multiple resolutions and bitdepths with alpha channel.";
	ExtMap["ico"] = "a file format used for icons in Microsoft Windows. Contains small bitmap images at multiple resolutions and sizes.";
	ExtMap["iff"] = "ILBM";
	ExtMap["ilbm"] = "ILBM";
	ExtMap["lbm"] = "ILBM";
	ExtMap["jng"] = "a single-frame MNG using JPEG compression and possibly an alpha channel.";
	ExtMap["jpeg"] = "Joint Photographic Experts Group - a lossy image format widely used to display photographic images.";
	ExtMap["jfif"] = "Joint Photographic Experts Group - a lossy image format widely used to display photographic images.";
	ExtMap["jp2"] = "JPEG2000";
	ExtMap["jps"] = "JPEG Stereo";
	ExtMap["lbm"] = "Deluxe Paint image file";
	ExtMap["max"] = "ScanSoft PaperPort document";
	ExtMap["miff"] = "ImageMagick's native file format";
	ExtMap["mng"] = "Multiple Network Graphics, the animated version of PNG";
	ExtMap["msp"] = "a file format used by old versions of Microsoft Paint. Replaced with BMP in Microsoft Windows 3.0";
	ExtMap["nitf"] = "A U.S. Government standard commonly used in Intelligence systems";
	ExtMap["ota"] = "a specification designed by Nokia for black and white images for mobile phones";
	ExtMap["pbm"] = "Portable bitmap";
	ExtMap["pc1"] = "Low resolution, compressed Degas picture file";
	ExtMap["pc2"] = "Medium resolution, compressed Degas picture file";
	ExtMap["pc3"] = "High resolution, compressed Degas picture file";
	ExtMap["pcf"] = "Pixel Coordination Format";
	ExtMap["pcx"] = "a lossless format used by ZSoft's PC Paint, popular at one time on DOS systems.";
	ExtMap["pdn"] = "Paint.NET image file";
	ExtMap["pgm"] = "Portable graymap";
	ExtMap["pi1"] = "Low resolution, uncompressed Degas picture file";
	ExtMap["pi2"] = "Medium resolution, uncompressed Degas picture file. Also Portrait Innovations encrypted image format.";
	ExtMap["pi3"] = "High resolution, uncompressed Degas picture file";
	ExtMap["pct"] = "Apple Macintosh PICT image";
	ExtMap["pict"] = "Apple Macintosh PICT image";
	ExtMap["png"] = "Portable Network Graphic (lossless, recommended for display and edition of graphic images)";
	ExtMap["pnm"] = "Portable anymap graphic bitmap image";
	ExtMap["pns"] = "PNG Stereo";
	ExtMap["ppm"] = "Portable Pixmap (Pixel Map) image";
	ExtMap["psb"] = "Adobe Photoshop Big image file (for large files)";
	ExtMap["psd"] = "Adobe Photoshop Drawing";
	ExtMap["pdd"] = "Adobe Photoshop Drawing";
	ExtMap["psp"] = "Paint Shop Pro image";
	ExtMap["px"] = "Pixel image editor image file";
	ExtMap["pxm"] = "Pixelmator image file";
	ExtMap["pxr"] = "Pixar Image Computer image file";
	ExtMap["qfx"] = "QuickLink Fax image";
	ExtMap["raw"] = "General term for minimally processed image data (acquired by a digital camera)";
	ExtMap["rle"] = "a run-length encoded image";
	ExtMap["sct"] = "Scitex Continuous Tone image file";
	ExtMap["sgi"] = "Silicon Graphics Image";
	ExtMap["rgb"] = "Silicon Graphics Image";
	ExtMap["int"] = "Silicon Graphics Image";
	ExtMap["bw"] = "Silicon Graphics Image";
	ExtMap["tga"] = "Truevision TGA (Targa) image";
	ExtMap["targa"] = "Truevision TGA (Targa) image";
	ExtMap["icb"] = "Truevision TGA (Targa) image";
	ExtMap["vda"] = "Truevision TGA (Targa) image";
	ExtMap["vst"] = "Truevision TGA (Targa) image";
	ExtMap["pix"] = "Truevision TGA (Targa) image";
	ExtMap["tiff"] = "Tagged Image File Format (usually lossless, but many variants exist, including lossy ones)";
	ExtMap["tif"] = "Tagged Image File Format (usually lossless, but many variants exist, including lossy ones)";
	ExtMap["vtf"] = "Valve Texture Format";
	ExtMap["xbm"] = "X Window System Bitmap";
	ExtMap["xcf"] = "GIMP image (from Gimp's origin at the eXperimental Computing Facility of the University of California)";
	ExtMap["xpm"] = "X Window System Pixmap";

	return ExtMap;
}

QHash<QString, QString> InitVImagesEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["3dv"] = "3-D wireframe graphics by Oscar Garcia";
	ExtMap["amf"] = "Additive Manufacturing File Format";
	ExtMap["awg"] = "Ability Draw";
	ExtMap["ai"] = "Adobe Illustrator Document";
	ExtMap["cgm"] = "Computer Graphics Metafile an ISO Standard";
	ExtMap["cdr"] = "CorelDRAW Document";
	ExtMap["cmx"] = "CorelDRAW vector image";
	ExtMap["dxf"] = "ASCII Drawing Interchange file Format, used in AutoCAD and other CAD-programs";
	ExtMap["e2d"] = "2-dimensional vector graphics used by the editor which is included in JFire";
	ExtMap["egt"] = "EGT Universal Document, EGT Vector Draw images are used to draw vector to a website";
	ExtMap["eps"] = "Encapsulated Postscript";
	ExtMap["fs"] = "FlexiPro file";
	ExtMap["gbr"] = "Gerber file";
	ExtMap["odg"] = "OpenDocument Drawing";
	ExtMap["svg"] = "Scalable Vector Graphics, employs XML";
	ExtMap["stl"] = "Stereo Lithographic data format (see STL (file format)) used by various CAD systems and stereo lithographic printing machines. See above.";
	ExtMap["wrl"] = "Virtual Reality Modeling Language, for the creation of 3D viewable web images.";
	ExtMap["x3d"] = "X3D";
	ExtMap["sxd"] = "OpenOffice.org XML (obsolete) Drawing";
	ExtMap["v2d"] = "voucher design used by the voucher management included in JFire";
	ExtMap["vnd"] = "Vision numeric Drawing file used in TypeEdit, Gravostyle.";
	ExtMap["wmf"] = "Windows Meta File";
	ExtMap["emf"] = "Enhanced (Windows) MetaFile, an extension to WMF";
	ExtMap["art"] = "Xara - Drawing (superseded by XAR)";
	ExtMap["xar"] = "Xara - Drawing";

	return ExtMap;
}

QHash<QString, QString> Init3DImagesEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["3dmf"] = "QuickDraw 3D Metafile (.3dmf)";
	ExtMap["3dm"] = "OpenNURBS Initiative 3D Model (used by Rhinoceros 3D) (.3dm)";
	ExtMap["3ds"] = "Legacy 3D Studio Model (.3ds)";
	ExtMap["abc"] = "Alembic (Computer Graphics)";
	ExtMap["ac"] = "AC3D Model (.ac)";
	ExtMap["amf"] = "Additive Manufacturing File Format";
	ExtMap["an8"] = "Anim8or Model (.an8)";
	ExtMap["aoi"] = "Art of Illusion Model (.aoi)";
	ExtMap["b3d"] = "Blitz3D Model (.b3d)";
	ExtMap["blend"] = "Blender (.blend)";
	ExtMap["block"] = "Blender encrypted blend files (.block)";
	ExtMap["c4d"] = "Cinema 4D (.c4d)";
	ExtMap["cal3d"] = "Cal3D (.cal3d)";
	ExtMap["ccp4"] = "X-ray crystallography voxels (electron density)";
	ExtMap["cfl"] = "Compressed File Library (.cfl)";
	ExtMap["cob"] = "Caligari Object (.cob)";
	ExtMap["core3d"] = "Coreona 3D Coreona 3D Virtual File(.core3d)";
	ExtMap["ctm"] = "OpenCTM (.ctm)";
	ExtMap["dae"] = "COLLADA (.dae)";
	ExtMap["dff"] = "RenderWare binary stream, commonly used by Grand Theft Auto III-era games as well as other RenderWare titles";
	ExtMap["dpm"] = "deepMesh (.dpm)";
	ExtMap["dts"] = "Torque Game Engine (.dts)";
	ExtMap["egg"] = "Panda3D Engine";
	ExtMap["fact"] = "Electric Image (.fac)";
	ExtMap["fbx"] = "Autodesk FBX (.fbx)";
	ExtMap["g"] = "BRL-CAD geometry (.g)";
	ExtMap["glm"] = "Ghoul Mesh (.glm)";
	ExtMap["jas"] = "Cheetah 3D file (.jas)";
	ExtMap["lwo"] = "Lightwave Object (.lwo)";
	ExtMap["lws"] = "Lightwave Scene (.lws)";
	ExtMap["lxo"] = "Luxology Modo (software) file (.lxo)";
	ExtMap["ma"] = "Autodesk Maya ASCII File (.ma)";
	ExtMap["max"] = "Autodesk 3D Studio Max file (.max)";
	ExtMap["mb"] = "Autodesk Maya Binary File (.mb)";
	ExtMap["md2"] = "Quake 2 model format (.md2)";
	ExtMap["md3"] = "Quake 3 model format (.md3)";
	ExtMap["mdx"] = "Blizzard Entertainment's own model format (.mdx)";
	ExtMap["mesh"] = "New York University(.m)";
	ExtMap["mesh"] = "Meshwork Model (.mesh)";
	ExtMap["mm3d"] = "Misfit Model 3d (.mm3d)";
	ExtMap["mpo"] = "Multi-Picture Object - This JPEG standard is used for 3d images, as with the Nintendo 3DS";
	ExtMap["mrc"] = "voxels in cryo-electron microscopy";
	ExtMap["nif"] = "Gamebryo NetImmerse File (.nif)";
	ExtMap["obj"] = "Wavefront .obj file (.obj)";
	ExtMap["off"] = "OFF Object file format (.off)";
	ExtMap["ogex"] = "Open Game Engine Exchange (OpenGEX) format (.ogex)";
	ExtMap["ply"] = "Polygon File Format / Stanford Triangle Format (.ply)";
	ExtMap["prc"] = "Adobe PRC (embedded in PDF files)";
	ExtMap["pov"] = "POV-Ray document (.pov)";
	ExtMap["rwx"] = "RenderWare Object (.rwx)";
	ExtMap["sia"] = "Nevercenter Silo Object (.sia)";
	ExtMap["sib"] = "Nevercenter Silo Object (.sib)";
	ExtMap["skp"] = "Google Sketchup file (.skp)";
	ExtMap["sldasm"] = "SolidWorks Assembly Document (.sldasm)";
	ExtMap["sldprt"] = "SolidWorks Part Document (.sldprt)";
	ExtMap["smd"] = "Valve Studiomdl Data format. (.smd)";
	ExtMap["u3d"] = "Universal 3D file format (.u3d)";
	ExtMap["vim"] = "Revizto visual information model format (.vimproj)";
	ExtMap["vrml97"] = "VRML Virtual reality modeling language (.wrl)";
	ExtMap["vue"] = "Vue scene file (.vue)";
	ExtMap["vwx"] = "Vectorworks (.vwx)";
	ExtMap["wings"] = "Wings3D (.wings)";
	ExtMap["w3d"] = "Wes twood 3D Model (.w3d)";
	ExtMap["x"] = "DirectX 3D Model (.x)";
	ExtMap["x3d"] = "Extensible 3D (.x3d)";
	ExtMap["z3d"] = "Zmodeler (.z3d)";
	return ExtMap;
}

////////////////////////////////////////////////////////////////////////////
// Audio
// 

QHash<QString, QString> InitAudioEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["8svx"] = "Commodore-Amiga 8-bit sound (usually in an IFF container)";
	ExtMap["16svx"] = "Commodore-Amiga 16-bit sound (usually in an IFF container)";
	ExtMap["aif"] = "Audio Interchange File Format";
	ExtMap["aifc"] = "Audio Interchange File Format";
	ExtMap["aiff"] = "Audio Interchange File Format";
	ExtMap["au"] = "AU";
	ExtMap["bwf"] = "Broadcast Wave Format (BWF), an extension of WAVE";
	ExtMap["cdda"] = "CDDA";
	ExtMap["raw"] = "raw samples without any header or sync";
	ExtMap["wav"] = "Microsoft Wave";
	ExtMap["flac"] = "free lossless codec of the Ogg project";
	ExtMap["la"] = "Lossless Audio (.la)";
	ExtMap["pac"] = "LPAC (.pac)";
	ExtMap["m4a"] = "Apple Lossless (M4A)";
	ExtMap["ape"] = "Monkey's Audio (APE)";
	ExtMap["ofr"] = "OptimFROG (.ofr, .ofs, .off)";
	ExtMap["ofs"] = "OptimFROG (.ofr, .ofs, .off)";
	ExtMap["off"] = "OptimFROG (.ofr, .ofs, .off)";
	ExtMap["rka"] = "RKAU (.rka)";
	ExtMap["shn"] = "Shorten (SHN)";
	ExtMap["tak"] = "Tom's Lossless Audio Kompressor (TAK)[8]";
	ExtMap["tta"] = "free lossless audio codec (True Audio)";
	ExtMap["wv"] = "WavPack (.wv)";
	ExtMap["wma"] = "Windows Media Audio 9 Lossless (WMA)";
	ExtMap["brstm"] = "Binary Revolution Stream (.brstm)[9]";
	ExtMap["dts"] = "DTS (sound system)";
	ExtMap["dtshd"] = "DTS (sound system)";
	ExtMap["dtsma"] = "DTS (sound system)";
	ExtMap["ast"] = "Audio Stream (.ast)[10]";
	ExtMap["amr"] = "for GSM and UMTS based mobile phones";
	ExtMap["mp2"] = "MPEG Layer 2";
	ExtMap["mp3"] = "MPEG Layer 3";
	ExtMap["spx"] = "Speex (Ogg project, specialized for voice, low bitrates)";
	ExtMap["gsm"] = "GSM Full Rate, originally developed for use in mobile phones";
	ExtMap["wma"] = "Windows Media Audio (.WMA)";
	ExtMap["m4a"] = "Advanced Audio Coding (usually in an MPEG-4 container)";
	ExtMap["aac"] = "Advanced Audio Coding (usually in an MPEG-4 container)";
	ExtMap["mpc"] = "Musepack";
	ExtMap["vqf"] = "Yamaha TwinVQ";
	ExtMap["ra"] = "RealAudio (RA, RM)";
	//ExtMap["rm"] = "RealAudio (RA, RM)";
	ExtMap["ots"] = "Audio File (similar to MP3, with more data stored in the file and slightly better compression; designed for use with OtsLabs' OtsAV)";
	ExtMap["swa"] = "Macromedia Shockwave Audio (Same compression as MP3 with additional header information specific to Macromedia Director";
	ExtMap["vox"] = "Dialogic ADPCM Low Sample Rate Digitized Voice (VOX)";
	ExtMap["voc"] = "Creative Labs Soundblaster Creative Voice 8-bit & 16-bit (VOC)";
	ExtMap["dwd"] = "DiamondWare Digitized (DWD)";
	ExtMap["smp"] = "Turtlebeach SampleVision (SMP)";
	return ExtMap;
}

////////////////////////////////////////////////////////////////////////////
// Video
// 

QHash<QString, QString> InitVideoEXTs()
{
	QHash<QString, QString> ExtMap;
	ExtMap["aaf"] = "mostly intended to hold edit decisions and rendering information, but can also contain compressed media essence";
	ExtMap["3gp"] = "the most common video format for cell phones";
	ExtMap["gif"] = "Animated GIF (simple animation; until recently often avoided because of patent problems)";
	ExtMap["asf"] = "container (enables any form of compression to be used; MPEG-4 is common; video in ASF-containers is also called Windows Media Video (WMV))";
	ExtMap["avchd"] = "Advanced Video Codec High Definition";
	ExtMap["avi"] = "container (a shell, which enables any form of compression to be used)";
	ExtMap["cam"] = "aMSN webcam log file";
	ExtMap["dat"] = "video standard data file (automatically created when we attempted to burn as video file on the CD)";
	ExtMap["dsh"] = "DSH";
	ExtMap["flv"] = "Flash video (encoded to run in a flash animation)";
	ExtMap["m1v"] = "1 - Video";
	ExtMap["m2v"] = "2 - Video";
	ExtMap["fla"] = "Macromedia Flash (for producing)";
	ExtMap["flr"] = "(text file which contains scripts extracted from SWF by a free ActionScript decompiler named FLARE)";
	ExtMap["sol"] = "Adobe Flash shared object ('Flash cookie')";
	ExtMap["m4v"] = "(file format for videos for iPods and PlayStation Portables developed by Apple)";
	ExtMap["mkv"] = "Matroska is a container format, which enables any video format such as MPEG-4 ASP or AVC to be used along with other content such as subtitles and detailed meta information";
	ExtMap["wrap"] = "MediaForge (*.wrap)";
	ExtMap["mng"] = "mainly simple animation containing PNG and JPEG objects, often somewhat more complex than animated GIF";
	ExtMap["mov"] = "container which enables any form of compression to be used; Sorenson codec is the most common; QTCH is the filetype for cached video and audio streams";
	ExtMap["mpeg"] = "MPEG (.mpeg, .mpg, .mpe)";
	ExtMap["mpg"] = "MPEG (.mpeg, .mpg, .mpe)";
	ExtMap["mpe"] = "MPEG (.mpeg, .mpg, .mpe)";
	ExtMap["mp4"] = "MPEG-4";
	//ExtMap["m4p"] = "MPEG-4 (DRM)";
	ExtMap["mxf"] = "Material Exchange Format (standardized wrapper format for audio/visual material developed by SMPTE)";
	ExtMap["roq"] = "used by Quake 3";
	ExtMap["nsv"] = "Nullsoft Streaming Video (media container designed for streaming video content over the Internet)";
	ExtMap["ogg"] = "container, multimedia";
	ExtMap["rm"] = "RealMedia";
	ExtMap["svi"] = "Samsung video format for portable players";
	ExtMap["smi"] = "SAMI Caption file (HTML like subtitle for movie files)";
	ExtMap["swf"] = "Macromedia Flash (for viewing)";
	ExtMap["wmv"] = "Windows Media Video (See ASF)";
	ExtMap["yuv"] = "Raw video format - Resolution (horizontal x vertical)and Sample structure 4:2:2 or 4:2:0 need to be know explicitly";

	return ExtMap;
}
