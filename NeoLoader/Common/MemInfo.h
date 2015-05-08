#pragma once

class CMemInfo
{
public:
	CMemInfo();
	~CMemInfo();

	quint64		Get();

protected:
	struct SMemInfo*	m_p;
};