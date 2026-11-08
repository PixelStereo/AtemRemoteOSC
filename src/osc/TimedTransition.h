#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include "../backend/IAtemBackend.h"

namespace Osc {

// A transition of any length, driven by the application: the T-bar is moved from 0 to 1 in N seconds, the way a
// hand would. The switcher's own automatic transitions stop at 250 frames (10 s); the T-bar has no such limit.
class TimedTransition : public QObject {
    Q_OBJECT
public:
    enum class Curve { Linear, EaseIn, EaseOut, EaseInOut };
    enum class State { Idle, Running, Paused };

    static constexpr double kMinSeconds = 0.1;
    static constexpr double kMaxSeconds = 3600.0;

    TimedTransition(Atem::IAtemBackend *backend, int me, QObject *parent = nullptr);

    void start(double seconds, Curve curve);
    void pause();
    void resume();
    void stop();   // keeps the T-bar where it is
    void cancel(); // puts the T-bar back at the start

    State state() const { return m_state; }
    double progress() const { return m_progress; }
    int me() const { return m_me; }
    Atem::IAtemBackend *backend() const { return m_backend; }

    // t in [0, 1] -> position in [0, 1]
    static double apply(Curve curve, double t);
    static bool parseCurve(const QString &name, Curve &curve);
    static QString stateName(State state);
    static QStringList curveNames();

signals:
    void stateChanged(TimedTransition::State state);
    void progressChanged(double progress);

private:
    void tick();
    void setState(State state);

    Atem::IAtemBackend *m_backend;
    int m_me;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_elapsedBeforePauseMs = 0;
    double m_durationMs = 0.0;
    Curve m_curve = Curve::EaseInOut;
    State m_state = State::Idle;
    double m_progress = 0.0;
};

} // namespace Osc
