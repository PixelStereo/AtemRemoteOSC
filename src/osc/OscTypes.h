#pragma once

#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QVector>
#include <cstdint>

namespace Osc {

enum class Type : char {
    Int32   = 'i',
    Float32 = 'f',
    String  = 's',
    Blob    = 'b',
    Int64   = 'h',
    Double  = 'd',
    True    = 'T',
    False   = 'F',
    Nil     = 'N',
    Bang    = 'I',
    Unknown = '?'
};

class Value {
public:
    Value() : m_type(Type::Nil) {}
    explicit Value(int32_t val) : m_type(Type::Int32), m_int32(val) {}
    explicit Value(float val) : m_type(Type::Float32), m_float32(val) {}
    explicit Value(const QString &val) : m_type(Type::String), m_string(val) {}
    explicit Value(const char *val) : m_type(Type::String), m_string(QString::fromUtf8(val)) {}
    explicit Value(const QByteArray &val) : m_type(Type::Blob), m_blob(val) {}
    explicit Value(int64_t val) : m_type(Type::Int64), m_int64(val) {}
    explicit Value(double val) : m_type(Type::Double), m_double(val) {}
    explicit Value(bool val) : m_type(val ? Type::True : Type::False) {}

    static Value makeBang() {
        Value v;
        v.m_type = Type::Bang;
        return v;
    }

    Type type() const { return m_type; }
    bool isInt() const { return m_type == Type::Int32; }
    bool isFloat() const { return m_type == Type::Float32; }
    bool isString() const { return m_type == Type::String; }
    bool isBlob() const { return m_type == Type::Blob; }
    bool isBool() const { return m_type == Type::True || m_type == Type::False; }
    bool isNil() const { return m_type == Type::Nil; }

    int32_t toInt(int32_t defaultVal = 0) const {
        switch (m_type) {
            case Type::Int32:   return m_int32;
            case Type::Float32: return static_cast<int32_t>(m_float32);
            case Type::Int64:   return static_cast<int32_t>(m_int64);
            case Type::Double:  return static_cast<int32_t>(m_double);
            case Type::True:    return 1;
            case Type::False:   return 0;
            case Type::String:  return m_string.toInt();
            default:            return defaultVal;
        }
    }

    float toFloat(float defaultVal = 0.0f) const {
        switch (m_type) {
            case Type::Float32: return m_float32;
            case Type::Double:  return static_cast<float>(m_double);
            case Type::Int32:   return static_cast<float>(m_int32);
            case Type::Int64:   return static_cast<float>(m_int64);
            case Type::True:    return 1.0f;
            case Type::False:   return 0.0f;
            case Type::String:  return m_string.toFloat();
            default:            return defaultVal;
        }
    }

    QString toString() const {
        switch (m_type) {
            case Type::String:  return m_string;
            case Type::Int32:   return QString::number(m_int32);
            case Type::Float32: return QString::number(m_float32);
            case Type::Int64:   return QString::number(m_int64);
            case Type::Double:  return QString::number(m_double);
            case Type::True:    return QStringLiteral("true");
            case Type::False:   return QStringLiteral("false");
            case Type::Nil:     return QStringLiteral("nil");
            case Type::Bang:    return QStringLiteral("bang");
            default:            return QString();
        }
    }

    bool toBool() const {
        switch (m_type) {
            case Type::True:    return true;
            case Type::False:   return false;
            case Type::Int32:   return m_int32 != 0;
            case Type::Float32: return m_float32 != 0.0f;
            case Type::String:  return m_string.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || m_string == QLatin1String("1");
            default:            return false;
        }
    }

    QByteArray toBlob() const { return m_blob; }

private:
    Type m_type;
    int32_t m_int32 = 0;
    float m_float32 = 0.0f;
    int64_t m_int64 = 0;
    double m_double = 0.0;
    QString m_string;
    QByteArray m_blob;
};

class Message {
public:
    Message() = default;
    explicit Message(const QString &address) : m_address(address) {}

    const QString &address() const { return m_address; }
    void setAddress(const QString &address) { m_address = address; }

    const QVector<Value> &arguments() const { return m_arguments; }
    int argumentCount() const { return m_arguments.size(); }

    void addInt(int32_t val) { m_arguments.append(Value(val)); }
    void addFloat(float val) { m_arguments.append(Value(val)); }
    void addString(const QString &val) { m_arguments.append(Value(val)); }
    void addBool(bool val) { m_arguments.append(Value(val)); }
    void addBlob(const QByteArray &val) { m_arguments.append(Value(val)); }
    void addNil() { m_arguments.append(Value()); }
    void addBang() { m_arguments.append(Value::makeBang()); }
    void addValue(const Value &val) { m_arguments.append(val); }

    Value arg(int index) const {
        if (index >= 0 && index < m_arguments.size()) {
            return m_arguments[index];
        }
        return Value();
    }

    QString printString() const {
        QString s = m_address;
        for (const auto &a : m_arguments) {
            s += QStringLiteral(" [") + a.toString() + QStringLiteral("]");
        }
        return s;
    }

private:
    QString m_address;
    QVector<Value> m_arguments;
};

} // namespace Osc
