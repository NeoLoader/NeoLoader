#pragma once

#include <QImage>

class CBarShader
{
public:
	CBarShader(uint64 uFileSize, uint32 iWidth, uint32 iHeight, int iDepth = 5);
	~CBarShader();

	void				FillRange(uint64 uStart, uint64 uEnd, uint Color);
	void				Fill(uint Color);

	void				DrawImage(QImage& Image, int iTop = 0, int iOffset = 0);

protected:
	void				FillRect(QImage& Image, int iTop, int iLeft, int iRight, float fRed, float fGreen, float fBlue);

	uint64				m_uFileSize;
	int					m_iWidth;
	int					m_iHeight;

	double				m_dPixelsPerByte;
	double				m_dBytesPerPixel;
	float*				m_Modifiers;

	typedef QMap<uint64,uint> TBarMap;
	TBarMap				m_Spans;
};