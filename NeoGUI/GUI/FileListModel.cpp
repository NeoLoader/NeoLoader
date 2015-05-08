#include "GlobalHeader.h"
#include "FileListModel.h"
#include "FileListView.h"
#include "../NeoGUI.h"
#include "FileSummary.h"
#include "../Common/Common.h"

CFilesModel::CFilesModel(QObject *parent)
:QAbstractItemModel(parent)
{
	m_Mode = CFileListView::eUndefined;

	m_Root = new SFileNode(-1);
}

CFilesModel::~CFilesModel()
{
	delete m_Root;
}

void CFilesModel::IncrSync(const QVariantList& Files)
{
	QMap<QString, QList<SFileNode*> > New;
	QMap<uint64, SFileNode*> Old;

	foreach (const QVariant vFile, Files)
	{
		QVariantMap File = vFile.toMap();
		uint64 ID = File["ID"].toULongLong();

		bool bUpdate = true;
		
		SFileNode* pNode = m_Map.value(ID);
		if(File.value("FileState") == "Deprecated")
		{
			if(pNode)
				Old.insert(ID, pNode);
			continue;
		}

		SFileNode* pOldNode = NULL;
		if(pNode && ((File.contains("FileDir") && pNode->Path != File["FileDir"].toString()) || (File.contains("FileName") && pNode->Name  != File["FileName"].toString()))) 
		{
			pOldNode = pNode;

			Old.insert(ID, pNode);	
			pNode = NULL;
		}

		if(!pNode)
		{
			pNode = new SFileNode(ID);

			if(pOldNode)
			{
				pNode->File = pOldNode->File;
				pNode->Name = pOldNode->Name;
				pNode->Path = pOldNode->Path;
			}

			if(File.contains("FileDir"))
				pNode->Path = File["FileDir"].toString();
			if(File.contains("FileName"))
				pNode->Name = File["FileName"].toString();

			New[pNode->Path].append(pNode);
			bUpdate = false;
		}

		for(QVariantMap::iterator I = File.begin(); I != File.end(); I++)
			pNode->File[I.key()] = I.value();

		if(!bUpdate)
			continue;

		//
		QModelIndex Index;
		//emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		int Col = 0;
		bool State = false;
		for(int i = 0; i < columnCount(); i++)
		{
			bool Changed = false;
			switch(i)
			{
				case eFileName:		Changed = File.contains("FileName") || File.contains("FileState") || File.contains("KnownStatus"); break;
				case eSize:			Changed = File.contains("FileSize"); break;
				case eType:			Changed = File.contains("FileType"); break;
				case eStatus:		Changed = File.contains("FileState") || File.contains("FileStatus") || File.contains("FileJobs") || File.contains("HosterStatus"); break;
				case eProgress:		Changed = File.contains("Progress"); break;
				case eAvailability:	Changed = File.contains("Availability") || File.contains("AuxAvailability"); break;
				case eSources:		Changed = File.contains("ConnectedTransfers") || File.contains("CheckedTransfers") || File.contains("SeedTransfers") || File.contains("Transfers"); break;
				case eUpRate:		Changed = File.contains("Upload") || File.contains("UpRate"); break;
				case eDownRate:		Changed = File.contains("Download") || File.contains("DownRate"); break;
				case eUploaded:		Changed = File.contains("Uploaded") || File.contains("Downloaded") || File.contains("ShareRatio"); break;
				case eDownloaded:	Changed = File.contains("Downloaded"); break;
				case eQueuePos:		Changed = File.contains("QueuePos") || File.contains("Priority") || File.contains("Force") || File.contains("Upload") || File.contains("Download"); break;
			}

			if(State != Changed)
			{
				if(State)
				{
					if(!Index.isValid())
						Index = Find(m_Root, pNode);
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), i-1, pNode));
				}
				State = Changed;
				Col = i;
			}
		}
		if(State)
		{
			if(!Index.isValid())
				Index = Find(m_Root, pNode);
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		}
		//
	}

	Sync(New, Old);
}

void CFilesModel::Sync(const QVariantList& Files)
{
	QMap<QString, QList<SFileNode*> > New;
	QMap<uint64, SFileNode*> Old = m_Map;

	foreach (const QVariant vFile, Files)
	{
		QVariantMap File = vFile.toMap();
		uint64 ID = File["ID"].toULongLong();

		bool bUpdate = true;
		QString Path = File["FileDir"].toString();
		
		SFileNode* pNode = Old.value(ID);
		if(!pNode || pNode->Path != Path)
		{
			bUpdate = false;
			pNode = new SFileNode(ID);
			pNode->Path = Path;
			New[Path].append(pNode);
		}
		else
			Old[ID] = NULL;

		if(pNode->File != File)
		{
			QVariantMap OldFile = pNode->File;
			if(File.contains("FileName"))
			{
				if(bUpdate)
				{
					ASSERT(pNode->Parent);
					int Pos = pNode->Parent->Aux.take(pNode->Name);
					pNode->Parent->Aux.insert(File["FileName"].toString(), Pos);
				}
				pNode->Name = File["FileName"].toString();
			}
			pNode->File = File;

			if(bUpdate)
			{
				QModelIndex Index = Find(m_Root, pNode);
				//emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
				int Col = 0;
				bool State = false;
				for(int i = 0; i < columnCount(); i++)
				{
					bool Changed = false;
					switch(i)
					{
						case eFileName:		Changed = (OldFile["FileName"] != File["FileName"] || OldFile["FileState"] != File["FileState"] || OldFile["FileStatus"] != File["FileStatus"] || OldFile["HosterStatus"] != File["HosterStatus"]); break;
						case eSize:			Changed = (OldFile["FileSize"] != File["FileSize"]); break;
						case eType:			Changed = (OldFile["FileType"] != File["FileType"]); break;
						case eStatus:		Changed = (OldFile["FileState"] != File["FileState"] || OldFile["FileStatus"] != File["FileStatus"] || OldFile["FileJobs"] != File["FileJobs"]); break;
						case eProgress:		Changed = (OldFile["Progress"] != File["Progress"]); break;
						case eAvailability:	Changed = (OldFile["Availability"] != File["Availability"] || OldFile["AuxAvailability"] != File["AuxAvailability"]); 
						case eSources:		Changed = (OldFile["ConnectedTransfers"] != File["ConnectedTransfers"] || OldFile["CheckedTransfers"] != File["CheckedTransfers"] || OldFile["SeedTransfers"] != File["SeedTransfers"] || OldFile["Transfers"] != File["Transfers"]);
						case eUpRate:		Changed = (OldFile["Upload"] != File["Upload"] || OldFile["UpRate"] != File["UpRate"]); break;
						case eDownRate:		Changed = (OldFile["Download"] != File["Download"] || OldFile["DownRate"] != File["DownRate"]); break;
						case eUploaded:		Changed = (OldFile["Uploaded"] != File["Uploaded"]); break;
						case eDownloaded:	Changed = (OldFile["Downloaded"] != File["Downloaded"]); break;
						case eQueuePos:		Changed = (OldFile["QueuePos"] != File["QueuePos"] || OldFile["Priority"] != File["Priority"] || OldFile["Force"] != File["Force"]); break;
					}

					if(State != Changed)
					{
						if(State)
							emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), i-1, pNode));
						State = Changed;
						Col = i;
					}
				}
				if(State)
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
			}
		}
	}

	Sync(New, Old);
}

void CFilesModel::Sync(QMap<QString, QList<SFileNode*> >& New, QMap<uint64, SFileNode*>& Old)
{
	Purge(m_Root, QModelIndex(), Old);

	if(!New.isEmpty())
	{
		emit layoutAboutToBeChanged();

		//foreach(const QString& Path, New.uniqueKeys())
		for(QMap<QString, QList<SFileNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
			Fill(m_Root, QModelIndex(), I.key().split("/", QString::SkipEmptyParts), 0, I.value(), I.key());

		emit layoutChanged();
	}

	emit Updated();
}

void CFilesModel::CountFiles()
{
	CountFiles(m_Root);
}

int CFilesModel::CountFiles(SFileNode* pRoot)
{
	if(pRoot->Children.isEmpty())
		return 1;
	
	int Count = 0;
	foreach(SFileNode* pChild, pRoot->Children)
		Count += CountFiles(pChild);
	pRoot->AllChildren = Count;
	return Count;
}

void CFilesModel::Purge(SFileNode* pParent, const QModelIndex &parent, QMap<uint64, SFileNode*> &Old)
{
	int Removed = 0;

	int Begin = -1;
	int End = -1;
	for(int i = pParent->Children.count()-1; i >= -1; i--) 
	{
		SFileNode* pNode = i >= 0 ? pNode = pParent->Children[i] : NULL;
		if(pNode)
			Purge(pNode, index(i, 0, parent), Old);

		bool bRemove = false;
		if(pNode && (!pNode->ID || (bRemove = Old.value(pNode->ID) != NULL)) && pNode->Children.isEmpty()) // remove it
		{
			m_Map.remove(pNode->ID, pNode);
			if(End == -1)
				End = i;
		}
		else // keep it
		{
			if(bRemove)
			{
				ASSERT(!pNode->Children.isEmpty()); // we wanted to remove it but we have to keep it
				m_Map.remove(pNode->ID, pNode);
				pNode->ID = 0;
				pNode->Icon.clear();
				pNode->File.clear();
			}

			if(End != -1) // remove whats to be removed at once
			{
				Begin = i + 1;

				beginRemoveRows(parent, Begin, End);
				//ASSERT(pParent->Children.count() > End);
				for(int j = End; j >= Begin; j--)
				{
					pNode = pParent->Children.takeAt(j);
					delete pNode;
					Removed++;
				}
				endRemoveRows();

				End = -1;
				Begin = -1;
			}
		}
    }

	if(Removed > 0)
	{
		pParent->Aux.clear();
		for(int i = pParent->Children.count()-1; i >= 0; i--) 
			pParent->Aux.insert(pParent->Children[i]->Name, i);
	}
}

void CFilesModel::Fill(SFileNode* pParent, const QModelIndex &parent, const QStringList& Paths, int PathsIndex, const QList<SFileNode*>& New, const QString& Path)
{
	if(Paths.size() > PathsIndex)
	{
		QString CurPath = Paths.at(PathsIndex);
		SFileNode* pNode;
		int i = pParent->Aux.value(CurPath, -1);
		if(i != -1)
			pNode = pParent->Children[i];
		else
		{
			i = 0;
			pNode = new SFileNode(0);
			pNode->Name = CurPath;
			pNode->Parent = pParent;

			//int Count = pParent->Children.count();
			//beginInsertRows(parent, Count, Count);
			pParent->Aux.insert(pNode->Name, pParent->Children.size());
			pParent->Children.append(pNode);
			//endInsertRows();
		}
		Fill(pNode, index(i, 0, parent), Paths, PathsIndex + 1, New, Path);
	}
	else
	{
		for(QList<SFileNode*>::const_iterator I = New.begin(); I != New.end(); I++)
		{
			SFileNode* pNode = *I;
			ASSERT(pNode);
			//ASSERT(!m_Map.contains(pNode->ID));
			if(pNode->File["FileType"] == "MultiFile" || pNode->File["FileType"] == "Collection")
			{
				for(int i=0; i < pParent->Children.count(); i++)
				{
					SFileNode* pKnownNode = pParent->Children.at(i);
					if(pKnownNode->ID != 0)
						continue;
					if(pKnownNode->Name == pNode->Name)
					{
						ASSERT(pNode->Children.isEmpty());
						pKnownNode->ID = pNode->ID;
						pKnownNode->File = pNode->File;
						m_Map.insert(pKnownNode->ID, pKnownNode);
						pNode = NULL;
						break;
					}
				}
				if(pNode == NULL)
					continue;
			}
			m_Map.insert(pNode->ID, pNode);
			pNode->Parent = pParent;

			//int Count = pParent->Children.count();
			//beginInsertRows(parent, Count, Count);
			pParent->Aux.insert(pNode->Name, pParent->Children.size());
			pParent->Children.append(pNode);
			//endInsertRows();
		}
	}
}

QModelIndex CFilesModel::FindIndex(uint64 ID)
{
	if(SFileNode* pNode = m_Map.value(ID))
		return Find(m_Root, pNode);
	return QModelIndex();
}

QModelIndex CFilesModel::Find(SFileNode* pParent, SFileNode* pNode)
{
	for(int i=0; i < pParent->Children.count(); i++)
	{
		if(pParent->Children[i] == pNode)
			return createIndex(i, eFileName, pNode);

		QModelIndex Index = Find(pParent->Children[i], pNode);
		if(Index.isValid())
			return Index;
	}
	return QModelIndex();
}

void CFilesModel::Clear()
{
	QMap<uint64, SFileNode*> Old = m_Map;
	//beginResetModel();
	Purge(m_Root, QModelIndex(), Old);
	//endResetModel();
	ASSERT(m_Map.isEmpty());
}

QVariantMap CFilesModel::Data(const QModelIndex &index) const
{
	if (!index.isValid())
        return QVariantMap();

	SFileNode* pNode = static_cast<SFileNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->File;
}

QVariant CFilesModel::data(const QModelIndex &index, int role) const
{
    return data(index, role, index.column());
}

bool CFilesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(index.column() == 0 && role == Qt::CheckStateRole)
	{
		SFileNode* pNode = static_cast<SFileNode*>(index.internalPointer());
		ASSERT(pNode);
		emit CheckChanged(pNode->ID, value.toInt() != Qt::Unchecked);
		return true;
	}
	return false;
}

uint64 GetTransferETA(uint64 uActiveTime, uint64 uFileSize, uint64 uTransferred, uint64 uDataRate)
{
	if(uTransferred >= uFileSize)
		return 0;

	uint64 uAdvanced = 0;
	if(uActiveTime && uTransferred > 0)
		uAdvanced = uActiveTime * (uFileSize - uTransferred) / uTransferred;

	uint64 uSimple = 0;
	if(uDataRate)
		uSimple = (uFileSize - uTransferred) / uDataRate;

	if(uAdvanced && uSimple == 0)
		return uAdvanced;
	if(uAdvanced == 0 && uSimple)
		return uSimple;
	return Min(uAdvanced, uSimple);
}

QVariant CFilesModel::data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole)
    //    return QSize(64,16); // for fixing height

	SFileNode* pNode = static_cast<SFileNode*>(index.internalPointer());
	ASSERT(pNode);

	if(role == Qt::DecorationRole)
	{
		if (section == eFileName)
		{
			QString Ext;
			if(pNode->ID == 0)
				Ext = ".";
			else if(pNode->File["FileType"] == "MultiFile" || pNode->File["FileType"] == "Archive")
				Ext = "...";
			else if(pNode->File["FileType"] == "Collection")
				Ext = "....";
			else
				Ext = Split2(pNode->Name, ".", true).second;

			if(!pNode->Icon.isValid() || Ext != pNode->Ext)
			{
				pNode->Ext = Ext;
				pNode->Icon = GetFileIcon(Ext, 16);
			}
			return pNode->Icon;
		}
		return QVariant();
	}

	if(pNode->ID == 0 || pNode->File["FileType"] == "Collection")
	{
		if(section == eFileName && (role == Qt::UserRole))
			return pNode->ID;
		if(section == eFileName && (role == Qt::DisplayRole || role == Qt::EditRole))
			return pNode->Name;
		if(section == eSize && (role == Qt::DisplayRole || role == Qt::EditRole))
			return pNode->AllChildren;
		return QVariant();
	}

    switch(role)
	{
		case Qt::DisplayRole:
			switch(section)
			{
				case eFileName:		return pNode->Name;
				case eSize:
				{
					QString Size = FormatSize(pNode->File["FileSize"].toULongLong());
					if(pNode->AllChildren)
						Size += " / " + QString::number(pNode->AllChildren);
					return Size;
				}
				case eType:			return pNode->File["FileType"];
				case eStatus:
				{
					QString Status = pNode->File["FileState"].toString() + " " + pNode->File["FileStatus"].toString();
					if(pNode->File["FileJobs"].isValid())
						Status += " " + pNode->File["FileJobs"].toStringList().join(" ");

					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
					{
						if(pNode->File["HosterStatus"].isValid())
						{
							StrPair HosterStatus = Split2(pNode->File["HosterStatus"].toString(), ":");
							Status += " Hoster " + HosterStatus.first;
							if(HosterStatus.first == "Waiting")
								Status += ": " + FormatTime(HosterStatus.second.toULongLong()/1000);
							if(HosterStatus.first == "Error")
								Status += ": " + HosterStatus.second;
						}
					}
					return Status;
				}
				case eProgress:		return tr("%1 %").arg(pNode->File["Progress"].toString());
				case eAvailability:	return "";
				case eSources:	
				{
					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
					{
						int Peers = pNode->File["CheckedTransfers"].toInt();
						int Seeds = pNode->File["SeedTransfers"].toInt();
						int Leechers = Peers - Seeds;
						if(Leechers < 0)
							Leechers = 0;
						return tr("%1|%2/%3 (%4)").arg(Leechers).arg(Seeds).arg(pNode->File["Transfers"].toInt()).arg(pNode->File["ConnectedTransfers"].toInt());
					}
					else
						return tr("%1/%2").arg(pNode->File["CheckedTransfers"].toInt()).arg(pNode->File["Transfers"].toInt());
				}
				case eUpRate:
				{
					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
					{
						return FormatSize(pNode->File["Upload"].toULongLong()) + "/s (" + FormatSize(pNode->File["UpRate"].toULongLong()) + "/s)"
							+ QString(" [%1/%2]").arg(pNode->File["ActiveUploads"].toString()).arg(pNode->File["WaitingUploads"].toString());
					}
					else
						return FormatSize(pNode->File["Upload"].toULongLong()) + "/s";

				}
				case eDownRate:
				{
					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
					{
						return FormatSize(pNode->File["Download"].toULongLong()) + "/s (" + FormatSize(pNode->File["DownRate"].toULongLong()) + "/s)"
							+ QString(" [%1/%2]").arg(pNode->File["ActiveDownloads"].toString()).arg(pNode->File["WaitingDownloads"].toString());
					}
					else
						return FormatSize(pNode->File["Download"].toULongLong()) + "/s";
				}
				case eUploaded:
				{
					uint64 uDownloadedSize = pNode->File["Downloaded"].toULongLong();
					uint64 uUploadedSize = pNode->File["Uploaded"].toULongLong();
					uint64 uFileSize = pNode->File["FileSize"].toULongLong();
					uint64 uSize = pNode->File["FileStatus"] == "Incomplete" ? uDownloadedSize : uFileSize;
					double CurrentRatio = uSize ? double(uUploadedSize) / uSize : 0.0;
					double ShareRatio = pNode->File["ShareRatio"].toDouble() / 100;

					QString Info = FormatSize(uUploadedSize);
					Info += " - " + QString::number(CurrentRatio, 'f', 2);
					if(ShareRatio)
						Info += "/" + QString::number(ShareRatio, 'f', 2);
					return Info;
				}
				case eDownloaded:	
				{
					uint64 uDownloadedSize = pNode->File["Downloaded"].toULongLong();
					time_t uEstimatedTime = pNode->File["FileStatus"] == "Incomplete" ? GetTransferETA(pNode->File["ActiveTime"].toULongLong(), pNode->File["FileSize"].toULongLong(), uDownloadedSize, pNode->File["Download"].toULongLong()) : 0;
					return FormatSize(uDownloadedSize) + (uEstimatedTime ? " - " + (uEstimatedTime > DAY2S(30) ? tr("> 1 Month") : FormatTime(uEstimatedTime)) : "");
				}
				case eQueuePos:
				{
					int Prio = pNode->File["Priority"].toInt();
					QString PrioStr;
					if(Prio == 0	)		PrioStr = tr("");
					else if(Prio <= 1)		PrioStr = tr("--");
					else if(Prio <= 3)		PrioStr = tr("-");
					else if(Prio == 5)		PrioStr = tr("");
					else if(Prio >= 9)		PrioStr = tr("++");
					else if(Prio >= 7)		PrioStr = tr("+");
					QString Info = QString("%1%2%3").arg(pNode->File["Force"].toBool() ? "! " : "").arg(pNode->File["QueuePos"].toInt()).arg(PrioStr);
					// → ← ↑ ↓ ↖↗↘↙
					if(pNode->File["MaxUpload"].toInt() > 0)
						Info += QString::fromWCharArray(L" ↑ %1").arg(FormatSize(pNode->File["MaxUpload"].toInt()) + "/s");
					if(pNode->File["MaxDownload"].toInt() > 0)
						Info += QString::fromWCharArray(L" ↓ %1").arg(FormatSize(pNode->File["MaxDownload"].toInt()) + "/s");
					return Info;
				}
			}
			break;
		case Qt::EditRole: // sort role
			switch(section)
			{
				case eSize:			return pNode->File["FileSize"];
				case eProgress:		return pNode->File["Progress"];
				case eAvailability:	return m_Mode == CFileListView::eFilesSearch ? pNode->File["AuxAvailability"] : pNode->File["Availability"];
				case eSources:		return pNode->File["CheckedTransfers"];
				case eUpRate:		return pNode->File["UpRate"];
				case eDownRate:		return pNode->File["DownRate"];
				case eUploaded:		return pNode->File["Uploaded"];
				case eDownloaded:	return pNode->File["Downloaded"];
				case eQueuePos:		return pNode->File["QueuePos"];
				default:			return data(index, Qt::DisplayRole, section).toString().toLower();
			}
			break;
		case Qt::BackgroundRole:
			{
				if(pNode->File["FileStatus"] == "Error")
					return QBrush(QColor(255,128,128));
				break;
			}
		case Qt::ForegroundRole:
			if(section == eFileName && (m_Mode == CFileListView::eFilesSearch || m_Mode == CFileListView::eFilesGrabber))
			{
				QColor Color = Qt::black;
				if(m_Mode == CFileListView::eFilesGrabber)
				{
					if(pNode->File["FileState"] == "Removed")
						Color = Qt::red;
					else if(pNode->File["FileStatus"] == "Complete")
						Color = Qt::green;
					else if(pNode->File["FileState"] != "Pending" && pNode->File["FileState"] != "Halted") // fiel is being downloaded
						Color = Qt::blue;
				}
				else if(m_Mode == CFileListView::eFilesSearch)
				{
					if(pNode->File["KnownStatus"] == "Removed")
						Color = Qt::red;
					else if(pNode->File["KnownStatus"] == "Complete")
						Color = Qt::green;
					else if(pNode->File["KnownStatus"] == "Incomplete")
						Color = Qt::blue;
				}
				return QBrush(Color); 
			}
			break;
		case Qt::CheckStateRole:
		{
			if(section == eFileName && (((m_Mode == CFileListView::eFilesSearch || m_Mode == CFileListView::eFilesGrabber) && pNode->Parent != m_Root) || m_Mode == CFileListView::eSubFiles))
			{
				if(pNode->File["FileState"] == "Halted")
					return Qt::Unchecked;
				else
					return Qt::Checked;
			}
			break;
        }
		case Qt::UserRole:
			switch(section)
			{
				case eFileName:			return pNode->ID;
				case eAvailability:		return CFileSummary::GetFitness(pNode->File);
			}
			break;
	}
	return QVariant();
}

Qt::ItemFlags CFilesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
	if(index.column() == 0)
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CFilesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SFileNode* pParent;
    if (!parent.isValid())
        pParent = m_Root;
    else
        pParent = static_cast<SFileNode*>(parent.internalPointer());

	if(SFileNode* pNode = pParent->Children.count() > row ? pParent->Children[row] : NULL)
        return createIndex(row, column, pNode);
    return QModelIndex();
}

QModelIndex CFilesModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SFileNode* pNode = static_cast<SFileNode*>(index.internalPointer());
	ASSERT(pNode->Parent);
	SFileNode* pParent = pNode->Parent;
    if (pParent == m_Root)
        return QModelIndex();

	int row = 0;
	if(pParent->Parent)
		row = pParent->Parent->Children.indexOf(pParent);
    return createIndex(row, 0, pParent);
}

int CFilesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

	SFileNode* pNode;
    if (!parent.isValid())
        pNode = m_Root;
    else
        pNode = static_cast<SFileNode*>(parent.internalPointer());
	return pNode->Children.count();
}

int CFilesModel::columnCount(const QModelIndex &parent) const
{
	return eCount + 1;
}

QVariant CFilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eFileName:		return tr("File Name");
			case eSize:			return tr("Size");
			case eType:			return tr("Type");
			case eStatus:		return tr("Status");
			case eProgress:		return tr("Progress");
			case eAvailability:	return tr("Availability");
			case eSources:		return tr("Sources");
			case eUpRate:		return tr("Upload");
			case eDownRate:		return tr("Download");
			case eUploaded:		return tr("Uploaded");
			case eDownloaded:	return tr("Downloaded");
			case eQueuePos:		return tr("Queue");
		}
	}
    return QVariant();
}
