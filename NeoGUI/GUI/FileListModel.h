#pragma once

class CFilesModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CFilesModel(QObject *parent = 0);
	~CFilesModel();

	void			SetMode(UINT Mode)		{m_Mode = Mode;}

	void			Sync(const QVariantList& Files);
	void			IncrSync(const QVariantList& Files);
	void			CountFiles();
	QModelIndex		FindIndex(uint64 ID);

	void			Clear();

	QVariantMap		Data(const QModelIndex &index) const;

	QVariant		data(const QModelIndex &index, int role, int section) const;

	// derived functions
    QVariant		data(const QModelIndex &index, int role) const;
	bool			setData(const QModelIndex &index, const QVariant &value, int role);
    Qt::ItemFlags	flags(const QModelIndex &index) const;
    QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex		parent(const QModelIndex &index) const;
    int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eFileName = 0,
		eSize,
		eType,
		eStatus,
		eProgress,
		eAvailability,
		eSources,
		eUpRate,
		eDownRate,
		eUploaded,
		eDownloaded,
		eQueuePos,
		eCount,
	};

signals:
	void			CheckChanged(quint64 ID, bool State);
	void			Updated();

protected:
	struct SFileNode
	{
		SFileNode(uint64 Id){
			ID = Id;
			Parent = NULL;
			AllChildren = 0;
		}
		~SFileNode(){
			foreach(SFileNode* pNode, Children)
				delete pNode;
		}

		uint64				ID;
		QString				Name;
		QVariant			Icon;
		QVariantMap			File;
		SFileNode*			Parent;
		QString				Path;
		QList<SFileNode*>	Children;
		int					AllChildren;
		QMap<QString, int>	Aux;
		QString				Ext;
	};

	void			Sync(QMap<QString, QList<SFileNode*> >& New, QMap<uint64, SFileNode*>& Old);
	void			Purge(SFileNode* pParent, const QModelIndex &parent, QMap<uint64, SFileNode*> &Old);
	void			Fill(SFileNode* pParent, const QModelIndex &parent, const QStringList& Paths, int PathsIndex, const QList<SFileNode*>& New, const QString& Path);
	QModelIndex		Find(SFileNode* pParent, SFileNode* pNode);
	int				CountFiles(SFileNode* pRoot);

	SFileNode*					m_Root;
	QMultiMap<uint64, SFileNode*>	m_Map;

	UINT						m_Mode;
};