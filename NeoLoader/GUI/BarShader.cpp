#include "GlobalHeader.h"
#include "BarShader.h"
#include <math.h>

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#define HALF(X) (((X) + 1) / 2)

CBarShader::CBarShader(uint64 uFileSize, uint32 iWidth, uint32 iHeight, int iDepth)
{
	m_uFileSize = uFileSize;
	m_iWidth = iWidth;

	if (m_uFileSize > (uint64)0)
		m_dPixelsPerByte = (double)m_iWidth / (uint64)m_uFileSize;
	else
		m_dPixelsPerByte = 0.0;

	if (m_iWidth)
		m_dBytesPerPixel = (double)m_uFileSize / m_iWidth;
	else
		m_dBytesPerPixel = 0.0;


	m_iHeight = iHeight;

	int Depth = (7-iDepth); // 2 gives greatest depth and the value cant be less, the higher depth value, the flatter the appearance
	int Count = HALF(m_iHeight);
	double piOverDepth = M_PI/Depth;
	double base = piOverDepth * ((Depth / 2.0) - 1);
	double increment = piOverDepth / (Count - 1);

	m_Modifiers = new float[Count];
	for (int i = 0; i < Count; i++)
		m_Modifiers[i] = (float)(sin(base + i * increment));
	

	m_Spans[0] = 0;

	QImage Image(m_iWidth, m_iHeight, QImage::Format_RGB32);
}

CBarShader::~CBarShader()
{
	delete[] m_Modifiers;
}

void CBarShader::FillRange(uint64 uStart, uint64 uEnd, uint Color) 
{
	ASSERT(uEnd <= m_uFileSize);
	if(uStart == uEnd || m_iWidth == 0)
		return;
	if(uStart > uEnd)
	{
		ASSERT(0);
		return;
	}

	TBarMap::iterator EndPos = m_Spans.upperBound(uEnd+1);

	if (EndPos != m_Spans.end())
		EndPos--;
	else
		EndPos = m_Spans.find(m_uFileSize);

	//ASSERT(EndPos != m_Spans.end());

	uint EndColor = EndPos.value();
	EndPos = m_Spans.insert(uEnd,EndColor);

	for (TBarMap::iterator Pos = m_Spans.upperBound(uStart+1); Pos != EndPos && Pos != m_Spans.end();)
		Pos = m_Spans.erase(Pos);
	
	EndPos--;

	if (EndPos.value() != Color)
		m_Spans[uStart] = Color;
}

void CBarShader::Fill(uint Color) 
{
	m_Spans.clear();
	m_Spans[0] = Color;
	m_Spans[m_uFileSize] = 0;
}

void CBarShader::DrawImage(QImage& Image, int iTop, int iOffset)
{
	uint64 uBytesInOnePixel = (uint64)(m_dBytesPerPixel + 0.5f);
	uint64 uStart = 0;

	TBarMap::iterator Pos = m_Spans.begin();
	uint Color = Pos.value();
	Pos++;

	if(iTop < 0)
		iTop += Image.height();

	int iLeft = 0;
	int iRight = 0;

	while(Pos != m_Spans.end() && iRight < m_iWidth)
	{
		uint64 uSpan = Pos.key() - uStart;
		uint64 uPixels = (uint64)(uSpan * m_dPixelsPerByte + 0.5f);

		if (uPixels > 0)
		{
			iLeft = iRight;
			iRight += (int)uPixels;
			FillRect(Image, iTop, iOffset + iLeft, iOffset + iRight, qRed(Color), qGreen(Color), qBlue(Color));
			uStart += (uint64)(uPixels * m_dBytesPerPixel + 0.5f);
		} 
		else 
		{
			float fRed = 0;
			float fGreen = 0;
			float fBlue = 0;

			uint64 uEnd = uStart + uBytesInOnePixel;
			uint64 uLast = uStart;

			do
			{
				float fWeight = (float)((min(Pos.key(), uEnd) - uLast) * m_dPixelsPerByte);
				fRed   += qRed(Color) * fWeight;
				fGreen += qGreen(Color) * fWeight;
				fBlue  += qBlue(Color) * fWeight;
				if(Pos.key() > uEnd)
					break;
				uLast = Pos.key();
				Color = Pos.value();
				Pos++;
			} 
			while(Pos != m_Spans.end());
			
			iLeft = iRight;
			iRight++;
			FillRect(Image, iTop, iOffset + iLeft, iOffset + iRight, fRed, fGreen, fBlue);
			uStart += uBytesInOnePixel;
		}
		
		while(Pos != m_Spans.end() && Pos.key() < uStart) 
		{
			Color = Pos.value();
			Pos++;
		}
	}
}

void CBarShader::FillRect(QImage& Image, int iTop, int iLeft, int iRight, float fRed, float fGreen, float fBlue)
{
	int iMax = HALF(m_iHeight);
	for(int i = 0; i < iMax; i++) 
	{
		uint Color = qRgb((int)(fRed * m_Modifiers[i] + .5f), (int)(fGreen * m_Modifiers[i] + .5f), (int)(fBlue * m_Modifiers[i] + .5f));
		for(int j=iLeft; j<iRight;j++)
		{
			Image.setPixel(j,iTop+i,Color);
			Image.setPixel(j,iTop+m_iHeight-i-1,Color);
		}
	}
}
