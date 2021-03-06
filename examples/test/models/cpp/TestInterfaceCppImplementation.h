#pragma once

#include "facelift/test/TestInterfacePropertyAdapter.h"


using namespace facelift::test;

/**
 * C++ Implementation of the TestInterface API
 */
class TestInterfaceCppImplementation : public TestInterfacePropertyAdapter
{

public:
    TestInterfaceCppImplementation(QObject *parent = nullptr) : TestInterfacePropertyAdapter(parent)
    {
        qDebug() << "C++ implementation of TestInterface is used";
        QTimer::singleShot(1000, this, [this] () {
            emit eventWithList(QList<int>({3, 4, 5}), true);
            QList<TestStruct> structs;
            TestStruct s;
            s.setaString("Struct1");
            structs.append(s);
            s.setaString("Struct2");
            structs.append(s);

            TestStructWithList arg;
            arg.setlistOfStructs(structs);
            arg.setlistOfInts(QList<int>({2, 5, 8}));
            eventWithStructWithList(arg);
        });
    }

    void setintProperty(const int &newValue) override
    {
        qDebug() << "new intProperty:" << newValue;
    };
    void setwritableEnumProperty(const TestEnum & /*newValue*/) override
    {
    };
    void setstructProperty(const TestStruct & /*newValue*/) override
    {
    };
    void setstructProperty2(const TestStruct2 & /*newValue*/) override
    {
    };
    void setstringListProperty(const QList<QString> & /*newValue*/) override
    {
    };
    QString method1() override
    {
        return QString();
    };
    TestStruct2 method2(int /*intParam*/, bool /*boolParam*/) override
    {
        return TestStruct2();
    };
    TestEnum method3() override
    {
        return TestEnum();
    };
    QList<TestEnum> method4(TestStruct2 /*s*/)  override
    {
        return QList<TestEnum>();
    };
    QList<TestStruct> method5() override
    {
        return QList<TestStruct>();
    };
};
