#pragma once


#define NEO_VERSION_MJR	0
#define NEO_VERSION_MIN 51
#define NEO_VERSION_UPD 0
#define NEO_VERSION_BLD 148

__inline QString GetNeoVersion(bool Long = false)
{
	QString Version = QString("NeoLoader v") + QString::number(NEO_VERSION_MJR) + "." + QString::number(NEO_VERSION_MIN).rightJustified(2, '0');
#if NEO_VERSION_UPD > 0
	Version.append('a' + NEO_VERSION_UPD - 1);
#endif
	if (Long)
		Version += " (Build " + QString::number(NEO_VERSION_BLD) + ")";
	return Version;
}

#define APP_ORGANISATION	"Neo"
#define APP_NAME			"NeoLoader"
#define APP_DOMAIN			"neoloader.com"