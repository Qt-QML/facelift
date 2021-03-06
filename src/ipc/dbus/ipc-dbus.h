/*
 *   Copyright (C) 2017 Pelagicore AG
 *   SPDX-License-Identifier: LGPL-2.1
 *   This file is subject to the terms of the LGPL 2.1 license.
 *   Please see the LICENSE file for details.
 */

#pragma once

#include <QDebug>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDBusAbstractInterface>
#include <QDBusInterface>
#include <QDBusServiceWatcher>

#include "FaceliftModel.h"
#include "utils.h"
#include "Property.h"

#include "ipc-common/ipc-common.h"

#include "ipc-dbus-serialization.h"

namespace facelift {
namespace dbus {

using namespace facelift;


class DBusIPCMessage
{

public:
    DBusIPCMessage() :
        DBusIPCMessage("dummy/dummy", "dummy.dummy", "gg")
    {
    }

    DBusIPCMessage(const QDBusMessage &msg)
    {
        m_message = msg;
    }

    DBusIPCMessage(const QString &service, const QString &path, const QString &interface, const QString &method)
    {
        m_message = QDBusMessage::createMethodCall(service, path, interface, method);
    }

    DBusIPCMessage(const QString &path, const QString &interface, const QString &signal)
    {
        m_message = QDBusMessage::createSignal(path, interface, signal);
    }

    DBusIPCMessage call(const QDBusConnection &connection)
    {
        qDebug() << "Sending IPC message : " << toString();
        auto replyDbusMessage = connection.call(m_message);
        DBusIPCMessage reply(replyDbusMessage);
        return reply;
    }

    void send(const QDBusConnection &connection)
    {
        qDebug() << "Sending IPC message : " << toString();
        bool successful = connection.send(m_message);
        Q_ASSERT(successful);
    }

    QString member() const
    {
        return m_message.member();
    }

    QString toString() const;

    DBusIPCMessage createReply()
    {
        return DBusIPCMessage(m_message.createReply());
    }

    DBusIPCMessage createErrorReply(const QString &msg, const QString &member)
    {
        return DBusIPCMessage(m_message.createErrorReply(msg, member));
    }

    template<typename Type>
    void readNextParameter(Type &v)
    {
        auto asVariant = m_message.arguments()[m_readPos++];
        //        qDebug() << asVariant;
        v = asVariant.value<Type>();
    }

    template<typename Type>
    void writeSimple(const Type &v)
    {
        //        qDebug() << "Writing to message : " << v;
        msg() << v;
    }

    QString signature() const
    {
        return m_message.signature();
    }

    bool isReplyMessage() const
    {
        return (m_message.type() == QDBusMessage::ReplyMessage);
    }

    bool isErrorMessage() const
    {
        return (m_message.type() == QDBusMessage::ErrorMessage);
    }

private:
    QDBusMessage &msg()
    {
        return m_message;
    }

    QDBusMessage m_message;

    size_t m_readPos = 0;
};



template<typename Type, typename Enable = void>
struct IPCTypeHandler
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "i";
    }

    static void write(DBusIPCMessage &msg, const Type &v)
    {
        msg.writeSimple(v);
    }

    static void read(DBusIPCMessage &msg, Type &v)
    {
        msg.readNextParameter(v);
    }

};


template<size_t I = 0, typename ... Ts>
typename std::enable_if<I == sizeof ... (Ts)>::type
appendDBUSTypeSignature(QTextStream &s, std::tuple<Ts ...> &t)
{
    Q_UNUSED(s);
    Q_UNUSED(t);
}

template<size_t I = 0, typename ... Ts>
typename std::enable_if < I<sizeof ... (Ts)>::type
appendDBUSTypeSignature(QTextStream &s, std::tuple<Ts ...> &t)
{
    typedef typeof (std::get<I>(t)) Type;
    IPCTypeHandler<Type>::writeDBUSSignature(s);
    appendDBUSTypeSignature<I + 1>(s, t);
}


template<size_t I = 0, typename ... Ts>
typename std::enable_if<I == sizeof ... (Ts)>::type
appendDBUSMethodArgumentsSignature(QTextStream &s, std::tuple<Ts ...> &t, const std::array<const char *,
        sizeof ... (Ts)> &argNames)
{
    Q_UNUSED(s);
    Q_UNUSED(t);
    Q_UNUSED(argNames);
}

template<size_t I = 0, typename ... Ts>
typename std::enable_if < I<sizeof ... (Ts)>::type
appendDBUSMethodArgumentsSignature(QTextStream &s, std::tuple<Ts ...> &t, const std::array<const char *,
        sizeof ... (Ts)> &argNames)
{
    typedef typeof (std::get<I>(t)) Type;
    s << "<arg name=\"" << argNames[I] << "\" type=\"";
    IPCTypeHandler<Type>::writeDBUSSignature(s);
    s << "\" direction=\"in\"/>";
    appendDBUSMethodArgumentsSignature<I + 1>(s, t, argNames);
}


template<size_t I = 0, typename ... Ts>
typename std::enable_if<I == sizeof ... (Ts)>::type
appendDBUSSignalArgumentsSignature(QTextStream &s, std::tuple<Ts ...> &t, const std::array<const char *,
        sizeof ... (Ts)> &argNames)
{
    Q_UNUSED(s);
    Q_UNUSED(t);
    Q_UNUSED(argNames);
}

template<size_t I = 0, typename ... Ts>
typename std::enable_if < I<sizeof ... (Ts)>::type
appendDBUSSignalArgumentsSignature(QTextStream &s, std::tuple<Ts ...> &t, const std::array<const char *,
        sizeof ... (Ts)> &argNames)
{
    typedef typeof (std::get<I>(t)) Type;
    s << "<arg name=\"" << argNames[I] << "\" type=\"";
    IPCTypeHandler<Type>::writeDBUSSignature(s);
    s << "\"/>";
    appendDBUSSignalArgumentsSignature<I + 1>(s, t, argNames);
}


struct AppendDBUSSignatureFunction
{
    AppendDBUSSignatureFunction(QTextStream &s) :
        s(s)
    {
    }

    QTextStream &s;

    template<typename T>
    void operator()(T &&t)
    {
        Q_UNUSED(t);
        typedef typename std::decay<T>::type TupleType;
        std::tuple<TupleType> dummyTuple;
        appendDBUSTypeSignature(s, dummyTuple);
    }
};

template<>
struct IPCTypeHandler<float>
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "d";
    }

    static void write(DBusIPCMessage &msg, const float &v)
    {
        msg.writeSimple((double)v);
    }

    static void read(DBusIPCMessage &msg, float &v)
    {
        double d;
        msg.readNextParameter(d);
        v = d;
    }

};


template<>
struct IPCTypeHandler<bool>
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "b";
    }

    static void write(DBusIPCMessage &msg, const bool &v)
    {
        msg.writeSimple(v);
    }

    static void read(DBusIPCMessage &msg, bool &v)
    {
        msg.readNextParameter(v);
    }

};


template<>
struct IPCTypeHandler<QString>
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "s";
    }

    static void write(DBusIPCMessage &msg, const QString &v)
    {
        msg.writeSimple(v);
    }

    static void read(DBusIPCMessage &msg, QString &v)
    {
        msg.readNextParameter(v);
    }

};

template<typename Type>
struct IPCTypeHandler<Type, typename std::enable_if<std::is_base_of<StructureBase, Type>::value>::type>
{

    static void writeDBUSSignature(QTextStream &s)
    {
        typename Type::FieldTupleTypes t;          // TODO : get rid of that tuple
        s << "(";
        for_each_in_tuple(t, AppendDBUSSignatureFunction(s));
        s << ")";
    }

    static void write(DBusIPCMessage &msg, const Type &param)
    {
        for_each_in_tuple_const(param.asTuple(), StreamWriteFunction<DBusIPCMessage>(msg));
        param.id();
        msg << param.id();
    }

    static void read(DBusIPCMessage &msg, Type &param)
    {
        typename Type::FieldTupleTypes tuple;
        for_each_in_tuple(tuple, StreamReadFunction<DBusIPCMessage>(msg));
        param.setValue(tuple);
        ModelElementID id;
        msg.readNextParameter(id);
        param.setId(id);
    }

};

template<typename Type>
struct IPCTypeHandler<Type, typename std::enable_if<std::is_enum<Type>::value>::type>
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "i";
    }

    static void write(DBusIPCMessage &msg, const Type &param)
    {
        msg.writeSimple(static_cast<int>(param));
    }

    static void read(DBusIPCMessage &msg, Type &param)
    {
        int i;
        msg.readNextParameter(i);
        param = static_cast<Type>(i);
    }
};


template<typename ElementType>
struct IPCTypeHandler<QList<ElementType> >
{
    static void writeDBUSSignature(QTextStream &s)
    {
        s << "a";
        IPCTypeHandler<ElementType>::writeDBUSSignature(s);
    }

    static void write(DBusIPCMessage &msg, const QList<ElementType> &list)
    {
        int count = list.size();
        msg.writeSimple(count);
        for (const auto &e : list) {
            IPCTypeHandler<ElementType>::write(msg, e);
        }
    }

    static void read(DBusIPCMessage &msg, QList<ElementType> &list)
    {
        list.clear();
        int count;
        msg.readNextParameter(count);
        for (int i = 0; i < count; i++) {
            ElementType e;
            IPCTypeHandler<ElementType>::read(msg, e);
            list.append(e);
        }
    }

};


template<typename Type>
DBusIPCMessage &operator<<(DBusIPCMessage &msg, const Type &v)
{
    IPCTypeHandler<Type>::write(msg, v);
    return msg;
}


template<typename Type>
DBusIPCMessage &operator>>(DBusIPCMessage &msg, Type &v)
{
    IPCTypeHandler<Type>::read(msg, v);
    return msg;
}


template<typename Type>
DBusIPCMessage &operator>>(DBusIPCMessage &msg, Property<Type> &property)
{
    Type v;
    IPCTypeHandler<Type>::read(msg, v);
    property.setValue(v);
    return msg;
}


class DBusManager
{

public:
    DBusManager();

    static DBusManager &instance();

    bool isDBusConnected() const
    {
        return m_dbusConnected;
    }

    void registerServiceName(const QString &serviceName)
    {
        qDebug() << "Registering serviceName " << serviceName;
        auto success = m_busConnection.registerService(serviceName);
        Q_ASSERT(success);
    }

    QDBusConnection &connection()
    {
        return m_busConnection;
    }

private:
    QDBusConnection m_busConnection;
    bool m_dbusConnected;
};


class DBusIPCServiceAdapterBase : public IPCServiceAdapterBase
{
    Q_OBJECT

public:
    static constexpr const char *DEFAULT_SERVICE_NAME = "facelift.ipc";

    static constexpr const char *GET_PROPERTIES_MESSAGE_NAME = "GetAllProperties";
    static constexpr const char *PROPERTIES_CHANGED_SIGNAL_NAME = "PropertiesChanged";
    static constexpr const char *SIGNAL_TRIGGERED_SIGNAL_NAME = "SignalTriggered";
    static constexpr const char *SET_PROPERTY_MESSAGE_NAME = "SetProperty";
    static constexpr const char *INTROSPECTABLE_INTERFACE_NAME = "org.freedesktop.DBus.Introspectable";
    static constexpr const char *PROPERTIES_INTERFACE_NAME = "org.freedesktop.DBus.Properties";

    class DBusVirtualObject : public QDBusVirtualObject
    {

public:
        DBusVirtualObject(DBusIPCServiceAdapterBase &adapter) : QDBusVirtualObject(nullptr), m_adapter(adapter)
        {
        }

        QString introspect(const QString &path) const
        {
            return m_adapter.introspect(path);
        }

        bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
        {
            return m_adapter.handleMessage(message, connection);
        }

private:
        DBusIPCServiceAdapterBase &m_adapter;
    };

    DBusIPCServiceAdapterBase(QObject *parent = nullptr) : IPCServiceAdapterBase(parent), m_dbusVirtualObject(*this)
    {
    }

    ~DBusIPCServiceAdapterBase()
    {
        destroyed(this);
    }

    virtual QString introspect(const QString &path) const = 0;

    bool handleMessage(const QDBusMessage &dbusMsg, const QDBusConnection &connection);

    void onPropertyValueChanged()
    {
        DBusIPCMessage msg(objectPath(), interfaceName(), PROPERTIES_CHANGED_SIGNAL_NAME);
        serializePropertyValues(msg);
        msg.send(dbusManager().connection());
    }

    template<typename ... Args>
    void sendSignal(const char *signalName, const Args & ... args)
    {
        DBusIPCMessage msg(objectPath(), interfaceName(), SIGNAL_TRIGGERED_SIGNAL_NAME);
        msg << signalName;
        auto argTuple = std::make_tuple(args ...);
        for_each_in_tuple(argTuple, StreamWriteFunction<DBusIPCMessage>(msg));
        msg.send(dbusManager().connection());
    }

    template<typename Type>
    void addPropertySignature(QTextStream &s, const char *propertyName, bool isReadonly) const
    {
        s << "<property name=\"" << propertyName << "\" type=\"";
        std::tuple<Type> dummyTuple;
        appendDBUSTypeSignature(s, dummyTuple);
        s << "\" access=\"" << (isReadonly ? "read" : "readwrite") << "\"/>";
    }

    template<typename ... Args>
    void addMethodSignature(QTextStream &s, const char *methodName,
            const std::array<const char *, sizeof ... (Args)> &argNames) const
    {
        s << "<method name=\"" << methodName << "\">";
        std::tuple<Args ...> t;  // TODO : get rid of the tuple
        appendDBUSMethodArgumentsSignature(s, t, argNames);
        s << "</method>";
    }

    template<typename ... Args>
    void addSignalSignature(QTextStream &s, const char *methodName,
            const std::array<const char *, sizeof ... (Args)> &argNames) const
    {
        s << "<signal name=\"" << methodName << "\">";
        std::tuple<Args ...> t;  // TODO : get rid of the tuple
        appendDBUSSignalArgumentsSignature(s, t, argNames);
        s << "</signal>";
    }

    virtual IPCHandlingResult handleMethodCallMessage(DBusIPCMessage &requestMessage, DBusIPCMessage &replyMessage) = 0;

    void serializePropertyValues(DBusIPCMessage &replyMessage)
    {
        replyMessage << m_service->ready();
        serializeSpecificPropertyValues(replyMessage);
    }

    virtual void serializeSpecificPropertyValues(DBusIPCMessage &replyMessage) = 0;

    void init(InterfaceBase *service);

    DBusManager &dbusManager()
    {
        return DBusManager::instance();
    }

protected:
    DBusVirtualObject m_dbusVirtualObject;

    QString m_introspectionData;
    QString m_serviceName = DEFAULT_SERVICE_NAME;

    InterfaceBase *m_service = nullptr;

    bool m_alreadyInitialized = false;
};



template<typename ServiceType>
class DBusIPCServiceAdapter : public DBusIPCServiceAdapterBase
{
public:
    typedef ServiceType TheServiceType;

    DBusIPCServiceAdapter(QObject *parent) : DBusIPCServiceAdapterBase(parent)
    {
        setInterfaceName(ServiceType::FULLY_QUALIFIED_INTERFACE_NAME);
    }

    ServiceType *service() const override
    {
        return m_service;
    }

    void setService(InterfaceBase *service) override
    {
        m_service = toProvider<ServiceType>(service);
    }

    virtual void appendDBUSIntrospectionData(QTextStream &s) const = 0;

    void init() override final
    {
        DBusIPCServiceAdapterBase::init(m_service);
    }

    QString introspect(const QString &path) const override
    {
        QString introspectionData;

        if (path == objectPath()) {
            QTextStream s(&introspectionData);
            s << "<interface name=\"" << interfaceName() << "\">";
            appendDBUSIntrospectionData(s);
            s << "</interface>";
        } else {
            qFatal("Wrong object path");
        }

        qDebug() << "Introspection data for " << path << ":" << introspectionData;
        return introspectionData;
    }

protected:
    QPointer<ServiceType> m_service;

};

class IPCRequestHandler
{

public:
    virtual ~IPCRequestHandler()
    {
    }

    virtual void deserializePropertyValues(DBusIPCMessage &msg) = 0;
    virtual void deserializeSignal(DBusIPCMessage &msg) = 0;
    virtual void setServiceRegistered(bool isRegistered) = 0;

};

class DBusIPCProxyBinder : public IPCProxyBinderBase
{
    Q_OBJECT

public:
    Q_PROPERTY(QString serviceName READ serviceName WRITE setServiceName)
    Q_PROPERTY(QString interfaceName READ interfaceName WRITE setInterfaceName)

    DBusIPCProxyBinder(QObject *parent = nullptr) : IPCProxyBinderBase(parent)
    {
        m_busWatcher.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
    }

    const QString &serviceName() const
    {
        return m_serviceName;
    }

    void setServiceName(const QString &name)
    {
        m_serviceName = name;
        checkInit();
    }

    const QString &interfaceName() const
    {
        return m_interfaceName;
    }

    void setInterfaceName(const QString &name)
    {
        m_interfaceName = name;
        checkInit();
    }

    Q_SLOT
    void onPropertiesChanged(const QDBusMessage &dbusMessage)
    {
        DBusIPCMessage msg(dbusMessage);
        m_serviceObject->deserializePropertyValues(msg);
    }

    Q_SLOT
    void onSignalTriggered(const QDBusMessage &dbusMessage)
    {
        DBusIPCMessage msg(dbusMessage);
        m_serviceObject->deserializeSignal(msg);
    }

    void bindToIPC();

    void onServiceAvailable()
    {
        requestPropertyValues();
    }

    void requestPropertyValues()
    {
        DBusIPCMessage msg(serviceName(), objectPath(), interfaceName(),
                DBusIPCServiceAdapterBase::GET_PROPERTIES_MESSAGE_NAME);
        auto replyMessage = msg.call(connection());
        if (replyMessage.isReplyMessage()) {
            m_serviceObject->deserializePropertyValues(replyMessage);
            m_serviceObject->setServiceRegistered(true);
        } else {
            qDebug() << "Service not yet available : " << objectPath();
        }
    }

    template<typename PropertyType>
    void sendSetterCall(const char *methodName, const PropertyType &value)
    {
        DBusIPCMessage msg(m_serviceName, objectPath(), m_interfaceName, methodName);
        msg << value;
        auto replyMessage = msg.call(connection());
        if (replyMessage.isErrorMessage()) {
            qCritical(
                "Error message received when calling method '%s' on service at path '%s'. This likely indicates that the server you are trying to access is not available yet",
                qPrintable(methodName), qPrintable(objectPath()));
        }
    }

    template<typename ... Args>
    DBusIPCMessage sendMethodCall(const char *methodName, const Args & ... args)
    {
        DBusIPCMessage msg(m_serviceName, objectPath(), m_interfaceName, methodName);
        auto argTuple = std::make_tuple(args ...);
        for_each_in_tuple(argTuple, StreamWriteFunction<DBusIPCMessage>(msg));
        auto replyMessage = msg.call(connection());
        if (replyMessage.isErrorMessage()) {
            qCritical(
                "Error message received when calling method '%s' on service at path '%s'. This likely indicates that the server you are trying to access is not available yet",
                qPrintable(methodName), qPrintable(objectPath()));
        }
        return replyMessage;
    }

    QDBusConnection &connection()
    {
        return manager().connection();
    }

    DBusManager &manager()
    {
        return DBusManager::instance();
    }

    void setHandler(IPCRequestHandler *handler)
    {
        m_serviceObject = handler;
        checkInit();
    }

private:
    bool m_alreadyInitialized = false;

    QString m_serviceName = DBusIPCServiceAdapterBase::DEFAULT_SERVICE_NAME;
    QString m_interfaceName;

    IPCRequestHandler *m_serviceObject = nullptr;

    QDBusServiceWatcher m_busWatcher;
};



template<typename AdapterType, typename IPCAdapterType>
class DBusIPCProxy : public IPCProxyBase<AdapterType, IPCAdapterType>, protected IPCRequestHandler
{

public:
    DBusIPCProxy(QObject *parent = nullptr) :
        IPCProxyBase<AdapterType, IPCAdapterType>(parent)
    {
        m_ipcBinder.setInterfaceName(AdapterType::FULLY_QUALIFIED_INTERFACE_NAME);
        m_ipcBinder.setHandler(this);

        this->initBinder(m_ipcBinder);
        this->setImplementationID("DBus IPC Proxy");
    }

    virtual void deserializeSpecificPropertyValues(DBusIPCMessage &msg) = 0;

    void deserializePropertyValues(DBusIPCMessage &msg) override
    {
        auto r = this->ready();
        msg >> r;
        this->setReady(r);
        deserializeSpecificPropertyValues(msg);
    }

    void setServiceRegistered(bool isRegistered) override
    {
        bool oldReady = this->ready();
        m_serviceRegistered = isRegistered;
        if (this->ready() != oldReady) {
            this->readyChanged();
        }
    }

    template<typename PropertyType>
    void sendSetterCall(const char *methodName, const PropertyType &value)
    {
        m_ipcBinder.sendSetterCall(methodName, value);
    }

    template<typename ... Args>
    void sendMethodCall(const char *methodName, const Args & ... args)
    {
        auto msg = m_ipcBinder.sendMethodCall(methodName, args ...);
    }

    template<typename ReturnType, typename ... Args>
    void sendMethodCallWithReturn(const char *methodName, ReturnType &returnValue, const Args & ... args)
    {
        auto msg = m_ipcBinder.sendMethodCall(methodName, args ...);
        if (msg.isReplyMessage()) {
            IPCTypeHandler<ReturnType>::read(msg, returnValue);
        }
    }

    DBusIPCProxyBinder *ipc()
    {
        return &m_ipcBinder;
    }

    void connectToServer()
    {
        m_ipcBinder.connectToServer();
    }

private:
    DBusIPCProxyBinder m_ipcBinder;
    bool m_serviceRegistered = false;

};


class DBusIPCAdapterFactoryManager
{
public:
    typedef DBusIPCServiceAdapterBase * (*IPCAdapterFactory)(InterfaceBase *);

    static DBusIPCAdapterFactoryManager &instance();

    template<typename AdapterType>
    static DBusIPCServiceAdapterBase *createInstance(InterfaceBase *i)
    {
        auto adapter = new AdapterType(i);
        adapter->setService(i);
        qDebug() << "Created adapter for interface " << i->interfaceID();
        return adapter;
    }

    template<typename AdapterType>
    static void registerType()
    {
        auto &i = instance();
        const auto &typeID = AdapterType::TheServiceType::FULLY_QUALIFIED_INTERFACE_NAME;
        if (i.m_factories.contains(typeID)) {
            qWarning() << "IPC type already registered" << typeID;
        } else {
            i.m_factories.insert(typeID, &DBusIPCAdapterFactoryManager::createInstance<AdapterType>);
        }
    }

    IPCAdapterFactory getFactory(const QString &typeID) const
    {
        if (m_factories.contains(typeID)) {
            return m_factories[typeID];
        } else {
            return nullptr;
        }
    }

private:
    QMap<QString, IPCAdapterFactory> m_factories;

};

class IPCAdapterAttachedType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString objectPath READ objectPath WRITE setObjectPath NOTIFY objectPathChanged)

public:
    IPCAdapterAttachedType(QObject *parent) : QObject(parent)
    {
    }

    Q_SIGNAL void objectPathChanged();

    const QString &objectPath() const
    {
        return m_objectPath;
    }

    void setObjectPath(const QString &objectPath)
    {
        m_objectPath = objectPath;
    }

private:
    QString m_objectPath;

};


class DBusIPCAttachedPropertyFactory : public IPCAttachedPropertyFactoryBase
{

public:
    static DBusIPCServiceAdapterBase *qmlAttachedProperties(QObject *object);

};

}
}

QML_DECLARE_TYPEINFO(facelift::dbus::DBusIPCAttachedPropertyFactory, QML_HAS_ATTACHED_PROPERTIES)
