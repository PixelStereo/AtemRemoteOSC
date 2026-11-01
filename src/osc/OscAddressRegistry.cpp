#include "OscAddressRegistry.h"

namespace Osc {

AddressRegistry::AddressRegistry(QObject *parent)
    : QAbstractTableModel(parent)
{
    initStandardEndpoints();
}

int AddressRegistry::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_endpoints.size();
}

int AddressRegistry::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant AddressRegistry::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_endpoints.size()) {
        return QVariant();
    }

    const auto &ep = m_endpoints[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColPattern:     return QStringLiteral("/atem") + ep.pattern;
            case ColDescription: return ep.description;
            case ColType:        return ep.valueType;
            case ColCategory:    return ep.category;
            case ColFeedback:    return ep.feedback ? QStringLiteral("Yes") : QStringLiteral("No");
            case ColLiveValue:   return ep.currentValue;
            default: break;
        }
    }
    return QVariant();
}

QVariant AddressRegistry::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColPattern:     return QStringLiteral("OSC Address Pattern");
            case ColDescription: return QStringLiteral("Description");
            case ColType:        return QStringLiteral("Type");
            case ColCategory:    return QStringLiteral("Category");
            case ColFeedback:    return QStringLiteral("Feedback");
            case ColLiveValue:   return QStringLiteral("Live Value");
            default: break;
        }
    }
    return QVariant();
}

void AddressRegistry::registerEndpoint(const QString &pattern, const QString &description,
                                       const QString &valueType, const QString &category, bool feedback) {
    if (m_patternIndex.contains(pattern)) {
        int row = m_patternIndex[pattern];
        m_endpoints[row].description = description;
        m_endpoints[row].valueType = valueType;
        m_endpoints[row].category = category;
        m_endpoints[row].feedback = feedback;
        emit dataChanged(index(row, 0), index(row, ColCount - 1));
        return;
    }

    int newRow = m_endpoints.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    EndpointDefinition def;
    def.pattern = pattern;
    def.description = description;
    def.valueType = valueType;
    def.category = category;
    def.feedback = feedback;
    m_endpoints.append(def);
    m_patternIndex[pattern] = newRow;
    endInsertRows();
}

// Registered patterns contain placeholders ("/me/<me>/program"); feedback uses concrete addresses
// ("/me/1/program"). A placeholder segment matches any numeric segment.
int AddressRegistry::rowForAddress(const QString &address) {
    auto exact = m_patternIndex.constFind(address);
    if (exact != m_patternIndex.constEnd()) return exact.value();

    auto cached = m_concreteIndex.constFind(address);
    if (cached != m_concreteIndex.constEnd()) return cached.value();

    const QStringList parts = address.split('/');
    int found = -1;
    for (int row = 0; row < m_endpoints.size() && found < 0; ++row) {
        const QStringList patternParts = m_endpoints[row].pattern.split('/');
        if (patternParts.size() != parts.size()) continue;
        bool match = true;
        for (int i = 0; i < parts.size() && match; ++i) {
            if (patternParts[i].startsWith('<')) {
                bool numeric = false;
                parts[i].toLongLong(&numeric);
                match = numeric;
            } else {
                match = (patternParts[i] == parts[i]);
            }
        }
        if (match) found = row;
    }
    m_concreteIndex.insert(address, found);
    return found;
}

void AddressRegistry::updateLiveValue(const QString &pattern, const QString &value) {
    const int row = rowForAddress(pattern);
    if (row >= 0) {
        m_endpoints[row].currentValue = value;
        emit dataChanged(index(row, ColLiveValue), index(row, ColLiveValue));
    }
}

void AddressRegistry::initStandardEndpoints() {
    // M/E & Program / Preview
    registerEndpoint("/send-status", "Request complete switcher status refresh", "none", "System", false);
    registerEndpoint("/me/<me>/send-status", "Request M/E status refresh", "none", "M/E", false);
    registerEndpoint("/me/<me>/program", "Set Program input for M/E <me>", "int", "M/E", true);
    registerEndpoint("/me/<me>/preview", "Set Preview input for M/E <me>", "int", "M/E", true);
    registerEndpoint("/program", "Set Program input for M/E 1 (alias)", "int", "M/E", true);
    registerEndpoint("/preview", "Set Preview input for M/E 1 (alias)", "int", "M/E", true);

    // Transitions
    registerEndpoint("/me/<me>/transition/cut", "Perform Cut transition on M/E <me>", "none", "Transitions", false);
    registerEndpoint("/me/<me>/transition/auto", "Perform Auto transition on M/E <me>", "none", "Transitions", false);
    registerEndpoint("/me/<me>/transition/ftb", "Perform Fade To Black (FTB) on M/E <me>", "none", "Transitions", true);
    registerEndpoint("/me/<me>/transition/bar", "Set T-Bar position (0.0 to 1.0)", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/position", "Set transition position (0.0 to 1.0)", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/preview", "Toggle or set preview transition", "bool", "Transitions", true);
    registerEndpoint("/me/<me>/transition/type", "Set next transition style (mix, dip, wipe, sting, dve)", "string", "Transitions", true);
    registerEndpoint("/me/<me>/transition/rate", "Set rate for current transition style in frames", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/mix/rate", "Set Mix transition rate in frames", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/dip/rate", "Set Dip transition rate in frames", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/wipe/rate", "Set Wipe transition rate in frames", "float", "Transitions", true);
    registerEndpoint("/me/<me>/transition/dve/rate", "Set DVE transition rate in frames", "float", "Transitions", true);

    // USK (Upstream Keyers)
    registerEndpoint("/me/<me>/usk/<key>/tie", "Set USK <key> Tie in next transition", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/tie/toggle", "Toggle USK <key> Tie", "none", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/on-air", "Set USK <key> On Air", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/on-air/toggle", "Toggle USK <key> On Air", "none", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/source/fill", "Set Fill input for USK <key>", "int", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/source/cut", "Set Cut/Key input for USK <key>", "int", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/type", "Set USK <key> type (luma, chroma, pattern, dve)", "string", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/luma/pre-multiplied", "Set Luma pre-multiplied parameter", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/luma/clip", "Set Luma clip level (0.0 to 1.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/luma/gain", "Set Luma gain level (0.0 to 1.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/luma/inverse", "Set Luma inverse parameter", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/hue", "Set Chroma hue (0.0 to 360.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/gain", "Set Chroma gain level (0.0 to 1.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/y-suppress", "Set Chroma Y-Suppress (0.0 to 1.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/lift", "Set Chroma lift (0.0 to 1.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/sample-x", "Set Advanced Chroma sampling X position", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/chroma/sample-y", "Set Advanced Chroma sampling Y position", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/enabled", "Enable or disable Flying Key (DVE)", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/position-x", "Set DVE Fly X position", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/position-y", "Set DVE Fly Y position", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/size-x", "Set DVE Fly X scale", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/size-y", "Set DVE Fly Y scale", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/rotation", "Set DVE Fly rotation in degrees", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/fly/reset", "Reset DVE Fly size, pos and rotation", "none", "Upstream Keyers", false);
    registerEndpoint("/me/<me>/usk/<key>/mask/enabled", "Enable mask on USK <key>", "bool", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/mask/top", "Set Mask Top (-9.0 to 9.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/mask/bottom", "Set Mask Bottom (-9.0 to 9.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/mask/left", "Set Mask Left (-16.0 to 16.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/mask/right", "Set Mask Right (-16.0 to 16.0)", "float", "Upstream Keyers", true);
    registerEndpoint("/me/<me>/usk/<key>/mask/reset", "Reset Mask to default zero values", "none", "Upstream Keyers", false);

    // DSK (Downstream Keyers)
    registerEndpoint("/dsk/<key>/tie", "Set DSK <key> Tie in next transition", "bool", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/tie/toggle", "Toggle DSK <key> Tie", "none", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/on-air", "Set DSK <key> On Air", "bool", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/on-air/toggle", "Toggle DSK <key> On Air", "none", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/on-air/auto", "Auto transition DSK <key>", "none", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/rate", "Set DSK <key> transition rate in frames", "int", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/source/fill", "Set Fill input for DSK <key>", "int", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/source/cut", "Set Cut/Key input for DSK <key>", "int", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/clip", "Set DSK <key> clip level", "float", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/gain", "Set DSK <key> gain level", "float", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/inverse", "Set DSK <key> inverse parameter", "bool", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/pre-multiplied", "Set DSK <key> pre-multiplied parameter", "bool", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/mask/enabled", "Enable mask on DSK <key>", "bool", "Downstream Keyers", true);
    registerEndpoint("/dsk/<key>/mask/reset", "Reset mask on DSK <key>", "none", "Downstream Keyers", false);

    // SuperSource
    registerEndpoint("/supersource/box/<key>/enabled", "Enable or disable SuperSource Box <key>", "bool", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/source", "Set SuperSource Box <key> input source", "int", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/x", "Set SuperSource Box <key> X position", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/y", "Set SuperSource Box <key> Y position", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/size", "Set SuperSource Box <key> size scale", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/cropped", "Enable crop on Box <key>", "bool", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/crop-top", "Set Box <key> Top crop", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/crop-bottom", "Set Box <key> Bottom crop", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/crop-left", "Set Box <key> Left crop", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/crop-right", "Set Box <key> Right crop", "float", "SuperSource", true);
    registerEndpoint("/supersource/box/<key>/crop-reset", "Reset Box <key> crop to full", "none", "SuperSource", false);
    registerEndpoint("/supersource/border/enabled", "Enable SuperSource Border", "bool", "SuperSource", true);
    registerEndpoint("/supersource/border/width-in", "Set Border Inner Width", "float", "SuperSource", true);
    registerEndpoint("/supersource/border/width-out", "Set Border Outer Width", "float", "SuperSource", true);

    // Audio & Fairlight Audio
    registerEndpoint("/audio/input/<number>/gain", "Set Audio channel fader gain in dB", "float", "Audio", true);
    registerEndpoint("/audio/input/<number>/balance", "Set Audio channel balance / pan (-1.0 to 1.0)", "float", "Audio", true);
    registerEndpoint("/audio/input/<number>/mix", "Set Audio mix option (afv, on, off)", "string", "Audio", true);
    registerEndpoint("/audio/input/<number>/solo", "Set Fairlight Audio Solo monitoring", "bool", "Audio", true);
    registerEndpoint("/audio/input/<number>/preamp", "Set Analog / XLR preamp gain in dB", "float", "Audio", true);
    registerEndpoint("/audio/input/<number>/phantom", "Enable +48V Phantom Power on XLR input", "bool", "Audio", true);
    registerEndpoint("/audio/output/gain", "Set Master Program Out audio gain in dB", "float", "Audio", true);

    // Fairlight EQ & Dynamics (SDK 11.0)
    registerEndpoint("/audio/input/<number>/eq/<band>/gain", "Set Fairlight EQ Band gain in dB (-20 to +20)", "float", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/eq/<band>/freq", "Set Fairlight EQ Band center frequency in Hz", "float", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/eq/<band>/q", "Set Fairlight EQ Band Q factor", "float", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/eq/<band>/enabled", "Enable or bypass Fairlight EQ Band", "bool", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/eq/reset", "Reset Fairlight EQ for channel", "none", "Audio Fairlight", false);
    registerEndpoint("/audio/input/<number>/compressor/threshold", "Set Fairlight Compressor threshold in dB", "float", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/compressor/ratio", "Set Fairlight Compressor ratio", "float", "Audio Fairlight", true);
    registerEndpoint("/audio/input/<number>/limiter/threshold", "Set Fairlight Limiter threshold in dB", "float", "Audio Fairlight", true);

    // MultiView (SDK 11.0)
    registerEndpoint("/multiview/<mv>/layout", "Set MultiView layout (default, quadrant, 1+7, 1+9, 2+8)", "string", "MultiView", true);
    registerEndpoint("/multiview/<mv>/window/<win>/source", "Set MultiView Window source input ID", "int", "MultiView", true);
    registerEndpoint("/multiview/<mv>/safe-area", "Set Safe Area display in MultiView", "bool", "MultiView", true);
    registerEndpoint("/multiview/<mv>/meters", "Set VU Audio Meters display in MultiView", "bool", "MultiView", true);

    // Display Clock (SDK 11.0)
    registerEndpoint("/clock/enabled", "Enable or hide on-screen Display Clock", "bool", "Display Clock", true);
    registerEndpoint("/clock/mode", "Set Clock mode (timeofday, countdown, countup)", "string", "Display Clock", true);
    registerEndpoint("/clock/start", "Start countdown or stopwatch clock", "none", "Display Clock", true);
    registerEndpoint("/clock/stop", "Stop countdown or stopwatch clock", "none", "Display Clock", true);
    registerEndpoint("/clock/reset", "Reset countdown or stopwatch clock", "none", "Display Clock", true);
    registerEndpoint("/clock/time", "Set target clock time in HH:MM:SS format", "string", "Display Clock", true);
    registerEndpoint("/clock/size", "Set Clock display size scale (0.5 to 2.0)", "float", "Display Clock", true);
    registerEndpoint("/clock/opacity", "Set Clock display opacity (0.0 to 1.0)", "float", "Display Clock", true);

    // Recording & Streaming
    registerEndpoint("/recording/start", "Start recording to external media disk", "none", "Recording", true);
    registerEndpoint("/recording/stop", "Stop recording to external media disk", "none", "Recording", true);
    registerEndpoint("/recording/switch-disk", "Switch active recording disk", "none", "Recording", true);
    registerEndpoint("/recording/filename", "Set base filename for recording", "string", "Recording", true);
    registerEndpoint("/recording/iso", "Enable ISO recording of all individual inputs", "bool", "Recording", true);
    registerEndpoint("/stream/start", "Start RTMP live streaming", "none", "Streaming", true);
    registerEndpoint("/stream/stop", "Stop RTMP live streaming", "none", "Streaming", true);
    registerEndpoint("/stream/service", "Select streaming service profile", "string", "Streaming", true);

    // Camera Control & PTZ (SDK 11.0)
    registerEndpoint("/camera/<id>/iris", "Set Camera lens iris (0.0 to 1.0)", "float", "Camera PTZ", true);
    registerEndpoint("/camera/<id>/focus", "Set Camera lens manual focus (0.0 to 1.0)", "float", "Camera PTZ", true);
    registerEndpoint("/camera/<id>/autofocus", "Trigger one-shot autofocus on camera", "none", "Camera PTZ", false);
    registerEndpoint("/camera/<id>/gain", "Set Camera gain in dB (integer steps)", "float", "Camera PTZ", true);
    registerEndpoint("/camera/<id>/shutter", "Set Camera shutter speed as 1/x s (60 = 1/60)", "float", "Camera PTZ", true);
    registerEndpoint("/camera/<id>/white-balance", "Set Camera color temperature in Kelvin", "float", "Camera PTZ", true);
    registerEndpoint("/camera/<id>/ptz/pan", "PTZ pan speed, -1.0 to 1.0 (0 stops)", "float", "Camera PTZ", false);
    registerEndpoint("/camera/<id>/ptz/tilt", "PTZ tilt speed, -1.0 to 1.0 (0 stops)", "float", "Camera PTZ", false);
    registerEndpoint("/camera/<id>/ptz/zoom", "PTZ zoom speed, -1.0 to 1.0 (0 stops)", "float", "Camera PTZ", false);
    registerEndpoint("/camera/<id>/ptz/recall", "Recall PTZ camera preset index", "int", "Camera PTZ", false);

    // Macros
    registerEndpoint("/macros/stop", "Stop currently running macro", "none", "Macros", true);
    registerEndpoint("/macros/max-number", "Query maximum supported macros count", "none", "Macros", false);
    registerEndpoint("/macros/<index>/run", "Run macro at <index>", "none", "Macros", true);
    registerEndpoint("/macros/<index>/loop", "Set whether macro loops upon completion", "bool", "Macros", true);
    registerEndpoint("/macros/<index>/name", "Query name of macro at <index>", "none", "Macros", false);
    registerEndpoint("/macros/<index>/description", "Query description of macro at <index>", "none", "Macros", false);
    registerEndpoint("/macros/<index>/is-valid", "Query validity status of macro at <index>", "none", "Macros", false);

    // HyperDecks
    registerEndpoint("/hyperdeck/<number>/play", "Start HyperDeck playback", "none", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/stop", "Stop HyperDeck playback/recording", "none", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/record", "Start HyperDeck recording", "none", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/shuttle", "Shuttle HyperDeck at speed percent", "int", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/jog", "Jog HyperDeck by frame offset", "int", "HyperDeck", false);
    registerEndpoint("/hyperdeck/<number>/clip", "Select HyperDeck clip index", "int", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/clip-time", "Cue HyperDeck to clip timecode (HH:MM:SS:FF)", "string", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/single-clip", "Set HyperDeck single clip playback mode", "bool", "HyperDeck", true);
    registerEndpoint("/hyperdeck/<number>/loop", "Set HyperDeck looped playback mode", "bool", "HyperDeck", true);

    // Media Players & Stills
    registerEndpoint("/mplayer/<player>/clip", "Select clip index on Media Player <player>", "int", "Media Player", true);
    registerEndpoint("/mplayer/<player>/still", "Select still index on Media Player <player>", "int", "Media Player", true);
    registerEndpoint("/mplayer/capture", "Capture live still to Media Pool from input ID", "int", "Media Player", false);

    // Additional addresses (original atemOSC scheme and extras)
    registerEndpoint("/me/<me>/transition/bar/reset", "Reset T-bar to the transition start position", "none", "Transitions", false);
    registerEndpoint("/me/<me>/usk/<key>/tie/set-next", "Set whether USK <key> is on air after the next transition", "bool", "Upstream Keyers", false);
    registerEndpoint("/me/<me>/usk/<key>/fly/enable", "Enable flying key on USK <key>", "bool", "Upstream Keyers", false);
    registerEndpoint("/me/<me>/usk/<key>/fly/clear", "Full screen, no rotation for USK <key> fly key", "none", "Upstream Keyers", false);
    registerEndpoint("/me/<me>/usk/<key>/chroma/narrow", "Narrow chroma key range on USK <key>", "bool", "Upstream Keyers", false);
    registerEndpoint("/dsk/<key>/tie/set-next", "Set whether DSK <key> is on air after the next transition", "bool", "Downstream Keyers", false);
    registerEndpoint("/dsk/<key>/mask/top", "Top mask of DSK <key>", "float", "Downstream Keyers", false);
    registerEndpoint("/dsk/<key>/mask/bottom", "Bottom mask of DSK <key>", "float", "Downstream Keyers", false);
    registerEndpoint("/dsk/<key>/mask/left", "Left mask of DSK <key>", "float", "Downstream Keyers", false);
    registerEndpoint("/dsk/<key>/mask/right", "Right mask of DSK <key>", "float", "Downstream Keyers", false);
    registerEndpoint("/supersource/border/softness-in", "SuperSource border inner softness", "float", "SuperSource", false);
    registerEndpoint("/supersource/border/softness-out", "SuperSource border outer softness", "float", "SuperSource", false);
    registerEndpoint("/supersource/border/hue", "SuperSource border hue", "float", "SuperSource", false);
    registerEndpoint("/supersource/border/saturation", "SuperSource border saturation", "float", "SuperSource", false);
    registerEndpoint("/supersource/border/luma", "SuperSource border luma", "float", "SuperSource", false);
    registerEndpoint("/audio/input/<number>/mic-level", "Mic level (true) or line level (false) on analog input", "bool", "Audio", false);
    registerEndpoint("/audio/input/<number>/low-cut", "Enable low cut filter on analog input", "bool", "Audio", false);
    registerEndpoint("/audio/input/<number>/compressor/enabled", "Enable compressor", "bool", "Audio", false);
    registerEndpoint("/audio/input/<number>/compressor/attack", "Compressor attack in ms", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/compressor/release", "Compressor release in ms", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/limiter/enabled", "Enable limiter", "bool", "Audio", false);
    registerEndpoint("/audio/input/<number>/limiter/attack", "Limiter attack in ms", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/limiter/release", "Limiter release in ms", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/expander/enabled", "Enable expander / gate", "bool", "Audio", false);
    registerEndpoint("/audio/input/<number>/expander/threshold", "Expander threshold in dB", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/expander/range", "Expander range in dB", "float", "Audio", false);
    registerEndpoint("/audio/input/<number>/expander/ratio", "Expander ratio", "float", "Audio", false);
    registerEndpoint("/clock/x", "Display clock horizontal position", "float", "Display Clock", false);
    registerEndpoint("/clock/y", "Display clock vertical position", "float", "Display Clock", false);
    registerEndpoint("/stream/url", "Streaming server URL", "string", "Streaming", false);
    registerEndpoint("/stream/key", "Streaming key", "string", "Streaming", false);
    registerEndpoint("/recording/state", "Recording state feedback (idle | recording)", "string", "Recording", true);
    registerEndpoint("/stream/state", "Streaming state feedback (idle | streaming)", "string", "Streaming", true);
    registerEndpoint("/hyperdeck/<number>/timeline-time", "Jump to timeline time on HyperDeck", "string", "HyperDeck", false);
    registerEndpoint("/led/green", "Connection LED: 1.0 when connected", "float", "System", true);
    registerEndpoint("/led/red", "Connection LED: 1.0 when disconnected", "float", "System", true);

    // Aux & Converted Video Mode
    registerEndpoint("/aux/<number>", "Set input source for Aux output <number>", "int", "Aux", true);
    registerEndpoint("/aux/<number>/converted-videomode", "Set converted video mode on Aux output (SDK 11.0)", "int", "Aux", true);
}

} // namespace Osc
