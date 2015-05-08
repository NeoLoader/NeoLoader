#include "GlobalHeader.h"
#include "TransfersModel.h"
#include "ProgressBar.h"
#include "../NeoGUI.h"
#include "ServicesWidget.h"
#include "../Common/Common.h"

CTransfersModel::CTransfersModel(QObject *parent)
:QAbstractItemModel(parent)
{
}

CTransfersModel::~CTransfersModel()
{
	foreach(STransferNode* pNode, m_List)
		delete pNode;
}

void CTransfersModel::IncrSync(const QVariantList& Transfers, UINT Mode)
{
	QList<STransferNode*> New;
	QMap<uint64, STransferNode*> Old;

	foreach (const QVariant vTransfer, Transfers)
	{
		QVariantMap Transfer = vTransfer.toMap();
		uint64 SubID = Transfer["ID"].toULongLong();

		bool bUpdate = true;
		STransferNode* pNode = m_Map.value(SubID);
		if(Transfer.value("TransferStatus") == "Deprecated")
		{
			if(pNode)
				Old.insert(SubID, pNode);
			continue;
		}

		if(!pNode)
		{
			pNode = new STransferNode();
			pNode->SubID = SubID;
			pNode->ID = Transfer["FileID"].toULongLong();
			New.append(pNode);
			bUpdate = false;
		}

		for(QVariantMap::iterator I = Transfer.begin(); I != Transfer.end(); I++)
			pNode->Transfer[I.key()] = I.value();

		if(!bUpdate)
			continue;

		//
		int Row = m_List.indexOf(pNode);
		ASSERT(Row != -1);
		//emit dataChanged(createIndex(Row, 0), createIndex(Row, columnCount()-1));
		int Col = 0;
		bool State = false;
		for(int i = 0; i < columnCount(); i++)
		{
			bool Changed = false;
			switch(i)
			{
				case eUrl:			Changed = (Transfer.contains("Url")); break;
				case eFileName:		Changed = (Transfer.contains("FileName") || Transfer.contains("FileSize")); break;
				case eType:			Changed = (Transfer.contains("Type") || Transfer.contains("Found")); break;
				case eSoftware:		Changed = (Transfer.contains("Software")); break;
				case eStatus:		Changed = (Transfer.contains("TransferStatus") || Transfer.contains("UploadState") || Transfer.contains("DownloadState") || Transfer.contains("TransferInfo"));  break;
				case eProgress:		Changed = (Transfer.contains("Progress")); break;
				case eUpRate:		Changed = (Transfer.contains("Upload") || Transfer.contains("UpRate")); break;
				case eDownRate:		Changed = (Transfer.contains("Download") || Transfer.contains("DownRate")); break;
				case eUploaded:		Changed = (Transfer.contains("Uploaded")); break;
				case eDownloaded:	Changed = (Transfer.contains("Downloaded")); break;
			}

			if(State != Changed)
			{
				if(State)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, i-1));
				State = Changed;
				Col = i;
			}
		}
		if(State)
			emit dataChanged(createIndex(Row, Col), createIndex(Row, columnCount()-1));
		//
	}

	Sync(New, Old);
}

void CTransfersModel::Sync(const QVariantList& Transfers)
{
	QList<STransferNode*> New;
	QMap<uint64, STransferNode*> Old = m_Map;

	foreach (const QVariant vTransfer, Transfers)
	{
		QVariantMap Transfer = vTransfer.toMap();
		uint64 SubID = Transfer["ID"].toULongLong();

		bool bUpdate = true;
		STransferNode* &pNode = Old[SubID];
		if(!pNode)
		{
			bUpdate = false;
			pNode = new STransferNode();
			pNode->SubID = SubID;
			pNode->ID = Transfer["FileID"].toULongLong();
			New.append(pNode);
		}

		if(pNode->Transfer != Transfer)
		{
			QVariantMap OldTransfer = pNode->Transfer;
			pNode->Transfer = Transfer;

			if(bUpdate)
			{
				int Row = m_List.indexOf(pNode);
				ASSERT(Row != -1);
				//emit dataChanged(createIndex(Row, 0), createIndex(Row, columnCount()-1));
				int Col = 0;
				bool State = false;
				for(int i = 0; i < columnCount(); i++)
				{
					bool Changed = false;
					switch(i)
					{
						case eUrl:			Changed = (OldTransfer["Url"] != Transfer["Url"]); break;
						case eFileName:		Changed = (OldTransfer["FileName"] != Transfer["FileName"] || OldTransfer["FileSize"] != Transfer["FileSize"]); break;
						case eType:			Changed = (OldTransfer["Type"] != Transfer["Type"] || OldTransfer["Found"] != Transfer["Found"]); break;
						case eSoftware:		Changed = (OldTransfer["Software"] != Transfer["Software"]); break;
						case eStatus:		Changed = (OldTransfer["TransferStatus"] != Transfer["TransferStatus"] || OldTransfer["UploadState"] != Transfer["UploadState"] || OldTransfer["DownloadState"] != Transfer["DownloadState"] || OldTransfer["TransferInfo"] != Transfer["TransferInfo"]); break;
						case eProgress:		Changed = (OldTransfer["Progress"] != Transfer["Progress"]); break;
						case eUpRate:		Changed = (OldTransfer["Upload"] != Transfer["Upload"] || OldTransfer["UpRate"] != Transfer["UpRate"]); break;
						case eDownRate:		Changed = (OldTransfer["Download"] != Transfer["Download"] || OldTransfer["DownRate"] != Transfer["DownRate"]); break;
						case eUploaded:		Changed = (OldTransfer["Uploaded"] != Transfer["Uploaded"]); break;
						case eDownloaded:	Changed = (OldTransfer["Downloaded"] != Transfer["Downloaded"]); break;
					}

					if(State != Changed)
					{
						if(State)
							emit dataChanged(createIndex(Row, Col), createIndex(Row, i-1));
						State = Changed;
						Col = i;
					}
				}
				if(State)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, columnCount()-1));
			}
		}

		pNode = NULL;
	}

	Sync(New, Old);
}

void CTransfersModel::Sync(QList<STransferNode*>& New, QMap<uint64, STransferNode*>& Old)
{
	int Begin = -1;
	int End = -1;
	for(int i = m_List.count()-1; i >= -1; i--) 
	{
		uint64 ID = i >= 0 ? m_List[i]->SubID : -1;
		if(ID != -1 && Old.value(ID)) // remove it
		{
			m_Map.remove(ID);
			if(End == -1)
				End = i;
		}
		else if(End != -1) // keep it and remove whatis to be removed at once
		{
			Begin = i + 1;

			beginRemoveRows(QModelIndex(), Begin, End);
			for(int j = End; j >= Begin; j--)
				delete m_List.takeAt(j);
			endRemoveRows();

			End = -1;
			Begin = -1;
		}
    }

	Begin = m_List.count();
	for(QList<STransferNode*>::iterator I = New.begin(); I != New.end(); I++)
	{
		STransferNode* pNode = *I;
		m_Map.insert(pNode->SubID, pNode);
		m_List.append(pNode);
	}
	End = m_List.count();
	if(Begin < End)
	{
		beginInsertRows(QModelIndex(), Begin, End-1);
		endInsertRows();
	}
}

QModelIndex CTransfersModel::FindIndex(uint64 SubID)
{
	if(STransferNode* pNode = m_Map.value(SubID))
	{
		int row = m_List.indexOf(pNode);
		ASSERT(row != -1);
		return createIndex(row, eUrl, pNode);
	}
	return QModelIndex();
}

void CTransfersModel::Clear()
{
	//beginResetModel();
	beginRemoveRows(QModelIndex(), 0, rowCount());
	foreach(STransferNode* pNode, m_List)
		delete pNode;
	m_List.clear();
	m_Map.clear();
	endRemoveRows();
	//endResetModel();
}

QVariant CTransfersModel::data(const QModelIndex &index, int role) const
{
    return data(index, role, index.column());
}

QVariant CTransfersModel::data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole )
    //    return QSize(64,16); // for fixing height

	STransferNode* pNode = static_cast<STransferNode*>(index.internalPointer());

	if(role == Qt::DecorationRole)
	{
		if (section == eUrl)
		{
			if(pNode->Transfer["Type"] == "Torrent Peer")
				return QIcon(QPixmap(":/Icons/Sources/torrent.png"));
			else if(pNode->Transfer["Type"] == "Mule Source")
				return QIcon(QPixmap(":/Icons/Sources/emule.png"));
			else if(pNode->Transfer["Type"] == "Neo Entity")
				return QIcon(QPixmap(":/Icons/Sources/neo.png"));
			else
				return theGUI->GetHosterIcon(pNode->Transfer["Software"].toString());
		}
		return QVariant();
	}

    switch(role)
	{
		case Qt::DisplayRole:
		{
			switch(section)
			{
				case eUrl:			return pNode->Transfer["Url"];
				case eFileName:		if(!pNode->Transfer.contains("FileSize")) return pNode->Transfer["FileName"];
									else return tr("%1 (%2)").arg(pNode->Transfer["FileName"].toString()).arg(FormatSize(pNode->Transfer["FileSize"].toULongLong()));
				case eType:			return tr("%1 @ %2").arg(pNode->Transfer["Type"].toString()).arg(pNode->Transfer["Found"].toString());
				case eSoftware:		return pNode->Transfer["Software"];
				case eStatus:
					{
						QString Status = pNode->Transfer["TransferStatus"].toString();
						QString Info = pNode->Transfer["TransferInfo"].toString();
						if(!Info.isEmpty())
							Status = Info;
						Status += " " + pNode->Transfer["UploadState"].toString() + "/" + pNode->Transfer["DownloadState"].toString();
						QString InfoEx = pNode->Transfer["SourceInfo"].toString();
						if(!InfoEx.isEmpty())
							Status += " [" + InfoEx + "]";
						return Status;
					}
				case eProgress:		return tr("%1 %").arg(pNode->Transfer["Progress"].toString());
				case eUpRate:
				{
					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
					{
						QString Info;
						QString Status = pNode->Transfer["UploadState"].toString();
						if(Status == "Tansfering")
						{
							if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
								Info = " - " + FormatTime(pNode->Transfer["UploadDuration"].toULongLong()/1000);
							if (theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
								Info = " - " + FormatSize(pNode->Transfer["PendingBytes"].toULongLong()) + " / " + QString::number(pNode->Transfer["PendingBlocks"].toInt());

							if(pNode->Transfer["IsFocused"].toBool())
								Info += " !";
							else if(pNode->Transfer["IsTrickle"].toBool())
								Info += " *";
							if(pNode->Transfer["IsBlocking"].toBool())
								Info += " #";
							if(pNode->Transfer.contains("Horde"))
								Info += " Horde: " + (pNode->Transfer["Horde"].toString().isEmpty() ? "No" : pNode->Transfer["Horde"].toString());
						}
						return FormatSize(pNode->Transfer["Upload"].toULongLong()) + "/s (" + FormatSize(pNode->Transfer["UpRate"].toULongLong()) + "/s)" + Info;
					}
					else
						return FormatSize(pNode->Transfer["Upload"].toULongLong()) + "/s";

				}
				case eDownRate:
				{
					if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
					{
						QString Info;
						QString Status = pNode->Transfer["DownloadState"].toString();
						if(Status == "Tansfering")
						{
							if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
								Info = " - " + FormatTime(pNode->Transfer["DownloadDuration"].toULongLong()/1000);
							if (theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
								Info = " - " + FormatSize(pNode->Transfer["ReservedBytes"].toULongLong()) + " / " + QString::number(pNode->Transfer["RequestedBlocks"].toInt());
						}
						return FormatSize(pNode->Transfer["Download"].toULongLong()) + "/s (" + FormatSize(pNode->Transfer["DownRate"].toULongLong()) + "/s)" + Info;
					}
					else
						return FormatSize(pNode->Transfer["Download"].toULongLong()) + "/s";
				}
				case eUploaded:		return (pNode->Transfer.contains("LastUploaded") ? FormatSize(pNode->Transfer["LastUploaded"].toULongLong()) : "-") + "/" + FormatSize(pNode->Transfer["Uploaded"].toULongLong());
				case eDownloaded:	return (pNode->Transfer.contains("LastDownloaded") ? FormatSize(pNode->Transfer["LastDownloaded"].toULongLong()) : "-") + "/" + FormatSize(pNode->Transfer["Downloaded"].toULongLong());
			}
			break;
		}
		case Qt::EditRole:
		{
			switch(section)
			{
				case eStatus:		return (pNode->Transfer["TransferStatus"].toString() + " " + pNode->Transfer["UploadState"].toString() + "/" + pNode->Transfer["DownloadState"].toString()).toLower();
				case eProgress:		return pNode->Transfer["Progress"];
				case eUpRate:		return pNode->Transfer["UpRate"];
				case eDownRate:		return pNode->Transfer["DownRate"];
				case eUploaded:		return pNode->Transfer["Uploaded"];
				case eDownloaded:	return pNode->Transfer["Downloaded"];
				default:			return data(index, Qt::DisplayRole, section).toString().toLower();
			}
			break;
		}
		case Qt::UserRole:
		{
			switch(section)
			{
				case eUrl:			return pNode->SubID;
				case eFileName:		return pNode->ID;
			};
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CTransfersModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CTransfersModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) 
        return QModelIndex();
	if(m_List.count() > row)
		return createIndex(row, column, m_List[row]);
	return QModelIndex();
}

QModelIndex CTransfersModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int CTransfersModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (parent.isValid())
        return 0;
	return m_List.count();
}

int CTransfersModel::columnCount(const QModelIndex &parent) const
{
	return eCount + 1;
}

QVariant CTransfersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eUrl:			return tr("URL");
			case eFileName:		return tr("FileName");
			case eType:			return tr("Type");
			case eSoftware:		return tr("Software");
			case eStatus:		return tr("Status");
			case eProgress:		return tr("Progress");
			case eUpRate:		return tr("Upload");
			case eDownRate:		return tr("Download");
			case eUploaded:		return tr("Uploaded");
			case eDownloaded:	return tr("Downloaded");
		}
	}
    return QVariant();
}
