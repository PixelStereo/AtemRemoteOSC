#include "TimedTransition.h"
#include <QtGlobal>

namespace Osc {

TimedTransition::TimedTransition(Atem::IAtemBackend *backend, int me, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_me(me)
{
    m_timer.setInterval(20); // 50 positions per second
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &TimedTransition::tick);
}

double TimedTransition::apply(Curve curve, double t)
{
    t = qBound(0.0, t, 1.0);
    switch (curve) {
        case Curve::EaseIn: return t * t;
        case Curve::EaseOut: return 1.0 - (1.0 - t) * (1.0 - t);
        case Curve::EaseInOut: return t * t * (3.0 - 2.0 * t); // smoothstep
        case Curve::Linear: break;
    }
    return t;
}

bool TimedTransition::parseCurve(const QString &name, Curve &curve)
{
    const QString n = name.trimmed().toLower().replace('_', '-');
    if (n == QLatin1String("linear")) curve = Curve::Linear;
    else if (n == QLatin1String("ease-in") || n == QLatin1String("in")) curve = Curve::EaseIn;
    else if (n == QLatin1String("ease-out") || n == QLatin1String("out")) curve = Curve::EaseOut;
    else if (n == QLatin1String("ease-in-out") || n == QLatin1String("ease") || n == QLatin1String("in-out")) curve = Curve::EaseInOut;
    else return false;
    return true;
}

QString TimedTransition::stateName(State state)
{
    switch (state) {
        case State::Running: return QStringLiteral("running");
        case State::Paused: return QStringLiteral("paused");
        case State::Idle: break;
    }
    return QStringLiteral("idle");
}

QStringList TimedTransition::curveNames()
{
    return {QStringLiteral("linear"), QStringLiteral("ease-in"), QStringLiteral("ease-out"), QStringLiteral("ease-in-out")};
}

void TimedTransition::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(state);
}

void TimedTransition::start(double seconds, Curve curve)
{
    if (m_state != State::Idle) cancel(); // a new one replaces the running one, from the start
    m_durationMs = qBound(kMinSeconds, seconds, kMaxSeconds) * 1000.0;
    m_curve = curve;
    m_elapsedBeforePauseMs = 0;
    m_progress = 0.0;
    m_clock.start();
    m_timer.start();
    setState(State::Running);
    emit progressChanged(0.0);
}

void TimedTransition::tick()
{
    const double elapsed = double(m_elapsedBeforePauseMs + m_clock.elapsed());
    const double t = qBound(0.0, elapsed / m_durationMs, 1.0);
    m_progress = t;
    m_backend->setTransitionPosition(m_me, static_cast<float>(apply(m_curve, t)));
    emit progressChanged(t);
    if (t >= 1.0) {
        m_timer.stop();
        emit finished();
        setState(State::Idle); // the switcher completes the transition and resets its bar by itself
    }
}

void TimedTransition::pause()
{
    if (m_state != State::Running) return;
    m_timer.stop();
    m_elapsedBeforePauseMs += m_clock.elapsed();
    setState(State::Paused);
}

void TimedTransition::resume()
{
    if (m_state != State::Paused) return;
    m_clock.start();
    m_timer.start();
    setState(State::Running);
}

void TimedTransition::stop()
{
    if (m_state == State::Idle) return;
    m_timer.stop();
    setState(State::Idle);
}

void TimedTransition::cancel()
{
    if (m_state == State::Idle) return;
    m_timer.stop();
    m_progress = 0.0;
    m_backend->setTransitionPosition(m_me, 0.0f);
    setState(State::Idle);
    emit progressChanged(0.0);
}

} // namespace Osc
