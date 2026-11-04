#pragma once

#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>
#include "OscAddressRegistry.h"
#include "OscDispatcher.h"

namespace Osc {

// The OSC address space as OSCQuery describes it: the registered address patterns expanded into concrete
// addresses (M/E 1, 2, ... according to the connected switcher), with the last known values.
class QueryTree {
public:
    QueryTree(const AddressRegistry *registry, const Dispatcher *dispatcher);

    // The JSON node of an OSC path ("/" is the root); false when the path does not exist
    bool node(const QString &path, QJsonObject &out);
    // One attribute of a node (?VALUE, ?TYPE, ...); false when the node does not exist, `out` empty when the
    // attribute has no value (the HTTP answer is then 204)
    bool attribute(const QString &path, const QString &attribute, QJsonObject &out);

    QJsonObject hostInfo(const QString &name, quint16 oscPort) const;

    // Concrete addresses a pattern stands for, given what the switcher has (exposed for tests)
    struct Counts {
        int me = 1, dsk = 2, aux = 1, hyperdeck = 1, macros = 100;
        QSet<qint64> audioInputs;
    };
    static QStringList expand(const QString &pattern, const Counts &counts);

private:
    struct Leaf {
        QString type;        // OSC type tag, empty when the method takes no argument
        bool feedback = false;
        QString description;
    };

    void rebuildIfNeeded();
    QJsonObject leafJson(const QString &path, const Leaf &leaf) const;
    QJsonObject containerJson(const QString &path, int depth) const;
    bool hasChildren(const QString &path) const;

    const AddressRegistry *m_registry;
    const Dispatcher *m_dispatcher;
    QMap<QString, Leaf> m_leaves; // full OSC path -> leaf, sorted
    QString m_signature;
};

} // namespace Osc
