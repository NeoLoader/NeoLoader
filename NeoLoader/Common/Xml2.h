#pragma once

class CXml2
{
public:
	CXml2() {}

	QString Serialize(QVariant Variant, bool base64 = false);
	QVariant Parse(QString String, bool base64 = false);

protected:
	void Serialize(QVariant Variant, QXmlStreamWriter &xml, bool base64 = false, const QString& Name = "");
	bool Parse(QVariant &Variant, QXmlStreamReader &xml, bool base64 = false);
};
