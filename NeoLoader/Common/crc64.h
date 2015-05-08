#pragma once

#define POLY64REV     0x95AC9329AC4BC9B5
//#define POLY64REV     0xC96C5795D7870F42
#define INITIALCRC    0xFFFFFFFFFFFFFFFF

class CCRC64
{
public:
	CCRC64()
	{
		unsigned __int64 part;
		for (unsigned short i = 0; i < 256; i++)
		{
			part = i;
			for (unsigned char j = 0; j < 8; j++)
			{
				if (part & 1)
					part = (part >> 1) ^ POLY64REV;
				else
					part >>= 1;
			}
			m_CRCTable[i] = part;
		}

		m_CRC64 = INITIALCRC;
	}

	void				Update(char *seq, unsigned int lg_seq)
	{
		while (lg_seq-- > 0)
			m_CRC64 = m_CRCTable[(m_CRC64 ^ *seq++) & 0xff] ^ (m_CRC64 >> 8);
			//m_CRC64 = m_CRCTable[(*seq++ ^ (m_CRC64 >> 56)) & 0xff] ^ (m_CRC64 << 8); // mirrored
	}

	unsigned __int64	Peek()		{return m_CRC64;}
	unsigned __int64	Finish()
	{
		//unsigned __int64 CRC64 = ~m_CRC64;
		unsigned __int64 CRC64 = m_CRC64;
		m_CRC64 = INITIALCRC; 
		return CRC64;
	}

private:
	unsigned __int64	m_CRC64;
	unsigned __int64	m_CRCTable[256];
};
