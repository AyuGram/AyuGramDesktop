/*
 MIT License

 Copyright (c) 2020-2021 Agadzhanov Vladimir
 Portions Copyright (c) 2021 Jerry Jacobs

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
                                                                                 */

#ifndef QSERIALIZER_H
#define QSERIALIZER_H

/* JSON */
#ifdef QS_HAS_JSON
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#endif

/* XML */
#ifdef QS_HAS_XML
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#endif

/* META OBJECT SYSTEM */
#include <QVariant>
#include <QMetaProperty>
#include <QMetaObject>
#include <QMetaType>

#include <type_traits>
#include <QDebug>

#define QS_VERSION "1.2"

/* Generate metaObject method */
#define QS_META_OBJECT_METHOD \
    virtual const QMetaObject * metaObject() const { \
        return &this->staticMetaObject; \
    } \

#define QSERIALIZABLE \
    Q_GADGET \
    QS_META_OBJECT_METHOD

/* Mark class as serializable */
#define QS_SERIALIZABLE QS_META_OBJECT_METHOD

#ifdef QS_HAS_XML
Q_DECLARE_METATYPE(QDomNode)
Q_DECLARE_METATYPE(QDomElement)
#endif

class QSerializer {
Q_GADGET
    QS_SERIALIZABLE
public:
    virtual ~QSerializer() = default;

#ifdef QS_HAS_JSON
    /*! \brief  Convert QJsonValue in QJsonDocument as QByteArray. */
    static QByteArray toByteArray(const QJsonValue & value){
        return QJsonDocument(value.toObject()).toJson();
    }
#endif

#ifdef QS_HAS_XML
    /*! \brief  Convert QDomNode in QDomDocument as QByteArray. */
    static QByteArray toByteArray(const QDomNode & value) {
        QDomDocument doc = value.toDocument();
        return doc.toByteArray();
    }

    /*! \brief  Make xml processing instruction (hat) and returns new XML QDomDocument. On deserialization procedure all processing instructions will be ignored. */
    static QDomDocument appendXmlHat(const QDomNode &node, const QString & encoding, const QString & version = "1.0"){
        QDomDocument doc = node.toDocument();
        QDomNode xmlNode = doc.createProcessingInstruction("xml", QString("version=\"%1\" encoding=\"%2\"").arg(version).arg(encoding));
        doc.insertBefore(xmlNode, doc.firstChild());
        return doc;
    }
#endif


#ifdef QS_HAS_JSON
    /*! \brief  Serialize all accessed JSON propertyes for this object. */
    QJsonObject toJson() const {
        QJsonObject json;
        for(int i = 0; i < metaObject()->propertyCount(); i++)
        {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if(QString(metaObject()->property(i).typeName()) != QMetaType::typeName(qMetaTypeId<QJsonValue>())) {
                continue;
            }
#else
            if(metaObject()->property(i).metaType().id() != QMetaType::QJsonValue) {
                continue;
            }
#endif

            json.insert(metaObject()->property(i).name(), metaObject()->property(i).readOnGadget(this).toJsonValue());
        }
        return json;
    }

    /*! \brief  Returns QByteArray representation this object using json-serialization. */
    QByteArray toRawJson() const {
        return toByteArray(toJson());
    }

    /*! \brief  Deserialize all accessed XML propertyes for this object. */
    void fromJson(const QJsonValue & val) {
        if(val.isObject())
        {
            QJsonObject json = val.toObject();
            QStringList keys = json.keys();
            int propCount = metaObject()->propertyCount();
            for(int i = 0; i < propCount; i++)
            {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                if(QString(metaObject()->property(i).typeName()) != QMetaType::typeName(qMetaTypeId<QJsonValue>())) {
                    continue;
                }
#else
                if(metaObject()->property(i).metaType().id() != QMetaType::QJsonValue) {
                    continue;
                }
#endif

                for(auto key : json.keys())
                {
                    if(key == metaObject()->property(i).name())
                    {
                        metaObject()->property(i).writeOnGadget(this, json.value(key));
                        break;
                    }
                }
            }
        }
    }

    /*! \brief  Deserialize all accessed JSON propertyes for this object. */
    void fromJson(const QByteArray & data) {
        fromJson(QJsonDocument::fromJson(data).object());
    }
#endif // QS_HAS_JSON

#ifdef QS_HAS_XML
    /*! \brief  Serialize all accessed XML propertyes for this object. */
    QDomNode toXml() const {
        QDomDocument doc;
        QDomElement el = doc.createElement(metaObject()->className());
        for(int i = 0; i < metaObject()->propertyCount(); i++)
        {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if(QString(metaObject()->property(i).typeName()) != QMetaType::typeName(qMetaTypeId<QDomNode>())) {
                continue;
            }
#else
            if(metaObject()->property(i).metaType().id() != qMetaTypeId<QDomNode>()) {
                continue;
            }
#endif
            el.appendChild(QDomNode(metaObject()->property(i).readOnGadget(this).value<QDomNode>()));
        }
        doc.appendChild(el);
        return doc;
    }

    /*! \brief  Returns QByteArray representation this object using xml-serialization. */
    QByteArray toRawXml() const {
        return toByteArray(toXml());
    }

    /*! \brief  Deserialize all accessed XML propertyes for this object. */
    void fromXml(const QDomNode & val) {
        QDomNode doc = val;

        auto n = doc.firstChildElement(metaObject()->className());

        if(!n.isNull()) {
            for(int i = 0; i < metaObject()->propertyCount(); i++) {
                QString name = metaObject()->property(i).name();
                QDomElement tmp = metaObject()->property(i).readOnGadget(this).value<QDomNode>().firstChildElement();

                auto f = n.firstChildElement(tmp.tagName());
                metaObject()->property(i).writeOnGadget(this, QVariant::fromValue<QDomNode>(f));
            }
        }
        else
        {
            for(int i = 0; i < metaObject()->propertyCount(); i++) {
                QString name = metaObject()->property(i).name();
                auto f = doc.firstChildElement(name);
                metaObject()->property(i).writeOnGadget(this, QVariant::fromValue<QDomNode>(f));
            }
        }
    }

    /*! \brief  Deserialize all accessed XML propertyes for this object. */
    void fromXml(const QByteArray & data) {
        QDomDocument d;
        d.setContent(data);
        fromXml(d);
    }
#endif // QS_HAS_XML
};

#define GET(prefix, name) get_##prefix##_##name
#define SET(prefix, name) set_##prefix##_##name

/* Create variable */
#define QS_DECLARE_MEMBER(type, name)                                                       \
    public :                                                                                \
    type name = type();                                                                     \

/* Create JSON property and methods for primitive type field*/
#ifdef QS_HAS_JSON
#define QS_JSON_FIELD(type, name)                                                           \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json, name))                  \
    private:                                                                                \
        QJsonValue GET(json, name)() const {                                                \
            QJsonValue val = QJsonValue::fromVariant(QVariant(name));                       \
            return val;                                                                     \
        }                                                                                   \
        void SET(json, name)(const QJsonValue & varname){                                   \
            name = varname.toVariant().value<type>();                                       \
        }
#else
#define QS_JSON_FIELD(type, name)
#endif

/* Create XML property and methods for primitive type field*/
#ifdef QS_HAS_XML
#define QS_XML_FIELD(type, name)                                                            \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                      \
    private:                                                                                \
    QDomNode GET(xml, name)() const {                                                       \
        QDomDocument doc;                                                                   \
        QString strname = #name;                                                            \
        QDomElement element = doc.createElement(strname);                                   \
        QDomText valueOfProp = doc.createTextNode(QVariant(name).toString());               \
        element.appendChild(valueOfProp);                                                   \
        doc.appendChild(element);                                                           \
        return  QDomNode(doc);                                                              \
    }                                                                                       \
    void SET(xml, name)(const QDomNode &node) {                                             \
        if(!node.isNull() && node.isElement()){                                             \
            QDomElement domElement = node.toElement();                                      \
            if(domElement.tagName() == #name)                                               \
                name = QVariant(domElement.text()).value<type>();                           \
        }                                                                                   \
    }
#else
#define QS_XML_FIELD(type, name)
#endif

/* Generate JSON-property and methods for primitive type objects */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#ifdef QS_HAS_JSON
#define QS_JSON_ARRAY(itemType, name)                                                       \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json, name))                  \
    private:                                                                                \
        QJsonValue GET(json, name)() const {                                                \
            QJsonArray val;                                                                 \
            for(int i = 0; i < name.size(); i++)                                            \
                val.push_back(name.at(i));                                                  \
            return QJsonValue::fromVariant(val);                                            \
        }                                                                                   \
        void SET(json, name)(const QJsonValue & varname) {                                  \
            if(!varname.isArray())                                                          \
                return;                                                                     \
            name.clear();                                                                   \
            QJsonArray val = varname.toArray();                                             \
            for(auto item : val) {                                                          \
                itemType tmp;                                                               \
                tmp = item.toVariant().value<itemType>();                                   \
                name.append(tmp);                                                           \
            }                                                                               \
        }
#else
#define QS_JSON_ARRAY(itemType, name)
#endif

/* Generate XML-property and methods for primitive type objects */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#ifdef QS_HAS_XML
#define QS_XML_ARRAY(itemType, name)                                                        \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                      \
    private:                                                                                \
        QDomNode GET(xml, name)() const {                                                   \
            QDomDocument doc;                                                               \
            QString strname = #name;                                                        \
            QDomElement arrayXml = doc.createElement(QString(strname));                     \
            arrayXml.setAttribute("type", "array");                                         \
                                                                                            \
            for(int i = 0; i < name.size(); i++) {                                          \
                itemType item = name.at(i);                                                 \
                QDomElement itemXml = doc.createElement("item");                            \
                itemXml.setAttribute("type", #itemType);                                    \
                itemXml.setAttribute("index", i);                                           \
                itemXml.appendChild(doc.createTextNode(QVariant(item).toString()));         \
                arrayXml.appendChild(itemXml);                                              \
            }                                                                               \
                                                                                            \
            doc.appendChild(arrayXml);                                                      \
            return  QDomNode(doc);                                                          \
        }                                                                                   \
        void SET(xml, name)(const QDomNode & node) {                                        \
            QDomNode domNode = node.firstChild();                                           \
            name.clear();                                                                   \
            while(!domNode.isNull()) {                                                      \
                if(domNode.isElement()) {                                                   \
                    QDomElement domElement = domNode.toElement();                           \
                    name.append(QVariant(domElement.text()).value<itemType>());             \
                }                                                                           \
                domNode = domNode.nextSibling();                                            \
            }                                                                               \
        }
#else
#define QS_XML_ARRAY(itemType, name)
#endif


/* Generate JSON-property and methods for some custom class */
/* Custom type must be provide methods fromJson and toJson or inherit from QSerializer */
#ifdef QS_HAS_JSON
#define QS_JSON_OBJECT(type, name)                                                          \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json, name))                  \
    private:                                                                                \
    QJsonValue GET(json, name)() const {                                                    \
        QJsonObject val = name.toJson();                                                    \
        return QJsonValue(val);                                                             \
    }                                                                                       \
    void SET(json, name)(const QJsonValue & varname) {                                      \
        if(!varname.isObject())                                                             \
        return;                                                                             \
        name.fromJson(varname);                                                             \
    }
#else
#define QS_JSON_OBJECT(type, name)
#endif

/* Generate XML-property and methods for some custom class */
/* Custom type must be provide methods fromJson and toJson or inherit from QSerializer */
#ifdef QS_HAS_XML
#define QS_XML_OBJECT(type, name)                                                           \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                      \
    private:                                                                                \
        QDomNode GET(xml, name)() const {                                                   \
            return name.toXml();                                                            \
        }                                                                                   \
        void SET(xml, name)(const QDomNode & node){                                         \
            name.fromXml(node);                                                             \
        }
#else
#define QS_XML_OBJECT(type, name)
#endif

/* Generate JSON-property and methods for collection of custom type objects */
/* Custom item type must be provide methods fromJson and toJson or inherit from QSerializer */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#ifdef QS_HAS_JSON
#define QS_JSON_ARRAY_OBJECTS(itemType, name)                                               \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json, name))                  \
    private:                                                                                \
        QJsonValue GET(json, name)() const {                                                \
            QJsonArray val;                                                                 \
            for(int i = 0; i < name.size(); i++)                                            \
                val.push_back(name.at(i).toJson());                                         \
            return QJsonValue::fromVariant(val);                                            \
        }                                                                                   \
        void SET(json, name)(const QJsonValue & varname) {                                  \
            if(!varname.isArray())                                                          \
                return;                                                                     \
            name.clear();                                                                   \
            QJsonArray val = varname.toArray();                                             \
            for(int i = 0; i < val.size(); i++) {                                           \
                itemType tmp;                                                               \
                tmp.fromJson(val.at(i));                                                    \
                name.append(tmp);                                                           \
            }                                                                               \
        }
#else
#define QS_JSON_ARRAY_OBJECTS(itemType, name)
#endif

/* Generate XML-property and methods for collection of custom type objects  */
/* Custom type must be provide methods fromXml and toXml or inherit from QSerializer */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#ifdef QS_HAS_XML
#define QS_XML_ARRAY_OBJECTS(itemType, name)                                                \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                      \
    private:                                                                                \
    QDomNode GET(xml, name)() const {                                                       \
        QDomDocument doc;                                                                   \
        QDomElement element = doc.createElement(#name);                                     \
        element.setAttribute("type", "array");                                              \
        for(int i = 0; i < name.size(); i++)                                                \
            element.appendChild(name.at(i).toXml());                                        \
        doc.appendChild(element);                                                           \
        return QDomNode(doc);                                                               \
    }                                                                                       \
    void SET(xml, name)(const QDomNode & node) {                                            \
        name.clear();                                                                       \
        QDomNodeList nodesList = node.childNodes();                                         \
        for(int i = 0;  i < nodesList.size(); i++) {                                        \
            itemType tmp;                                                                   \
            tmp.fromXml(nodesList.at(i));                                                   \
            name.append(tmp);                                                               \
        }                                                                                   \
    }
#else
#define QS_XML_ARRAY_OBJECTS(itemType, name)
#endif

/* Generate JSON-property and methods for dictionary of simple fields (int, bool, QString, ...)  */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be QMap, QHash)    */
/* THIS IS FOR QT DICTIONARY TYPES, for example QMap<int, QString>, QMap<int,int>, ...*/
#ifdef QS_HAS_JSON
#define QS_JSON_QT_DICT(map, name)                                                          \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json,name))                   \
    private:                                                                                \
    QJsonValue GET(json, name)() const {                                                    \
        QJsonObject val;                                                                    \
        for(auto p = name.constBegin(); p != name.constEnd(); ++p) {                        \
            val.insert(                                                                     \
                QVariant(p.key()).toString(),                                               \
                QJsonValue::fromVariant(QVariant(p.value())));                              \
        }                                                                                   \
        return val;                                                                         \
    }                                                                                       \
    void SET(json, name)(const QJsonValue & varname) {                                      \
        QJsonObject val = varname.toObject();                                               \
        name.clear();                                                                       \
        for(auto p = val.constBegin() ;p != val.constEnd(); ++p) {                          \
            name.insert(                                                                    \
                QVariant(p.key()).value<map::key_type>(),                                   \
                QVariant(p.value()).value<map::mapped_type>());                             \
        }                                                                                   \
    }
#else
#define QS_JSON_QT_DICT(map, name)
#endif

/* Generate XML-property and methods for dictionary of simple fields (int, bool, QString, ...)  */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be QMap, QHash)    */
/* THIS IS FOR QT DICTIONARY TYPES, for example QMap<int, QString>, QMap<int,int>, ...*/
#ifdef QS_HAS_XML
#define QS_XML_QT_DICT(map, name)                                                                       \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                                  \
    private:                                                                                            \
    QDomNode GET(xml, name)() const {                                                                   \
        QDomDocument doc;                                                                               \
        QDomElement element = doc.createElement(#name);                                                 \
        element.setAttribute("type", "map");                                                            \
        for(auto p = name.begin(); p != name.end(); ++p)                                                \
        {                                                                                               \
            QDomElement e = doc.createElement("item");                                                  \
            e.setAttribute("key", QVariant(p.key()).toString());                                        \
            e.setAttribute("value", QVariant(p.value()).toString());                                    \
            element.appendChild(e);                                                                     \
        }                                                                                               \
        doc.appendChild(element);                                                                       \
        return QDomNode(doc);                                                                           \
    }                                                                                                   \
    void SET(xml, name)(const QDomNode & node) {                                                        \
        if(!node.isNull() && node.isElement())                                                          \
        {                                                                                               \
            QDomElement root = node.toElement();                                                        \
            if(root.tagName() == #name)                                                                 \
            {                                                                                           \
                QDomNodeList childs = root.childNodes();                                                \
                                                                                                        \
                for(int i = 0; i < childs.size(); ++i) {                                                \
                QDomElement item = childs.at(i).toElement();                                            \
                name.insert(QVariant(item.attributeNode("key").value()).value<map::key_type>(),         \
                            QVariant(item.attributeNode("value").value()).value<map::mapped_type>());   \
                }                                                                                       \
            }                                                                                           \
        }                                                                                               \
    }
#else
#define QS_XML_QT_DICT(map, name)
#endif


/* Generate JSON-property and methods for dictionary of custom type objects  */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method inserv(KeyT, ValueT) (it's can be QMap, QHash)    */
/* THIS IS FOR QT DICTIONARY TYPES, for example QMap<int, CustomSerializableType> */
#ifdef QS_HAS_JSON
#define QS_JSON_QT_DICT_OBJECTS(map, name)                                                  \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json,name))                   \
    private:                                                                                \
    QJsonValue GET(json, name)() const {                                                    \
        QJsonObject val;                                                                    \
        for(auto p = name.begin(); p != name.end(); ++p) {                                  \
            val.insert(                                                                     \
                QVariant::fromValue(p.key()).toString(),                                    \
                p.value().toJson());                                                        \
        }                                                                                   \
        return val;                                                                         \
    }                                                                                       \
    void SET(json, name)(const QJsonValue & varname) {                                      \
        QJsonObject val = varname.toObject();                                               \
        name.clear();                                                                       \
        for(auto p = val.constBegin();p != val.constEnd(); ++p) {                           \
        map::mapped_type tmp;                                                               \
        tmp.fromJson(p.value());                                                            \
        name.insert(                                                                        \
                QVariant(p.key()).value<map::key_type>(),                                   \
                tmp);                                                                       \
        }                                                                                   \
    }
#else
#define QS_JSON_QT_DICT_OBJECTS(map, name)
#endif

/* Generate XML-property and methods for dictionary of custom type objects  */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be QMap, QHash)    */
/* THIS IS FOR QT DICTIONARY TYPES, for example QMap<int, CustomSerializableType> */
#ifdef QS_HAS_XML
#define QS_XML_QT_DICT_OBJECTS(map, name)                                                               \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                                  \
    private:                                                                                            \
    QDomNode GET(xml, name)() const {                                                                   \
        QDomDocument doc;                                                                               \
        QDomElement element = doc.createElement(#name);                                                 \
        element.setAttribute("type", "map");                                                            \
        for(auto p = name.begin(); p != name.end(); ++p)                                                \
        {                                                                                               \
            QDomElement e = doc.createElement("item");                                                  \
            e.setAttribute("key", QVariant(p.key()).toString());                                        \
            e.appendChild(p.value().toXml());                                                           \
            element.appendChild(e);                                                                     \
        }                                                                                               \
        doc.appendChild(element);                                                                       \
        return QDomNode(doc);                                                                           \
    }                                                                                                   \
    void SET(xml, name)(const QDomNode & node) {                                                        \
        if(!node.isNull() && node.isElement())                                                          \
        {                                                                                               \
            QDomElement root = node.toElement();                                                        \
            if(root.tagName() == #name)                                                                 \
            {                                                                                           \
                QDomNodeList childs = root.childNodes();                                                \
                                                                                                        \
                for(int i = 0; i < childs.size(); ++i) {                                                \
                QDomElement item = childs.at(i).toElement();                                            \
                map::mapped_type tmp;                                                                   \
                tmp.fromXml(item.firstChild());                                                         \
                name.insert(QVariant(item.attributeNode("key").value()).value<map::key_type>(),         \
                            tmp);                                                                       \
                }                                                                                       \
            }                                                                                           \
        }                                                                                               \
    }
#else
#define QS_XML_QT_DICT_OBJECTS(map, name)
#endif

/* Generate JSON-property and methods for dictionary of simple fields (int, bool, QString, ...)  */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be std::map)    */
/* THIS IS FOR STL DICTIONARY TYPES, for example std::map<int, QString>, std::map<int,int>, ...*/
#ifdef QS_HAS_JSON
#define QS_JSON_STL_DICT(map, name)                                                         \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json,name))                   \
    private:                                                                                \
    QJsonValue GET(json, name)() const {                                                    \
        QJsonObject val;                                                                    \
        for(auto p : name){                                                                 \
            val.insert(                                                                     \
                QVariant::fromValue(p.first).toString(),                                    \
                QJsonValue::fromVariant(QVariant(p.second)));                               \
        }                                                                                   \
        return val;                                                                         \
    }                                                                                       \
    void SET(json, name)(const QJsonValue & varname) {                                      \
        QJsonObject val = varname.toObject();                                               \
        name.clear();                                                                       \
        for(auto p = val.constBegin();p != val.constEnd(); ++p) {                           \
            name.insert(std::pair<map::key_type, map::mapped_type>(                         \
                QVariant(p.key()).value<map::key_type>(),                                   \
                QVariant(p.value()).value<map::mapped_type>()));                            \
        }                                                                                   \
    }
#else
#define QS_JSON_STL_DICT(map, name)
#endif

#ifdef QS_HAS_XML
#define QS_XML_STL_DICT(map, name)                                                                       \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                                  \
    private:                                                                                            \
    QDomNode GET(xml, name)() const {                                                                   \
        QDomDocument doc;                                                                               \
        QDomElement element = doc.createElement(#name);                                                 \
        element.setAttribute("type", "map");                                                            \
        for(auto p : name)                                                                              \
        {                                                                                               \
            QDomElement e = doc.createElement("item");                                                  \
            e.setAttribute("key", QVariant(p.first).toString());                                        \
            e.setAttribute("value", QVariant(p.second).toString());                                     \
            element.appendChild(e);                                                                     \
        }                                                                                               \
        doc.appendChild(element);                                                                       \
        return QDomNode(doc);                                                                           \
    }                                                                                                   \
    void SET(xml, name)(const QDomNode & node) {                                                        \
        if(!node.isNull() && node.isElement())                                                          \
        {                                                                                               \
            QDomElement root = node.toElement();                                                        \
            if(root.tagName() == #name)                                                                 \
            {                                                                                           \
                QDomNodeList childs = root.childNodes();                                                \
                                                                                                        \
                for(int i = 0; i < childs.size(); ++i) {                                                \
                QDomElement item = childs.at(i).toElement();                                            \
                name.insert(std::pair<map::key_type, map::mapped_type>(                                 \
                            QVariant(item.attributeNode("key").value()).value<map::key_type>(),         \
                            QVariant(item.attributeNode("value").value()).value<map::mapped_type>()));  \
                }                                                                                       \
            }                                                                                           \
        }                                                                                               \
    }
#else
#define QS_XML_STL_DICT(map, name)
#endif

/* Generate JSON-property and methods for dictionary of custom type objects */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be std::map)    */
/* THIS IS FOR STL DICTIONARY TYPES, for example std::map<int, CustomSerializableType> */
#ifdef QS_HAS_JSON
#define QS_JSON_STL_DICT_OBJECTS(map, name)                                                 \
    Q_PROPERTY(QJsonValue name READ GET(json, name) WRITE SET(json,name))                   \
    private:                                                                                \
    QJsonValue GET(json, name)() const {                                                    \
        QJsonObject val;                                                                    \
        for(auto p : name){                                                                 \
            val.insert(                                                                     \
                QVariant::fromValue(p.first).toString(),                                    \
                p.second.toJson());                                                         \
        }                                                                                   \
        return val;                                                                         \
    }                                                                                       \
    void SET(json, name)(const QJsonValue & varname) {                                      \
        QJsonObject val = varname.toObject();                                               \
        name.clear();                                                                       \
        for(auto p = val.constBegin(); p != val.constEnd(); ++p) {                          \
            map::mapped_type tmp;                                                           \
            tmp.fromJson(p.value());                                                        \
            name.insert(std::pair<map::key_type, map::mapped_type>(                         \
                QVariant(p.key()).value<map::key_type>(),                                   \
                tmp));                                                                      \
        }                                                                                   \
    }
#else
#define QS_JSON_STL_DICT_OBJECTS(map, name)
#endif

/* Generate XML-property and methods for dictionary of custom type objects */
/* Custom type must be inherit from QSerializer */
/* This collection must be provide method insert(KeyT, ValueT) (it's can be std::map)    */
/* THIS IS FOR STL DICTIONARY TYPES, for example std::map<int, CustomSerializableType> */
#ifdef QS_HAS_XML
#define QS_XML_STL_DICT_OBJECTS(map, name)                                                              \
    Q_PROPERTY(QDomNode name READ GET(xml, name) WRITE SET(xml, name))                                  \
    private:                                                                                            \
    QDomNode GET(xml, name)() const {                                                                   \
        QDomDocument doc;                                                                               \
        QDomElement element = doc.createElement(#name);                                                 \
        element.setAttribute("type", "map");                                                            \
        for(auto p : name)                                                \
        {                                                                                               \
            QDomElement e = doc.createElement("item");                                                  \
            e.setAttribute("key", QVariant(p.first).toString());                                        \
            e.appendChild(p.second.toXml());                                                            \
            element.appendChild(e);                                                                     \
        }                                                                                               \
        doc.appendChild(element);                                                                       \
        return QDomNode(doc);                                                                           \
    }                                                                                                   \
    void SET(xml, name)(const QDomNode & node) {                                                        \
        if(!node.isNull() && node.isElement())                                                          \
        {                                                                                               \
            QDomElement root = node.toElement();                                                        \
            if(root.tagName() == #name)                                                                 \
            {                                                                                           \
                QDomNodeList childs = root.childNodes();                                                \
                                                                                                        \
                for(int i = 0; i < childs.size(); ++i) {                                                \
                QDomElement item = childs.at(i).toElement();                                            \
                map::mapped_type tmp;                                                                   \
                tmp.fromXml(item.firstChild());                                                         \
                name.insert(std::pair<map::key_type, map::mapped_type> (                                \
                            QVariant(item.attributeNode("key").value()).value<map::key_type>(),         \
                            tmp));                                                                      \
                }                                                                                       \
            }                                                                                           \
        }                                                                                               \
    }
#else
#define QS_XML_STL_DICT_OBJECTS(map, name)
#endif


/* BIND: */
/* generate serializable propertyes JSON and XML for primitive type field */
#define QS_BIND_FIELD(type, name)                                                           \
    QS_JSON_FIELD(type, name)                                                               \
    QS_XML_FIELD(type, name)                                                                \

/* BIND: */
/* generate serializable propertyes JSON and XML for collection of primitive type fields */
#define QS_BIND_COLLECTION(itemType, name)                                                  \
    QS_JSON_ARRAY(itemType, name)                                                           \
    QS_XML_ARRAY(itemType, name)                                                            \

/* BIND: */
/* generate serializable propertyes JSON and XML for custom type object */
#define QS_BIND_OBJECT(type, name)                                                          \
    QS_JSON_OBJECT(type, name)                                                              \
    QS_XML_OBJECT(type, name)                                                               \

/* BIND: */
/* generate serializable propertyes JSON and XML for collection of custom type objects */
#define QS_BIND_COLLECTION_OBJECTS(itemType, name)                                          \
    QS_JSON_ARRAY_OBJECTS(itemType, name)                                                   \
    QS_XML_ARRAY_OBJECTS(itemType, name)                                                    \

/* BIND: */
/* generate serializable propertyes JSON and XML for dictionary with primitive value type for QT DICTIONARY TYPES */
#define QS_BIND_QT_DICT(map, name)                                                         \
    QS_JSON_QT_DICT(map, name)                                                             \
    QS_XML_QT_DICT(map, name)                                                              \

/* BIND: */
/* generate serializable propertyes JSON and XML for dictionary of custom type objects for QT DICTIONARY TYPES */
#define QS_BIND_QT_DICT_OBJECTS(map, name)                                                 \
    QS_JSON_QT_DICT_OBJECTS(map, name)                                                     \
    QS_XML_QT_DICT_OBJECTS(map,name)                                                       \


/* BIND: */
/* generate serializable propertyes JSON and XML for dictionary with primitive value type for STL DICTIONARY TYPES */
#define QS_BIND_STL_DICT(map, name)                                                         \
    QS_JSON_STL_DICT(map, name)                                                             \
    QS_XML_STL_DICT(map, name)                                                              \


/* BIND: */
/* generate serializable propertyes JSON and XML for dictionary of custom type objects for STL DICTIONARY TYPES */
#define QS_BIND_STL_DICT_OBJECTS(map, name)                                                 \
    QS_JSON_STL_DICT_OBJECTS(map, name)                                                     \
    QS_XML_STL_DICT_OBJECTS(map,name)                                                       \



/* CREATE AND BIND: */
/* Make primitive field and generate serializable propertyes */
/* For example: QS_FIELD(int, digit), QS_FIELD(bool, flag) */
#define QS_FIELD(type, name)                                                                \
    QS_DECLARE_MEMBER(type, name)                                                           \
    QS_BIND_FIELD(type, name)                                                               \

/* CREATE AND BIND: */
/* Make collection of primitive type objects [collectionType<itemType> name] and generate serializable propertyes for this collection */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#define QS_COLLECTION(collectionType, itemType, name)                                       \
    QS_DECLARE_MEMBER(collectionType<itemType>, name)                                       \
    QS_BIND_COLLECTION(itemType, name)                                                      \

/* CREATE AND BIND: */
/* Make custom class object and bind serializable propertyes */
/* This class must be inherited from QSerializer */
#define QS_OBJECT(type,name)                                                                \
    QS_DECLARE_MEMBER(type, name)                                                           \
    QS_BIND_OBJECT(type, name)                                                              \

/* CREATE AND BIND: */
/* Make collection of custom class objects [collectionType<itemType> name] and bind serializable propertyes */
/* This collection must be provide method append(T) (it's can be QList, QVector)    */
#define QS_COLLECTION_OBJECTS(collectionType, itemType, name)                               \
    QS_DECLARE_MEMBER(collectionType<itemType>, name)                                       \
    QS_BIND_COLLECTION_OBJECTS(itemType, name)                                              \


/* CREATE AND BIND: */
/* Make dictionary collection of simple types [dictionary<key, itemType> name] and bind serializable propertyes */
/* This collection must be QT DICTIONARY TYPE */
#define QS_QT_DICT(map, first, second, name)                                                \
    public:                                                                                 \
    typedef map<first,second> dict_##name##_t;                                              \
    dict_##name##_t name = dict_##name##_t();                                               \
    QS_BIND_QT_DICT(dict_##name##_t, name)                                                  \

/* CREATE AND BIND: */
/* Make dictionary collection of custom class objects [dictionary<key, itemType> name] and bind serializable propertyes */
/* This collection must be QT DICTIONARY TYPE */
#define QS_QT_DICT_OBJECTS(map, first, second, name)                                        \
    public:                                                                                 \
    typedef map<first,second> dict_##name##_t;                                              \
    dict_##name##_t name = dict_##name##_t();                                               \
    QS_BIND_QT_DICT_OBJECTS(dict_##name##_t, name)                                          \

/* CREATE AND BIND: */
/* Make dictionary collection of simple types [dictionary<key, itemType> name] and bind serializable propertyes */
/* This collection must be STL DICTIONARY TYPE */
#define QS_STL_DICT(map, first, second, name)                                               \
    public:                                                                                 \
    typedef map<first,second> dict_##name##_t;                                              \
    dict_##name##_t name = dict_##name##_t();                                               \
    QS_BIND_STL_DICT(dict_##name##_t, name)                                                 \

/* CREATE AND BIND: */
/* Make dictionary collection of custom class objects [dictionary<key, itemType> name] and bind serializable propertyes */
/* This collection must be STL DICTIONARY TYPE */
#define QS_STL_DICT_OBJECTS(map, first, second, name)                                       \
    public:                                                                                 \
    typedef map<first,second> dict_##name##_t;                                              \
    dict_##name##_t name = dict_##name##_t();                                               \
    QS_BIND_STL_DICT_OBJECTS(dict_##name##_t, name)                                         \



#endif // QSERIALIZER_H
