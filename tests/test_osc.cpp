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
#include "../src/osc/TimedTransition.h"
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
    QMap<int, Atem::MeState> meMap;
    const QMap<int, Atem::MeState>& meBlocks() const override { return meMap; }
    double frameRate() const override { return 1000.0; } // so that a few hundred frames last a fraction of a second
    QStringList log;
    void setTransitionStyle(int me, Atem::TransitionStyle s) override { log << QString("style %1").arg(int(s)); Q_UNUSED(me); }
    void setPreviewInput(int me, int64_t id) override { log << QString("preview %1").arg(id); Q_UNUSED(me); }
    void performAutoTransition(int) override { log << "auto"; }
    void performFadeToBlack(int) override { log << "ftb"; }
    void setFadeToBlackRate(int, int frames) override { log << QString("ftbrate %1").arg(frames); }
    void setTransitionRate(int, Atem::TransitionStyle, float frames) override { log << QString("rate %1").arg(frames); }
    QVector<float> positions;
    void setTransitionPosition(int, float p) override { positions << p; }
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

    void testTimedTransitionCurves() {
        using C = Osc::TimedTransition::Curve;
        for (C curve : {C::Linear, C::EaseIn, C::EaseOut, C::EaseInOut}) {
            QCOMPARE(Osc::TimedTransition::apply(curve, 0.0), 0.0);
            QCOMPARE(Osc::TimedTransition::apply(curve, 1.0), 1.0);
        }
        QCOMPARE(Osc::TimedTransition::apply(C::Linear, 0.3), 0.3);
        QCOMPARE(Osc::TimedTransition::apply(C::EaseIn, 0.5), 0.25);
        QCOMPARE(Osc::TimedTransition::apply(C::EaseOut, 0.5), 0.75);
        QCOMPARE(Osc::TimedTransition::apply(C::EaseInOut, 0.5), 0.5);
        QVERIFY(Osc::TimedTransition::apply(C::EaseInOut, 0.1) < 0.1); // slow start
        QVERIFY(Osc::TimedTransition::apply(C::EaseInOut, 0.9) > 0.9); // slow end

        C parsed = C::Linear;
        QVERIFY(Osc::TimedTransition::parseCurve("ease", parsed));
        QCOMPARE(int(parsed), int(C::EaseInOut));
        QVERIFY(Osc::TimedTransition::parseCurve("Ease_In", parsed));
        QCOMPARE(int(parsed), int(C::EaseIn));
        QVERIFY(!Osc::TimedTransition::parseCurve("bounce", parsed));
    }

    void testTimedTransitionViaOsc() {
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
        auto increasing = [](const QVector<float> &v) {
            for (int i = 1; i < v.size(); ++i) if (v[i] < v[i - 1]) return false;
            return true;
        };

        // No curve given: ease-in-out, ends at 1.0
        send("/atem/transition/timed", 0.3f);
        QTest::qWait(450);
        QVERIFY(backend.positions.size() > 2); // loaded CI runners fire the 20 ms timer late
        QVERIFY(increasing(backend.positions));
        QCOMPARE(backend.positions.last(), 1.0f);
        QVERIFY(backend.positions.first() < 0.1f); // slow start
        backend.positions.clear();

        // Curve as second argument, and in the address
        send("/atem/transition/timed", 0.2f, QString("linear"));
        QTest::qWait(350);
        QCOMPARE(backend.positions.last(), 1.0f);
        backend.positions.clear();
        send("/atem/me/1/transition/timed/ease-out", 0.2f);
        QTest::qWait(350);
        QCOMPARE(backend.positions.last(), 1.0f);
        backend.positions.clear();

        // Pause, resume
        send("/atem/transition/timed/linear", 5.0f);
        QTest::qWait(120);
        send("/atem/transition/timed/pause");
        const int paused = backend.positions.size();
        QVERIFY(paused > 0);
        QTest::qWait(100);
        QCOMPARE(backend.positions.size(), paused);
        send("/atem/transition/timed/resume");
        QTest::qWait(80);
        QVERIFY(backend.positions.size() > paused);

        // A cut takes over: the bar goes back to the start and nothing more is sent
        send("/atem/transition/cut");
        QCOMPARE(backend.positions.last(), 0.0f);
        const int afterCut = backend.positions.size();
        QTest::qWait(100);
        QCOMPARE(backend.positions.size(), afterCut);
        backend.positions.clear();

        // The user takes the bar: the timed transition stops, the bar stays where the user put it
        send("/atem/transition/timed/linear", 5.0f);
        QTest::qWait(60);
        send("/atem/transition/bar", 0.5f);
        const int afterBar = backend.positions.size();
        QCOMPARE(backend.positions.last(), 0.5f);
        QTest::qWait(100);
        QCOMPARE(backend.positions.size(), afterBar);

        // Cancel, and a button release (0.0) starts nothing
        send("/atem/transition/timed/linear", 5.0f);
        QTest::qWait(60);
        send("/atem/transition/timed/cancel");
        QCOMPARE(backend.positions.last(), 0.0f);
        backend.positions.clear();
        send("/atem/transition/timed/linear", 0.0f);
        QTest::qWait(60);
        QVERIFY(backend.positions.isEmpty());
    }

    void testLongTransitionsKeepTheSameAddresses() {
        Osc::AddressRegistry registry;
        Osc::Dispatcher dispatcher(&registry);
        RecordingBackend backend;
        Osc::Client client;
        dispatcher.addSwitcher("", &backend, &client);
        Atem::MeState me;
        me.index = 1;
        me.programInput = 3;
        me.previewInput = 5;
        me.transitionStyle = Atem::TransitionStyle::Wipe;
        backend.meMap[1] = me;
        auto send = [&](const QString &addr, auto... args) {
            Osc::Message m(addr);
            (addArg(m, args), ...);
            dispatcher.dispatchMessage(m, QHostAddress::LocalHost, 1);
        };

        // Up to 250 frames: the switcher's own transition
        send("/atem/transition/rate", 100.0f);
        send("/atem/transition/auto");
        QCOMPARE(backend.log, (QStringList{"rate 100", "auto"}));
        QVERIFY(backend.positions.isEmpty());
        backend.log.clear();

        // Above: the switcher is given its maximum and the application drives the T-bar (300 frames = 0.3 s here)
        send("/atem/transition/rate", 300.0f);
        send("/atem/transition/auto");
        QCOMPARE(backend.log, (QStringList{"rate 250"}));
        QTest::qWait(450);
        QVERIFY(backend.positions.size() > 2); // loaded CI runners fire the 20 ms timer late
        QCOMPARE(backend.positions.last(), 1.0f);
        QVERIFY(backend.positions.at(backend.positions.size() / 2) < 0.9f); // progressive
        backend.positions.clear();
        backend.log.clear();

        // Back to a short duration: the switcher's own again
        send("/atem/transition/rate", 50.0f);
        send("/atem/transition/auto");
        QCOMPARE(backend.log, (QStringList{"rate 50", "auto"}));
        backend.log.clear();

        // Fade to black: within 250 frames, the switcher's
        send("/atem/transition/ftb");
        QCOMPARE(backend.log, QStringList{"ftb"});
        backend.log.clear();

        // Above: a mix to Black through the T-bar, then back to the program on the next press
        send("/atem/transition/ftb/rate", 300.0f);
        QCOMPARE(backend.log, QStringList{"ftbrate 250"});
        backend.log.clear();
        send("/atem/transition/ftb");
        QCOMPARE(backend.log, (QStringList{"style 0", "preview 0"})); // mix, towards Black (input 0)
        QTest::qWait(450);
        QCOMPARE(backend.positions.last(), 1.0f);
        QCOMPARE(backend.log, (QStringList{"style 0", "preview 0", "style 2", "preview 3"})); // style and old program restored
        QVERIFY(!backend.log.contains("ftb"));
        backend.positions.clear();
        backend.log.clear();

        send("/atem/transition/ftb"); // fade back in
        QCOMPARE(backend.log, (QStringList{"style 0", "preview 3"}));
        QTest::qWait(450);
        QCOMPARE(backend.positions.last(), 1.0f);
        QCOMPARE(backend.log.last(), QString("preview 5")); // the preview it had before
        backend.positions.clear();
        backend.log.clear();

        // Pressed during the fade: abandoned, everything put back
        send("/atem/transition/ftb");
        QTest::qWait(60);
        send("/atem/transition/ftb");
        QCOMPARE(backend.positions.last(), 0.0f);
        QCOMPARE(backend.log.last(), QString("preview 5"));
    }

    void testOscQueryRanges() {
        Osc::AddressRegistry registry;
        Osc::Dispatcher dispatcher(&registry);
        RecordingBackend backend;
        Osc::Client client;
        dispatcher.addSwitcher("", &backend, &client);
        Osc::QueryTree tree(&registry, &dispatcher);

        auto range = [&](const QString &path) {
            QJsonObject n;
            [&] { QVERIFY(tree.node(path, n)); }();
            return n["RANGE"].toArray().at(0).toObject();
        };
        QJsonObject r = range("/atem/clock/opacity");
        QCOMPARE(r["MIN"].toDouble(), 0.0);
        QCOMPARE(r["MAX"].toDouble(), 1.0);
        r = range("/atem/audio/input/1/gain");
        QCOMPARE(r["MIN"].toDouble(), -60.0);
        QCOMPARE(r["MAX"].toDouble(), 6.0);
        r = range("/atem/me/1/usk/1/chroma/hue");
        QCOMPARE(r["MAX"].toDouble(), 360.0);
        r = range("/atem/me/1/usk/1/mask/left");
        QCOMPARE(r["MIN"].toDouble(), -16.0);
        r = range("/atem/camera/1/ptz/pan");
        QCOMPARE(r["MIN"].toDouble(), -1.0);
        r = range("/atem/me/1/transition/type");
        QCOMPARE(r["VALS"].toArray().size(), 5);
        QVERIFY(r["VALS"].toArray().contains("wipe"));
        QVERIFY(range("/atem/me/1/program").isEmpty()); // no known range: a plain number field

        // A range only makes sense on a number, a list of choices only on a string
        for (const auto &ep : registry.endpoints()) {
            if (ep.hasRange) {
                QVERIFY2(ep.valueType == "float" || ep.valueType == "int", qPrintable(ep.pattern));
                QVERIFY2(ep.rangeMin < ep.rangeMax, qPrintable(ep.pattern));
            }
            if (!ep.choices.isEmpty()) QVERIFY2(ep.valueType == "string", qPrintable(ep.pattern));
        }
        QJsonObject info = tree.hostInfo("x", 1);
        QVERIFY(info["EXTENSIONS"].toObject()["RANGE"].toBool());
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
