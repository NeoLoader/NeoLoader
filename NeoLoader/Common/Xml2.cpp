#include "GlobalHeader.h"
#include "Xml2.h"


QString CXml2::Serialize(QVariant Variant, bool base64)
{
	QString String;
	QXmlStreamWriter xml(&String);
#ifdef _DEBUG
	xml.setAutoFormatting(true);
#endif
	xml.writeStartDocument();
	Serialize(Variant, xml, base64);
	xml.writeEndDocument();
	return String;
}

void CXml2::Serialize(QVariant Variant, QXmlStreamWriter &xml, bool base64, const QString& Name)
{
	switch(Variant.type())
	{
		case QVariant::Map:
		{
			bool bEscape = false;
			QVariantMap VariantMap = Variant.toMap();
			foreach(const QString& Key, VariantMap.keys())
			{
				QVariant::Type Type = VariantMap.value(Key).type();
				if(Type == QVariant::List || Type == QVariant::StringList)
				{
					Serialize(VariantMap.value(Key), xml, base64, Key);
				}
				else if(Key.isEmpty())
				{
					if(base64)
						xml.writeCharacters (Variant.toByteArray().toBase64());
					else
						xml.writeCharacters (Variant.toString());
				}
				else if(Key.left(1) == "?")
				{
					if(base64)
						xml.writeAttribute(Key.mid(1), VariantMap.value(Key).toByteArray().toBase64());
					else
						xml.writeAttribute(Key.mid(1), VariantMap.value(Key).toString());
				}
				else if(Key == "\\")
					bEscape = true;
				else
				{
					xml.writeStartElement(Key);
					Serialize(VariantMap.value(Key), xml, base64, Key);
					xml.writeEndElement();
				}
			}
			if(bEscape)
				Serialize(VariantMap.value("\\"), xml, base64);
			break;
		}
		case QVariant::List:
		case QVariant::StringList:
		{
			QVariantList VariantList = Variant.toList();
			foreach (const QVariant &Entry, VariantList)
			{
				if(Name.isEmpty())
				{
					if(base64)
						xml.writeCharacters (Variant.toByteArray().toBase64());
					else
						xml.writeCharacters (Variant.toString());
				}
				else
				{
					xml.writeStartElement(Name);
					Serialize(Entry, xml, base64);
					xml.writeEndElement();
				}
			}
			break;
		}
		default:
			if(base64)
				xml.writeCharacters (Variant.toByteArray().toBase64());
			else
				xml.writeCharacters (Variant.toString());
	}
}

QVariant CXml2::Parse(QString String, bool base64)
{
	QXmlStreamReader xml(String);
	QVariant Variant;
	Parse(Variant, xml, base64);
	return Variant;
}

bool CXml2::Parse(QVariant &Variant, QXmlStreamReader &xml, bool base64)
{
	QVariant::Type eType = QVariant::Invalid;
	QStringList Text;
	while (!xml.atEnd())
	{
		xml.readNext();
		if (xml.error()) 
			break;
		if (xml.isEndDocument())
			continue;

		if (xml.isStartElement())
		{
			QString Name = xml.name().toString();

			QVariantMap Attributes;
			if(!xml.attributes().isEmpty())
			{
				for(int i=0; i < xml.attributes().count(); i++)
				{
					if(base64)
						Attributes.insert("?" + xml.attributes().at(i).name().toString(), QByteArray::fromBase64(xml.attributes().at(i).value().toString().toLatin1()));
					else
						Attributes.insert("?" + xml.attributes().at(i).name().toString(), xml.attributes().at(i).value().toString());
				}
			}

			QVariant NewVariant;
			Parse(NewVariant, xml, base64);

			if(!Attributes.isEmpty())
			{
				if(NewVariant.type() != QVariant::Map)
				{
					QVariantMap VariantMap;
					VariantMap.insert("\\", NewVariant);
					NewVariant = VariantMap;
				}

				QVariantMap VariantMap = NewVariant.toMap();				
				foreach(const QString& Key, Attributes.keys())
					VariantMap.insert(Key, Attributes.value(Key));
				NewVariant = VariantMap;
			}

			QVariantMap VariantMap = Variant.toMap();
			if(!VariantMap.contains(Name))
				VariantMap.insert(Name, NewVariant);
			else
			{
				QVariant OldVariant = VariantMap.value(Name);
				QVariantList VariantList;
				if(OldVariant.type() == QVariant::List)
					VariantList = OldVariant.toList();
				else
					VariantList.append(OldVariant);
				VariantList.append(NewVariant);
				VariantMap.insert(Name, VariantList);
			}
			Variant = VariantMap;
		}
		else if (xml.isCharacters())
		{
			if(base64)
				Text.append(QByteArray::fromBase64(xml.text().toString().toLatin1()));
			else
				Text.append(xml.text().toString());
		}
		else if (xml.isEndElement())
		{
			if(Variant.isValid())
			{
				ASSERT(Variant.type() == QVariant::Map);

				for(int i=0; i < Text.count(); i++)
				{
					QString Temp = Text[i].trimmed();
					if(Temp.isEmpty())
						Text.removeAt(i--);
					else
						Text[i] = Temp;
				}

				if(!Text.isEmpty())
				{
					QVariantMap VariantMap = Variant.toMap();
					VariantMap.insert("", Text);
					Variant = VariantMap;
				}
			}
			else if(!Text.isEmpty())
			{
				ASSERT(Text.count() == 1);
				Variant = Text.first();
			}
			return true;
		}
	}

	//ASSERT(0); // incomplete XML
	return false;
}
