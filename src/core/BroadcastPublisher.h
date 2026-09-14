#pragma once

// =================================================================
// src/core/BroadcastPublisher.h  (Contestprogramm)
// =================================================================
//
// Sends UDP datagrams in N1MM Logger+'s documented "External UDP
// Messages" XML wire format -- verified against the authoritative
// N1MM Logger+ documentation (https://n1mmwp.hamdocs.com/appendices/
// external-udp-broadcasts/, fetched and read in full 2026-09-09), not
// guessed:
//   - Default port 12060; the destination is a specifically
//     configured IP:port the operator enters (optionally a subnet
//     broadcast address like x.x.x.255) -- NOT an automatic subnet
//     broadcast N1MM sends on its own. See that page's own "Setting
//     the Destination IP Addresses"/"...Port Numbers" sections.
//   - <contactinfo> is sent once per logged QSO ("after a new record
//     has been added to a contest log"); field names/order copied
//     from that page's own XML example.
//   - <RadioInfo> is sent on CAT frequency/mode change and otherwise
//     periodically -- N1MM's own documented cadence is "at least
//     every 10 seconds, or when radio frequency or mode change"; field
//     names/order copied from that page's own XML example.
//
// This is the same wire format DXLog.net implements under its own
// "Use N1MM QSO format" broadcast option (dxlog.net/docs/index.php/
// Additional_Information, fetched 2026-09-09): same default port
// 12060, explicitly documented there as the same <contactinfo> tag
// set, with DXLog.net's own docs listing which N1MM-specific fields it
// always leaves empty. This class follows the same "leave what this
// program has no real value for empty, never fabricate one" discipline
// for the same reason -- e.g. dbname/contestnr/operator/countryprefix/
// wpxprefix/stationprefix/continent (station-lookup data this project
// does not compute), section/comment/qth/name/power/misctext/zone/
// prec/ck (contest-specific exchange sub-fields the two shipped
// ContestDefinitions do not use), points/ismultiplier1/ismultiplier2/
// ismultiplier3 (no scoring engine here), run1run2/RadioInterfaced/
// NetworkedCompNr/IsRunQSO (multi-radio/multi-op state that does not
// exist in this single-operator, single-radio program), and
// oldtimestamp/oldcall (only meaningful in a <contactreplace>, which
// this class never sends -- QSOs are never edited/deleted here).
//
// Per the plan's "UDP-Contact-Broadcast-Standard" section, N1MM+,
// DXLog.net and QARTest are all cited as implementing this one
// broadcast standard, with AirScout/KST4Contest hooking into it rather
// than each contest logger building a bespoke integration. This
// class's own research independently confirmed N1MM+'s side (above,
// primary documentation) and DXLog.net's side (above, DXLog.net's own
// docs); QARTest's exact wire format was not independently located
// during this pass -- secondary sources describe it as part of the
// same ecosystem, but no primary QARTest documentation confirming its
// schema was found. Interop is therefore verified with N1MM+ itself
// and, per DXLog.net's own documented field subset, with DXLog.net;
// AirScout/KST4Contest/QARTest are reported elsewhere to ride the same
// standard, but that third leg is not independently re-verified here.
//
// Off by default (ContestSettings::broadcastEnabled == false): this
// puts data on the local network, so it must not switch itself on for
// an operator who never asked for it.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created. AI-assisted via Anthropic Claude Code,
//                 operator Ralph Martin Fischer.
// =================================================================

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>

class QUdpSocket;

namespace Contestprogramm {

struct QsoRecord;

class BroadcastPublisher : public QObject {
    Q_OBJECT

public:
    explicit BroadcastPublisher(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // `host` is a literal IP (dotted-quad, including a subnet
    // broadcast address like 192.168.1.255) -- same as N1MM's own
    // Destination-IP field, which is never a hostname either. An
    // unparsable host clears the target, the same "no attempt with a
    // placeholder/empty setting" posture AppController already applies
    // to the other network clients.
    void setTarget(const QString& host, quint16 port);
    bool hasTarget() const { return m_hasTarget; }

    // Sends one <contactinfo> UDP datagram for `record`, if enabled
    // and a target is set. `ownCallsign`/`contestName` come from
    // ContestSettings/ContestDefinition -- QsoRecord itself carries
    // neither.
    void publishContact(const QsoRecord& record, const QString& ownCallsign, const QString& contestName);

    // Sends one <RadioInfo> UDP datagram reflecting the given CAT
    // state, if enabled, a target is set, and `ownCallsign` is not
    // empty (no station callsign configured yet means nothing useful
    // to report).
    void publishRadioInfo(const QString& ownCallsign, qint64 frequencyHz, const QString& mode, bool transmitting);

    // Pure builders, exposed so tests/test_broadcastpublisher.cpp can
    // check the XML shape without a live socket.
    static QByteArray buildContactInfoXml(const QsoRecord& record, const QString& ownCallsign, const QString& contestName);
    static QByteArray buildRadioInfoXml(const QString& ownCallsign, qint64 frequencyHz, const QString& mode, bool transmitting);

    // "CW"/"FM"/"RTTY" pass through unchanged (already valid N1MM Mode
    // values, per that page's RadioInfo field notes); "SSB" maps to
    // "USB" -- N1MM's own Mode enumeration has no plain "SSB" value,
    // and 2m/70cm SSB is conventionally upper sideband, so this is a
    // documented-convention mapping, not a guess.
    static QString n1mmModeCode(const QString& mode);

private:
    void send(const QByteArray& xml);

    QUdpSocket* m_socket;
    bool m_enabled = false;
    QHostAddress m_targetHost;
    quint16 m_targetPort = 12060;
    bool m_hasTarget = false;
};

} // namespace Contestprogramm
