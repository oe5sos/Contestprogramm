#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace Contestprogramm {

class ChatFeedModel;
class MergedChatModel;

// Der Chat: was auf ON4KST geschrieben wird und was der DX-Cluster
// meldet, in einer Liste, neueste Zeile unten.
//
// Bis 2026-09-28 standen diese Zeilen unter einer Trennlinie in der
// Log-Liste. Das war bei leerem Log unlesbar (die halbleeren Zeilen
// sahen aus wie kaputte QSOs, Martin mit Bild: "nach neuem log beginnen
// erscheint dies"), und N1MM wie DXLog.net halten ihr Log-Fenster
// ebenfalls frei davon. Beim Herausnehmen ist mir allerdings entgangen,
// dass es SONST keinen Ort für den Chatverlauf gab -- man konnte senden
// und bekam die Antwort nicht zu sehen. Dieses Panel ist dieser Ort.
//
// Was hier steht und was nicht: jede Zeile, die hereinkommt, mit
// Uhrzeit, Quelle (KST/CLU), Rufzeichen und dem Text, wie er gesendet
// wurde. Die verwertbaren Angaben daneben -- Entfernung und Peilung,
// sofern ein Locator in der Zeile stand. Schon gearbeitete Stationen
// stehen gedämpft, damit das Auge an den offenen hängenbleibt.
class ChatPanelWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatPanelWidget(QWidget* parent = nullptr);

    // Die beiden Quellen. Non-owning, wie überall in diesem Programm.
    void setFeedModels(ChatFeedModel* onKst, ChatFeedModel* cluster);

    // Die Tabelle und das Eingabefeld für Prüfstände.
    static constexpr const char* kTableObjectName = "chatPanelTable";
    static constexpr const char* kInputObjectName = "chatPanelInput";
    static constexpr const char* kSendButtonObjectName = "chatPanelSend";
    static constexpr const char* kStatusObjectName = "chatPanelStatus";

    enum Column { ColumnTime = 0, ColumnSource, ColumnCall, ColumnDistance, ColumnText, ColumnCount };

    // Was in der Kopfzeile des Panels steht ("ON4KST: verbunden" o. ä.).
    void setConnectionStatus(const QString& text);

    // Alle Zeilen zeigen statt nur der gefilterten. Standard ist
    // gefiltert -- Martin, 2026-09-28: "alles was mich nicht erreicht
    // bzw. was absolut nicht funktionieren kann möchte ich gefiltert
    // haben um nicht 1000 unnötige chat zu sehen." Der Schalter sitzt
    // im ⚙ des Panelkopfes.
    void setShowAll(bool showAll);
    bool showsAll() const;

    // Das eigene Rufzeichen. Zeilen, in denen es vorkommt, stehen in
    // Magenta -- Martin, 2026-09-28: "was im chat mich betrifft soll in
    // magenta gekennzeichnet werden." Leer heißt: nichts hervorheben.
    void setOwnCallsign(const QString& callsign);
    QString ownCallsign() const;

signals:
    // Der Bediener will diese Station anrufen -- Doppelklick auf eine
    // Zeile. Dieselbe Form, die MainWindow::handleCandidateActivated
    // schon von der Karte kennt.
    void candidateActivated(const QString& callsign, const QString& grid, qint64 freqHz);
    // Eine Nachricht soll in den ON4KST-Raum.
    void messageSubmitted(const QString& text);

private:
    void sendCurrentInput();
    void updateStatusLine();

    MergedChatModel* m_model = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_sendButton = nullptr;
    QLabel* m_status = nullptr;
    QString m_connectionText;
};

} // namespace Contestprogramm
