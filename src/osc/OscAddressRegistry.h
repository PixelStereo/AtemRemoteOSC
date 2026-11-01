#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMap>
#include <QAbstractTableModel>
#include "OscTypes.h"

namespace Osc {

struct EndpointDefinition {
    QString pattern;       // e.g. "/me/<me>/program"
    QString description;   // e.g. "Set Program Input for M/E <me>"
    QString valueType;     // e.g. "int" or "float" or "string" or "bool" or "none"
    QString category;      // e.g. "M/E & Transitions", "Audio", "Camera PTZ"
    bool feedback;         // true if switcher sends status updates on this address
    QString currentValue;  // live value for GUI display
};

class AddressRegistry : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Columns {
        ColPattern,
        ColDescription,
        ColType,
        ColCategory,
        ColFeedback,
        ColLiveValue,
        ColCount
    };

    explicit AddressRegistry(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void registerEndpoint(const QString &pattern, const QString &description,
                          const QString &valueType, const QString &category, bool feedback);
    int rowForAddress(const QString &address);
    void updateLiveValue(const QString &pattern, const QString &value);

    const QVector<EndpointDefinition>& endpoints() const { return m_endpoints; }
    void initStandardEndpoints();

private:
    QVector<EndpointDefinition> m_endpoints;
    QMap<QString, int> m_patternIndex;
    QHash<QString, int> m_concreteIndex; // concrete feedback address -> row (-1 if unknown)
};

} // namespace Osc
