#include <QtGui/QApplication>
#include "test_qt.h"

#include "../../../NeoLoader/Common/Archive/Archive.h"


#include <QFile>
#include <QTemporaryFile>



int main(int argc, char *argv[])
{
	QApplication a(argc, argv);


	/*QIODevice* xxx = new QTemporaryFile();
	xxx->open(QIODevice::WriteOnly);
	xxx->write("test");
	xxx->close();
	xxx->open(QIODevice::ReadOnly);
	QByteArray test = xxx->readAll();*/

	//QMap<QString, QIODevice*> Files;
	//Files.insert("grandmap.jpg", new QFile("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/grandmap.jpg"));

	CArchive::GetArcInfo("bla blup/test.jpg.part2.rar");

	CArchive test1("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/grandmap_2.zip");
	test1.Open();

	int ArcIndex = test1.AddFile("1199743956786.jpg");
	QMap<int, QIODevice*> Files;
	Files.insert(ArcIndex, new QFile("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/1199743956786.jpg"));
	test1.Update(&Files);
	

	/*CArchive test1("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/test.7z");
	test1.Open();
	test1.Extract();
	test1.Close();*/


	/*************************************************
	* Compression
	*/
	/*CreateObjectFunc createObjectFunc = (CreateObjectFunc)SevenZip.resolve("CreateObject");
	Q_ASSERT(createObjectFunc);

    CMyComPtr<IOutArchive> outArchive;
    Q_ASSERT(createObjectFunc(&CLSID_7z, &IID_IOutArchive, (void **)&outArchive) == S_OK);

	QFile* pFile0 = new QFile("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/test.7z");
	COutFile *outFileSpec = new COutFile(pFile0);
    CMyComPtr<IOutStream> outFileStream = outFileSpec;

	QFile* pFile1 = new QFile("F:/Temp/1199743956786.jpg");
	CArchiveUpdater *updateSpec = new CArchiveUpdater(outArchive);
	updateSpec->AddFile(pFile1);
    CMyComPtr<IArchiveUpdateCallback2> updateCallback(updateSpec);
	Q_ASSERT(outArchive->UpdateItems(outFileStream, updateSpec->Count(), updateCallback) == S_OK);*/



	/*************************************************
	* Decompression
	*/
	/*CMyComPtr<IInArchive> inArchive;
    Q_ASSERT(createObjectFunc(&CLSID_7z, &IID_IInArchive, (void **)&inArchive) == S_OK);

	QFile* pFile0 = new QFile("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/test.part1.rar");
	CInFile *inFileSpec = new CInFile(pFile0);
    CMyComPtr<IInStream> inFileStream = inFileSpec;

	CArchiveOpener *openSpec = new CArchiveOpener(inArchive);
	CMyComPtr<IArchiveOpenCallback> openCallback(openSpec);
	Q_ASSERT(inArchive->Open(inFileStream, 0, openCallback) == S_OK);

	QString FileName = openSpec->LoadList().keys()[0];
	QFile* pFile1 = new QFile("F:/Projects/Filesharing/NeoLoader/NeoLoader/7-Zip/test_qt/Debug/" + FileName);

	CArchiveExtractor *extractSpec = new CArchiveExtractor(inArchive);
	extractSpec->AddFile(FileName, pFile1);
	CMyComPtr<IArchiveExtractCallback> extractCallback(extractSpec);
	Q_ASSERT(inArchive->Extract(NULL, (UInt32)(Int32)(-1), false, extractCallback) == S_OK);*/


	test_qt w;
	w.show();
	return a.exec();
}
