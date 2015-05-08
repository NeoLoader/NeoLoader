#pragma once

class CTransfersModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CTransfersModel(QObject *parent = 0);
	~CTransfersModel();

	void			Sync(const QVariantList& Transfers);
	void			IncrSync(const QVariantList& Transfers, UINT Mode);
	QModelIndex		FindIndex(uint64 SubID);

	void			Clear();

	QVariant		data(const QModelIndex &index, int role, int section) const;

	// derived functions
    QVariant		data(const QModelIndex &index, int role) const;
    Qt::ItemFlags	flags(const QModelIndex &index) const;
    QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex		parent(const QModelIndex &index) const;
    int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eUrl = 0,
		eFileName,
		eType,
		eSoftware,
		eStatus,
		eProgress,
		eUpRate,
		eDownRate,
		eUploaded,
		eDownloaded,
		eCount,
	};

protected:
	struct STransferNode
	{
		uint64 ID;
		uint64 SubID;
		QVariantMap	Transfer;
	};

	void Sync(QList<STransferNode*>& New, QMap<uint64, STransferNode*>& Old);

	QList<STransferNode*>			m_List;
	QMap<uint64, STransferNode*>	m_Map;
};