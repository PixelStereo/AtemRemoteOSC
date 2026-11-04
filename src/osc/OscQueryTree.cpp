#include "OscQueryTree.h"
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
    return QString();
}

QJsonValue toJson(const Value &v)
{
    switch (v.type()) {
        case Type::Int32: return v.toInt();
        case Type::Float32: return static_cast<double>(v.toFloat());
        case Type::True: return true;
        case Type::False: return false;
        case Type::String: return v.toString();
        default: return QJsonValue::Null;
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

void QueryTree::rebuildIfNeeded()
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

    if (signature == m_signature && !m_leaves.isEmpty()) return;
    m_signature = signature;
    m_leaves.clear();

    if (backends.isEmpty()) {
        nicknames.append(QString());
        backends.append(nullptr);
    }
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
            Leaf leaf;
            leaf.type = typeTag(ep.valueType);
            leaf.feedback = ep.feedback;
            leaf.description = ep.description;
            leaf.hasRange = ep.hasRange;
            leaf.rangeMin = ep.rangeMin;
            leaf.rangeMax = ep.rangeMax;
            leaf.choices = ep.choices;
            const QStringList paths = expand(ep.pattern, c);
            const QStringList patternParts = ep.pattern.split('/');
            for (const QString &p : paths) {
                if (m_leaves.size() >= kMaxLeaves) return;
                // "Set Program input for M/E <me>" -> "... M/E 1"
                Leaf concrete = leaf;
                const QStringList parts = p.split('/');
                for (int i = 0; i < patternParts.size() && i < parts.size(); ++i) {
                    if (patternParts[i].startsWith('<')) concrete.description.replace(patternParts[i], parts[i]);
                }
                m_leaves.insert(prefix + p, concrete);
            }
        }
    }
}

bool QueryTree::hasChildren(const QString &path) const
{
    const QString prefix = path == QLatin1String("/") ? QStringLiteral("/") : path + QLatin1Char('/');
    auto it = m_leaves.lowerBound(prefix);
    return it != m_leaves.end() && it.key().startsWith(prefix);
}

QJsonObject QueryTree::leafJson(const QString &path, const Leaf &leaf) const
{
    QJsonObject o;
    o["FULL_PATH"] = path;
    o["ACCESS"] = leaf.feedback ? 3 : 2; // read/write, or write only
    if (!leaf.description.isEmpty()) o["DESCRIPTION"] = leaf.description;
    if (!leaf.type.isEmpty()) {
        o["TYPE"] = leaf.type;
        // One entry per argument: the interval of a number, or the accepted strings
        if (leaf.hasRange && (leaf.type == QLatin1String("f") || leaf.type == QLatin1String("i"))) {
            QJsonObject range;
            range["MIN"] = leaf.rangeMin;
            range["MAX"] = leaf.rangeMax;
            o["RANGE"] = QJsonArray{range};
        } else if (!leaf.choices.isEmpty() && leaf.type == QLatin1String("s")) {
            QJsonObject range;
            range["VALS"] = QJsonArray::fromStringList(leaf.choices);
            o["RANGE"] = QJsonArray{range};
        }
        if (leaf.feedback) {
            const QVector<Value> last = m_dispatcher->lastFeedback(path);
            if (!last.isEmpty()) {
                QJsonArray values;
                for (const auto &v : last) values.append(toJson(v));
                o["VALUE"] = values;
            }
        }
    }
    return o;
}

QJsonObject QueryTree::containerJson(const QString &path, int depth) const
{
    QJsonObject o;
    o["FULL_PATH"] = path;
    o["ACCESS"] = 0;
    if (path == QLatin1String("/")) o["DESCRIPTION"] = QStringLiteral("AtemRemoteOsc");

    const QString prefix = path == QLatin1String("/") ? QStringLiteral("/") : path + QLatin1Char('/');
    QJsonObject contents;
    QString lastChild;
    for (auto it = m_leaves.lowerBound(prefix); it != m_leaves.end() && it.key().startsWith(prefix); ++it) {
        const QString rest = it.key().mid(prefix.size());
        const QString child = rest.section('/', 0, 0);
        if (child == lastChild) continue;
        lastChild = child;
        const QString childPath = prefix + child;
        auto leafIt = m_leaves.constFind(childPath);
        if (leafIt != m_leaves.constEnd() && !hasChildren(childPath)) {
            contents[child] = leafJson(childPath, leafIt.value());
        } else {
            contents[child] = containerJson(childPath, depth + 1);
        }
    }
    o["CONTENTS"] = contents;
    return o;
}

bool QueryTree::node(const QString &pathIn, QJsonObject &out)
{
    rebuildIfNeeded();
    QString path = pathIn;
    while (path.size() > 1 && path.endsWith('/')) path.chop(1);
    if (path.isEmpty()) path = QStringLiteral("/");

    if (path == QLatin1String("/") || hasChildren(path)) {
        if (path != QLatin1String("/") && !hasChildren(path)) return false;
        out = containerJson(path, 0);
        // A method that also has sub-addresses keeps its own attributes
        auto it = m_leaves.constFind(path);
        if (it != m_leaves.constEnd()) {
            const QJsonObject self = leafJson(path, it.value());
            for (auto k = self.begin(); k != self.end(); ++k) {
                if (!out.contains(k.key())) out[k.key()] = k.value();
            }
        }
        return true;
    }
    auto it = m_leaves.constFind(path);
    if (it == m_leaves.constEnd()) return false;
    out = leafJson(path, it.value());
    return true;
}

bool QueryTree::attribute(const QString &path, const QString &attributeName, QJsonObject &out)
{
    QJsonObject nodeJson;
    if (!node(path, nodeJson)) return false;
    out = QJsonObject();
    const QString key = attributeName.toUpper();
    if (nodeJson.contains(key)) out[key] = nodeJson.value(key);
    return true;
}

QJsonObject QueryTree::hostInfo(const QString &name, quint16 oscPort) const
{
    QJsonObject extensions;
    for (const char *k : {"ACCESS", "VALUE", "DESCRIPTION", "RANGE"}) extensions[QLatin1String(k)] = true;
    for (const char *k : {"TAGS", "CLIPMODE", "UNIT", "CRITICAL", "LISTEN", "PATH_CHANGED",
                          "PATH_RENAMED", "PATH_ADDED", "PATH_REMOVED"}) extensions[QLatin1String(k)] = false;
    QJsonObject o;
    o["NAME"] = name;
    o["EXTENSIONS"] = extensions;
    o["OSC_PORT"] = static_cast<int>(oscPort);
    o["OSC_TRANSPORT"] = QStringLiteral("UDP");
    return o;
}

} // namespace Osc
