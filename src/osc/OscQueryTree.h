#pragma once

#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>
#include "OscAddressRegistry.h"
#include "OscDispatcher.h"
#include <pxs/Tree.h>

namespace Osc {

// The OSC address space as OSCQuery describes it: the registered address patterns expanded into concrete
// addresses (M/E 1, 2, ... according to the connected switcher), with the last known values. It is a pxs::Tree, which
// gives the JSON and the attributes.
class QueryTree {
public:
    QueryTree(const AddressRegistry *registry, const Dispatcher *dispatcher);

    // The tree to serve (pxs::QueryServer answers from it); `refresh()` builds it, and again when the switchers or what
    // they have changed: true when it did
    const pxs::Tree *tree() const { return &m_tree; }
    bool refresh() { return rebuildIfNeeded(); }

    // The JSON node of an OSC path ("/" is the root); false when the path does not exist
    bool node(const QString &path, QJsonObject &out);
    // One attribute of a node (?VALUE, ?TYPE, ...); false when the node does not exist, `out` empty when the
    // attribute has no value (the HTTP answer is then 204)
    bool attribute(const QString &path, const QString &attribute, QJsonObject &out);

    // Concrete addresses a pattern stands for, given what the switcher has (exposed for tests)
    struct Counts {
        int me = 1, dsk = 2, aux = 1, hyperdeck = 1, macros = 100;
        QSet<qint64> audioInputs;
    };
    static QStringList expand(const QString &pattern, const Counts &counts);

private:
    bool rebuildIfNeeded();

    const AddressRegistry *m_registry;
    const Dispatcher *m_dispatcher;
    pxs::Tree m_tree; // the address space; OSCQuery is derived from it
    QString m_signature;
};

} // namespace Osc
