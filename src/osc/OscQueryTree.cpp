#include "OscQueryTree.h"

#include <pxs/Convert.h>
#include <pxs/core/tree/oscquery.h>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>

namespace Osc {

namespace {

constexpr int kMaxLeaves = 40000;

QString typeTag(const QString &valueType)
{
    if (valueType == QLatin1String("int")) return QStringLiteral("i");
    if (valueType == QLatin1String("float")) return QStringLiteral("f");
    if (valueType == QLatin1String("bool")) return QStringLiteral("T");
    if (valueType == QLatin1String("string")) return QStringLiteral("s");
    return valueType == QLatin1String("none") ? QStringLiteral("I") : QString();
}

pxs::core::Value toValue(const Value &v)
{
    switch (v.type()) {
        case Type::Int32: return pxs::core::Value(static_cast<int>(v.toInt()));
        case Type::Float32: return pxs::core::Value(static_cast<double>(v.toFloat()));
        case Type::True: return pxs::core::Value(true);
        case Type::False: return pxs::core::Value(false);
        case Type::String: return pxs::core::Value(pxs::convert::toUtf8(v.toString()));
        default: return pxs::core::Value();
    }
}

// The values a placeholder takes, from the segment that holds it
QList<qint64> rangeFor(const QString &previous, const QString &placeholder, const QString &fullPattern,
                       const QueryTree::Counts &c)
{
    auto seq = [](qint64 from, qint64 to) {
        QList<qint64> r;
        for (qint64 i = from; i <= to; ++i) r.append(i);
        return r;
    };
    if (placeholder == QLatin1String("<me>")) return seq(1, qMax(1, c.me));
    if (placeholder == QLatin1String("<key>")) {
        if (previous == QLatin1String("dsk")) return seq(1, qMax(1, c.dsk));
        return seq(1, 4); // upstream keyers, SuperSource boxes
    }
    if (placeholder == QLatin1String("<band>")) return seq(0, 5);
    if (placeholder == QLatin1String("<win>")) return seq(1, 10);
    if (placeholder == QLatin1String("<mv>") || placeholder == QLatin1String("<player>")) return seq(1, 2);
    if (placeholder == QLatin1String("<id>")) return seq(1, 8); // cameras
    if (placeholder == QLatin1String("<index>")) return seq(0, qMax(1, qMin(c.macros, 100)) - 1);
    if (placeholder == QLatin1String("<number>")) {
        if (previous == QLatin1String("input") && fullPattern.contains(QLatin1String("/audio/"))) {
            if (!c.audioInputs.isEmpty()) {
                QList<qint64> r(c.audioInputs.begin(), c.audioInputs.end());
                std::sort(r.begin(), r.end());
                return r;
            }
            return seq(1, 8);
        }
        if (previous == QLatin1String("hyperdeck")) return seq(1, qMax(1, c.hyperdeck));
        return seq(1, qMax(1, c.aux)); // aux outputs
    }
    return seq(1, 1);
}

} // namespace

QueryTree::QueryTree(const AddressRegistry *registry, const Dispatcher *dispatcher)
    : m_registry(registry)
    , m_dispatcher(dispatcher)
{
}

QStringList QueryTree::expand(const QString &pattern, const Counts &counts)
{
    const QStringList segments = pattern.split('/');
    QStringList results{QString()};
    QString previous;
    for (const QString &seg : segments) {
        if (seg.isEmpty()) { previous.clear(); continue; }
        QStringList next;
        if (seg.startsWith('<') && seg.endsWith('>')) {
            const auto values = rangeFor(previous, seg, pattern, counts);
            for (const QString &base : results) {
                for (qint64 v : values) next.append(base + QLatin1Char('/') + QString::number(v));
            }
        } else {
            for (const QString &base : results) next.append(base + QLatin1Char('/') + seg);
        }
        results = next;
        previous = seg;
    }
    return results;
}

bool QueryTree::rebuildIfNeeded()
{
    // What the tree depends on: the switchers, what they have, and the audio inputs seen so far
    Counts counts;
    QString signature;
    QStringList nicknames;
    QList<Atem::IAtemBackend*> backends;
    for (const auto &r : m_dispatcher->routes()) {
        nicknames.append(r.nickname);
        backends.append(r.backend);
        signature += QStringLiteral("%1:%2:%3:%4:%5;").arg(r.nickname).arg(r.backend->meCount())
                         .arg(r.backend->dskCount()).arg(r.backend->auxCount()).arg(r.backend->hyperdeckCount());
    }
    static const QRegularExpression rxAudio("^(?:/atem(?:/[^/]+)?)?/audio/input/(\\d+)/");
    QSet<qint64> audio;
    const QStringList fbAddresses = m_dispatcher->feedbackAddresses();
    for (const QString &a : fbAddresses) {
        auto m = rxAudio.match(a);
        if (m.hasMatch()) audio.insert(m.captured(1).toLongLong());
    }
    QList<qint64> sorted(audio.begin(), audio.end());
    std::sort(sorted.begin(), sorted.end());
    for (qint64 a : sorted) signature += QString::number(a) + QLatin1Char(',');
    signature += QString::number(m_registry->endpoints().size());

    if (signature == m_signature && !m_tree.root().children().empty()) return false;
    m_signature = signature;
    m_tree.clear();
    m_tree.root().description = "AtemRemoteOsc";

    if (backends.isEmpty()) {
        nicknames.append(QString());
        backends.append(nullptr);
    }
    int leaves = 0;
    for (int i = 0; i < backends.size(); ++i) {
        Counts c;
        c.audioInputs = audio;
        if (backends[i]) {
            c.me = backends[i]->meCount();
            c.dsk = backends[i]->dskCount();
            c.aux = backends[i]->auxCount();
            c.hyperdeck = backends[i]->hyperdeckCount();
            c.macros = static_cast<int>(backends[i]->maxMacros());
        }
        const QString prefix = nicknames[i].isEmpty() ? QStringLiteral("/atem") : QStringLiteral("/atem/") + nicknames[i];
        for (const auto &ep : m_registry->endpoints()) {
            const QString type = typeTag(ep.valueType);
            std::vector<pxs::core::tree::ArgRange> range;
            // One entry per argument: the interval of a number, or the accepted strings
            if (ep.hasRange && (type == QLatin1String("f") || type == QLatin1String("i"))) {
                range = {pxs::core::tree::ArgRange{pxs::core::Value(static_cast<double>(ep.rangeMin)), pxs::core::Value(static_cast<double>(ep.rangeMax)), {}, {}}};
            } else if (!ep.choices.isEmpty() && type == QLatin1String("s")) {
                pxs::core::tree::ArgRange keys;
                for (const QString &choice : ep.choices) keys.vals.emplace_back(pxs::convert::toUtf8(choice));
                range = {keys};
            }
            const QStringList paths = expand(ep.pattern, c);
            const QStringList patternParts = ep.pattern.split('/');
            for (const QString &p : paths) {
                if (++leaves > kMaxLeaves) return true;
                // "Set Program input for M/E <me>" -> "... M/E 1"
                QString description = ep.description;
                const QStringList parts = p.split('/');
                for (int k = 0; k < patternParts.size() && k < parts.size(); ++k) {
                    if (patternParts[k].startsWith('<')) description.replace(patternParts[k], parts[k]);
                }
                const QString full = prefix + p;
                pxs::core::tree::Node &node = m_tree.add(pxs::convert::toUtf8(full));
                node.description = pxs::convert::toUtf8(description);
                const bool readable = ep.feedback && ep.valueType != QLatin1String("none");
                node.access = (readable ? 1 : 0) | (ep.writable ? 2 : 0);
                node.type = pxs::convert::toUtf8(type);
                if (!type.isEmpty()) {
                    node.range = range;
                    if (readable) {
                        const Dispatcher *dispatcher = m_dispatcher;
                        node.get = [dispatcher, full] {
                            pxs::core::ValueList values;
                            for (const Value &v : dispatcher->lastFeedback(full)) values.push_back(toValue(v));
                            return values;
                        };
                    }
                }
            }
        }
    }
    return true;
}

bool QueryTree::node(const QString &pathIn, QJsonObject &out)
{
    rebuildIfNeeded();
    out = pxs::convert::jsonObjectFromText(pxs::core::tree::describeJson(m_tree, pxs::convert::toUtf8(pathIn)));
    return !out.isEmpty();
}

bool QueryTree::attribute(const QString &path, const QString &attributeName, QJsonObject &out)
{
    rebuildIfNeeded();
    std::string text;
    if (!pxs::core::tree::attributeJson(m_tree, pxs::convert::toUtf8(path), pxs::convert::toUtf8(attributeName), text)) return false;
    out = pxs::convert::jsonObjectFromText(text);
    return true;
}

} // namespace Osc
