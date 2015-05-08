#include "GlobalHeader.h"
#include "Dialog.h"

int QDialogEx::m_OpenCount = 0;

QDialogEx::QDialogEx(QWidget *parent)
	: QDialog(parent)
{
	m_OpenCount++;
}

QDialogEx::~QDialogEx()
{
	m_OpenCount--;
}