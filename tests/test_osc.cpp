#include <QCoreApplication>
#include <QTest>
#include <QDebug>
#include "../src/osc/OscTypes.h"
#include "../src/osc/OscPacket.h"
#include "../src/osc/OscAddressRegistry.h"
#include "../src/osc/OscDispatcher.h"
#include "../src/osc/OscQueryTree.h"
#include "../src/osc/OscQueryServer.h"
#include "../src/osc/MdnsResponder.h"
#include <QJsonArray>
#include <QJsonDocument>
#include "../src/backend/null/NullAtemBackend.h"

// Records the calls the dispatcher makes, without needing a switcher
class RecordingBackend : public NullAtemBackend {
public:
    QStringList calls;

    void setProgramInput(int me, int64_t id) override { calls << QString("program %1 %2").arg(me).arg(id); }
    void setUskTie(int me, int key, bool tie) override { calls << QString("tie %1 %2 %3").arg(me).arg(key).arg(tie); }
    void setUskOnAir(int me, int key, bool on) override { calls << QString("onair %1 %2 %3").arg(me).arg(key).arg(on); }
    void setUskType(int me, int key, Atem::KeyType t) override { calls << QString("type %1 %2 %3").arg(me).arg(key).arg(int(t)); }
    void setUskMask(int me, int key, bool en, float t, float b, float l, float r) override {
        calls << QString("mask %1 %2 %3 %4 %5 %6 %7").arg(me).arg(key).arg(en).arg(t).arg(b).arg(l).arg(r);
    }
    void setFairlightEqBand(int64_t id, int band, bool en, float f, float g, float q) override {
        calls << QString("eq %1 %2 %3 %4 %5 %6").arg(id).arg(band).arg(en).arg(f).arg(g).arg(q);
    }
    void setDisplayClockLayout(float s, float x, float y, float o) override {
        calls << QString("clock %1 %2 %3 %4").arg(s).arg(x).arg(y).arg(o);
    }
    void setMultiViewLayout(int mv, Atem::MultiViewLayout l) override { calls << QString("layout %1 %2").arg(mv).arg(int(l)); }
};

static void addArg(Osc::Message &m, int v) { m.addInt(v); }
static void addArg(Osc::Message &m, float v) { m.addFloat(v); }
static void addArg(Osc::Message &m, bool v) { m.addBool(v); }
static void addArg(Osc::Message &m, const QString &v) { m.addString(v); }

class TestOsc : public QObject {
    Q_OBJECT
private slots:
    void testOscEncodeDecode() {
        Osc::Message msg("/atem/me/1/program");
        msg.addInt(5);
        msg.addFloat(0.75f);
        msg.addString("camera1");
        msg.addBool(true);

        QByteArray encoded = Osc::Packet::encodeMessage(msg);
        QVERIFY(!encoded.isEmpty());
        QVERIFY(encoded.size() % 4 == 0);

        QList<Osc::Message> decoded;
        bool ok = Osc::Packet::decode(encoded, decoded);
        QVERIFY(ok);
        QCOMPARE(decoded.size(), 1);

        const auto &outMsg = decoded[0];
        QCOMPARE(outMsg.address(), QString("/atem/me/1/program"));
        QCOMPARE(outMsg.argumentCount(), 4);
        QCOMPARE(outMsg.arg(0).toInt(), 5);
        QVERIFY(qAbs(outMsg.arg(1).toFloat() - 0.75f) < 0.001f);
        QCOMPARE(outMsg.arg(2).toString(), QString("camera1"));
        QCOMPARE(outMsg.arg(3).toBool(), true);
    }

    void testAddressRegistry() {
        Osc::AddressRegistry registry;
        QVERIFY(registry.rowCount() > 50);

        // Check essential endpoints exist
        bool foundCut = false;
        bool foundProgram = false;
        bool foundClock = false;
        bool foundMv = false;
        bool foundCamera = false;

        for (const auto &ep : registry.endpoints()) {
            if (ep.pattern == "/me/<me>/transition/cut") foundCut = true;
            if (ep.pattern == "/me/<me>/program") foundProgram = true;
            if (ep.pattern == "/clock/start") foundClock = true;
            if (ep.pattern == "/multiview/<mv>/layout") foundMv = true;
            if (ep.pattern == "/camera/<id>/iris") foundCamera = true;
        }

        QVERIFY(foundCut);
        QVERIFY(foundProgram);
        QVERIFY(foundClock);
        QVERIFY(foundMv);
        QVERIFY(foundCamera);
    }

    void testDispatcherRouting() {
        Osc::AddressRegistry registry;
        Osc::Dispatcher dispatcher(&registry);
        NullAtemBackend backend;
        Osc::Client client;

        dispatcher.addSwitcher("", &backend, &client);

        // Send a simulated program change
        Osc::Message progMsg("/atem/program");
        progMsg.addInt(3);
        dispatcher.dispatchMessage(progMsg, QHostAddress::LocalHost, 12345);

        // Send a transition cut
        Osc::Message cutMsg("/atem/me/1/transition/cut");
        dispatcher.dispatchMessage(cutMsg, QHostAddress::LocalHost, 12345);
    }

    void testDecodeRejectsMalformedPackets() {
        QList<Osc::Message> out;

        // Truncated int argument
        QByteArray truncated = Osc::Packet::encodeMessage([] { Osc::Message m("/atem/program"); m.addInt(1); return m; }());
        truncated.chop(2);
        QVERIFY(!Osc::Packet::decode(truncated, out));

        // Unknown type tag: the remaining payload cannot be parsed
        QByteArray unknownTag("/a\0\0,Z\0\0", 8);
        QVERIFY(!Osc::Packet::decode(unknownTag, out));

        // String without terminator
        QByteArray unterminated("/a\0\0,s\0\0abcd", 12);
        QVERIFY(!Osc::Packet::decode(unterminated, out));

        // Blob longer than the packet
        QByteArray badBlob("/a\0\0,b\0\0\x7f\xff\xff\xff", 12);
        QVERIFY(!Osc::Packet::decode(badBlob, out));
        QVERIFY(out.isEmpty());
    }

    void testBundleDecoding() {
        Osc::Message a("/atem/program"); a.addInt(1);
        Osc::Message b("/atem/preview"); b.addInt(2);
        QByteArray ea = Osc::Packet::encodeMessage(a), eb = Osc::Packet::encodeMessage(b);

        auto be32 = [](int v) { QByteArray r(4, 0); for (int i = 0; i < 4; ++i) r[i] = char((v >> (24 - 8 * i)) & 0xff); return r; };
        QByteArray bundle("#bundle\0", 8);
        bundle.append(QByteArray(8, 0));
        bundle += be32(ea.size()) + ea + be32(eb.size()) + eb;

        QList<Osc::Message> out;
        QVERIFY(Osc::Packet::decode(bundle, out));
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[1].address(), QString("/atem/preview"));

        // Deeply nested bundles must not blow the stack
        QByteArray nested = ea;
        for (int i = 0; i < 2000; ++i) {
            QByteArray wrap("#bundle\0", 8);
            wrap.append(QByteArray(8, 0));
            nested = wrap + be32(nested.size()) + nested;
        }
        out.clear();
        Osc::Packet::decode(nested, out);
        QVERIFY(out.isEmpty());
    }

    void testOscQueryTreeAndServer() {
        // Patterns expand to the concrete addresses
        Osc::QueryTree::Counts counts;
        counts.me = 2;
        QCOMPARE(Osc::QueryTree::expand("/me/<me>/usk/<key>/tie", counts).size(), 8);
        QVERIFY(Osc::QueryTree::expand("/dsk/<key>/tie", counts).contains("/dsk/2/tie"));
        QCOMPARE(Osc::QueryTree::expand("/audio/input/<number>/eq/<band>/gain", counts).size(), 8 * 6);

        Osc::AddressRegistry registry;
        Osc::Dispatcher dispatcher(&registry);
        RecordingBackend backend;
        Osc::Client client;
        dispatcher.addSwitcher("", &backend, &client);
        Osc::QueryTree tree(&registry, &dispatcher);
        Osc::QueryServer server(&tree);
        server.setOscPort(4321);

        QJsonObject n;
        QVERIFY(tree.node("/atem/me/1/program", n));
        QCOMPARE(n["TYPE"].toString(), QString("i"));
        QCOMPARE(n["FULL_PATH"].toString(), QString("/atem/me/1/program"));
        QVERIFY(n["DESCRIPTION"].toString().contains("M/E 1")); // placeholders filled in
        QVERIFY(!n.contains("VALUE")); // nothing reported yet

        QVERIFY(tree.node("/atem/me/1", n));
        QVERIFY(n["CONTENTS"].toObject().contains("program"));
        QVERIFY(tree.node("/", n));
        QVERIFY(n["CONTENTS"].toObject().contains("atem"));
        QVERIFY(!tree.node("/nope", n));

        // Values come from the feedback sent to the controllers
        Atem::MeState st;
        st.index = 1;
        st.programInput = 5;
        backend.meStateChanged(st);
        QVERIFY(tree.node("/atem/me/1/program", n));
        QCOMPARE(n["VALUE"].toArray().at(0).toInt(), 5);

        auto json = [](const QByteArray &b) { return QJsonDocument::fromJson(b).object(); };
        auto r = server.answer("GET", "/atem/me/1/program?VALUE");
        QCOMPARE(r.status, 200);
        QCOMPARE(json(r.body)["VALUE"].toArray().at(0).toInt(), 5);
        r = server.answer("GET", "/?HOST_INFO");
        QCOMPARE(json(r.body)["OSC_PORT"].toInt(), 4321);
        QCOMPARE(json(r.body)["OSC_TRANSPORT"].toString(), QString("UDP"));
        QCOMPARE(server.answer("GET", "/atem/me/1/transition/cut?VALUE").status, 204); // a method has no value
        QCOMPARE(server.answer("GET", "/nope").status, 404);
        QCOMPARE(server.answer("POST", "/").status, 405);
        QCOMPARE(server.answer("OPTIONS", "/").status, 204);
    }

    void testMdnsPackets() {
        const QByteArray name = Osc::MdnsResponder::encodeName("_oscjson._tcp.local");
        QCOMPARE(name.size(), 1 + 8 + 1 + 4 + 1 + 5 + 1);
        QCOMPARE(int(name.at(0)), 8);

        QVector<Osc::MdnsResponder::Service> services{{"_oscjson._tcp", 3334}, {"_osc._udp", 3333}};
        const QByteArray resp = Osc::MdnsResponder::buildResponse("AtemRemoteOsc", "atemremoteosc-host", services,
                                                                  {QHostAddress("192.168.0.10")}, 120);
        QCOMPARE(quint8(resp[2]), quint8(0x84)); // response, authoritative
        QCOMPARE(int(quint8(resp[7])), 2 * 3 + 1); // PTR + SRV + TXT per service, one A record
        QVERIFY(Osc::MdnsResponder::parseQuestions(resp).isEmpty()); // a response asks nothing

        // A query for the service type
        QByteArray query(12, 0);
        query[5] = 1; // one question
        query += Osc::MdnsResponder::encodeName("_oscjson._tcp.local");
        query += QByteArray("\x00\x0c\x00\x01", 4);
        QCOMPARE(Osc::MdnsResponder::parseQuestions(query), QStringList{"_oscjson._tcp.local"});

        // Compression pointer back to the first name, and a loop that must not hang
        QByteArray two(12, 0);
        two[5] = 2;
        two += Osc::MdnsResponder::encodeName("a.local");
        two += QByteArray("\x00\x01\x00\x01", 4);
        two += QByteArray("\xc0\x0c\x00\x01\x00\x01", 6);
        QCOMPARE(Osc::MdnsResponder::parseQuestions(two), (QStringList{"a.local", "a.local"}));
        QByteArray loop(12, 0);
        loop[5] = 1;
        loop += QByteArray("\xc0\x0c", 2);
        QVERIFY(Osc::MdnsResponder::parseQuestions(loop).isEmpty());
        QVERIFY(Osc::MdnsResponder::parseQuestions(QByteArray("\x00\x00", 2)).isEmpty());
    }

    void testLegacyAddressesAndParameterMerging() {
        Osc::AddressRegistry registry;
        Osc::Dispatcher dispatcher(&registry);
        RecordingBackend backend;
        Osc::Client client;
        dispatcher.addSwitcher("", &backend, &client);
        auto send = [&](const QString &addr, auto... args) {
            Osc::Message m(addr);
            (addArg(m, args), ...);
            dispatcher.dispatchMessage(m, QHostAddress::LocalHost, 1);
        };

        // Value in the address, default M/E
        send("/atem/program/5");
        send("/atem/usk/1/type/chroma");
        QCOMPARE(backend.calls, (QStringList{"program 1 5", "type 1 1 1"}));
        backend.calls.clear();

        // TouchOSC: value in the address plus a 1.0 press / 0.0 release
        send("/atem/program/7", 1.0f);
        send("/atem/program/8", 0.0f);
        QCOMPARE(backend.calls, (QStringList{"program 1 7"}));
        backend.calls.clear();

        // Toggle uses the last known state
        send("/atem/usk/2/on-air", true);
        send("/atem/usk/2/on-air/toggle");
        send("/atem/usk/2/tie/set-next", true); // on air now false -> must tie
        QCOMPARE(backend.calls, (QStringList{"onair 1 2 1", "onair 1 2 0", "tie 1 2 1"}));
        backend.calls.clear();

        // Partial updates keep previously set values
        send("/atem/me/1/usk/1/mask/top", 0.25f);
        send("/atem/me/1/usk/1/mask/left", 0.5f);
        QCOMPARE(backend.calls.last(), QString("mask 1 1 0 0.25 0 0.5 0"));
        backend.calls.clear();

        send("/atem/audio/input/3/eq/2/freq", 200.0f);
        send("/atem/audio/input/3/eq/2/gain", 4.0f);
        QCOMPARE(backend.calls.last(), QString("eq 3 2 1 200 4 0.71"));
        backend.calls.clear();

        send("/atem/clock/opacity", 0.5f);
        send("/atem/clock/size", 2.0f);
        QCOMPARE(backend.calls.last(), QString("clock 2 0.5 0.5 0.5"));
        backend.calls.clear();

        send("/atem/multiview/1/layout", QString("top"));
        QCOMPARE(backend.calls, (QStringList{QString("layout 1 %1").arg(int(Atem::MultiViewLayout::Top))}));
        backend.calls.clear();

        // Prefix must be an exact "/atem" segment
        send("/atemfoo/program/5");
        QVERIFY(backend.calls.isEmpty());
    }
};

QTEST_MAIN(TestOsc)
#include "test_osc.moc"
