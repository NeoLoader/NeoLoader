#pragma once
#include "../Common/Dialog.h"

class CTorrentCreator : public QDialogEx
{
	Q_OBJECT
public:
	CTorrentCreator(uint64 ID, QWidget *parent = 0);

	void				SetName(const QString& Name)		{m_pName->setText(Name);}
	void				SetComment(const QString& Comment)	{m_pComment->setPlainText(Comment);}

private slots:
	void				OnCreateTorrent();

	void				OnAddTracker();
	void				OnRemoveTracker();
	/*void				OnBootstrapAdd();
	void				OnBootstrapRemove();*/

protected:
	uint64				m_ID;

private:
	QTabWidget*			m_pCreatorTabs;

	QWidget*			m_pDetailsWidget;
	QLineEdit*			m_pCreator;
	QPlainTextEdit*		m_pComment;
	QLineEdit*			m_pName;
	QComboBox*			m_pPieceSize;
	QCheckBox*			m_pMerkle;
	QCheckBox*			m_pPrivate;

	QWidget*			m_pTrackersWidget;
	QLineEdit*			m_pTackerURL;
	QSpinBox*			m_pTackerTier;
	QPushButton*		m_pAddTracker;
	QPushButton*		m_pRemoveTracker;
	QTreeWidget*		m_pTrackerList;

	/*QWidget*			m_pBootstrapWidget;
	QLineEdit*			m_pBootstrapURL;
	QComboBox*			m_pBootstrapType;
	QPushButton*		m_pBootstrapAdd;
	QPushButton*		m_pBootstrapRemove;
	QTreeWidget*		m_pBootstrapList;*/

	QDialogButtonBox*	m_pButtonBox;
};