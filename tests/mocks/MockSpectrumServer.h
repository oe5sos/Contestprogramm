#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QWebSocket;
class QWebSocketServer;

namespace Contestprogramm {

// Longpath-Doppel für tests/test_wasserfall_leitung.cpp.
//
// Hört auf einem flüchtigen Port auf der Rückschleife (nie auf einer
// echten Adresse -- siehe tests/CMakeLists.txt und die Hausregel, dass
// kein Prüflauf ins Netz greift), nimmt einen Client an, merkt sich
// jede Textzeile, die er schickt, und spielt auf Abruf Binärrahmen im
// Format von Longpaths SpectrumStream zurück.
//
// Bewusst KEIN TCI-Server: keine Begrüßung, keine Befehle außer der
// Buchführung über spectrum_start/spectrum_stop, keine Geräte. Dieses
// Doppel prüft die LEITUNG -- dass ein Rahmen, der wirklich über einen
// WebSocket kommt, am anderen Ende als Spektrum ankommt, und dass ein
// verdorbener Rahmen auch dort niemanden umwirft. Gegen das echte
// Longpath prüft der Betreiber selbst, mit einem Gerät daran.
class MockSpectrumServer : public QObject {
    Q_OBJECT

public:
    explicit MockSpectrumServer(QObject* parent = nullptr);
    ~MockSpectrumServer() override;

    bool startListening();
    quint16 port() const;

    bool hasClient() const { return m_client != nullptr; }

    // Jede Textzeile, die der Client geschickt hat, in der Reihenfolge
    // des Eingangs -- der Prüfstand liest daran ab, ob er wirklich nur
    // spectrum_start/spectrum_stop sendet und nichts, was ein Gerät
    // bedienen würde.
    QStringList empfangeneBefehle() const { return m_befehle; }

    // Ein Bild mit `punkte` Werten schicken, alle auf `fuellwert`.
    void sendeSpektrum(int punkte, quint8 fuellwert);

    // Dasselbe, aber die Punktzahl im Kopf weicht von der Nutzlast ab
    // -- der Rahmen, über den ein ungeprüfter Leser hinausliest.
    void sendeVerdorbenenRahmen(int punkteImKopf, int punkteWirklich);

    // Etwas, das gar nicht zu uns gehört: anderer Stromtyp.
    void sendeFremdenStrom();

    void trenne();

private:
    void aufNeuerClient();

    QWebSocketServer* m_server = nullptr;
    QWebSocket*       m_client = nullptr;
    QStringList       m_befehle;
};

} // namespace Contestprogramm
