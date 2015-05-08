#pragma once

#include <QWidget>
#include <QDialog>
#include <QFormLayout>
#include <QTreeWidget>
#include <QMenu>
#include <QDialogButtonBox>
#include <QVariant>
#include <QLineEdit>
#include <QPushButton>

class CScriptRepositoryDialog: public QDialog
{
    Q_OBJECT
public:
    CScriptRepositoryDialog(QWidget *parent = 0);
    ~CScriptRepositoryDialog();

	QString SelectScript(const QString& Action, const QString& DefaultName = "");

	void SyncScripts(const QVariant& Scripts);

public slots:
	void OnItemClicked(QTreeWidgetItem*, int);
	void OnItemDoubleClicked(QTreeWidgetItem*, int);

protected:
	QTreeWidget* m_pView;
	QLineEdit* m_pEdit;
	QDialogButtonBox* m_pButtonBox;

    Q_DISABLE_COPY(CScriptRepositoryDialog)
};
