#pragma once

class CKadConfig: public CObject
{
public:
	DECLARE_OBJECT(CKadConfig)

	CKadConfig(CObject* pParent = NULL);

	__inline bool		GetBool(const char* Name) const		{return GetSetting(Name);}
	__inline sint32		GetInt(const char* Name) const		{return GetSetting(Name);}
	__inline sint64		GetInt64(const char* Name) const	{return GetSetting(Name);}
	__inline wstring	GetString(const char* Name) const	{return GetSetting(Name);}

	CVariant			GetSetting(const char* Name) const;
	void				SetSetting(const char* Name, const CVariant& Value);

	CVariant			Get(const char* Name) const			{return m_Config;}
	void				Merge(const CVariant& Config);

protected:
	CVariant			m_Config;
};
