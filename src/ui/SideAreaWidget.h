#pragma once

#include <QStringList>
#include <QWidget>

class QStackedWidget;
class QVBoxLayout;

namespace Contestprogramm {

// Der Seitenbereich: eine schmale Leiste, dahinter teilen sich mehrere
// Panels ein Fenster. Klick auf ein inaktives Symbol zeigt dessen Panel,
// Klick auf das aktive klappt den Bereich zu -- dann bleibt nur die
// Leiste stehen.
//
// Martin, 2026-09-28: "ich möchte wie bei longpath eine leiste haben, wo
// mehrere fenster untergebracht sind, welches ich mit klicken öffne" und
// "mehrere widget in einem fenster".
//
// Dieselbe Bedienung wie Longpaths SideAreaWindow (Zweig
// feature/seitenbereich, dort aus drei Entwürfen gewählte Variante 2:
// "Symbolleiste am Rand, Klick aufs aktive Symbol klappt zu"). Die
// Mechanik dahinter ist hier eine andere und viel kleinere: Longpath
// verschiebt Applets zwischen Spalte, schwebenden Fenstern und Bereich
// und muss dabei QRhi-Flächen aussparen. Hier wandert schlicht ein
// PanelContainerWidget in einen QStackedWidget und wieder heraus.
//
// Wozu das gut ist: auf dem 13"-Layout ist die Fläche voll (Rotoren,
// Karte, Nächstes Ziel, Rate, Log, Bandmap teilen sich 1372x793 ohne
// Lücke). Wer Chat, Skeds und Check selten gleichzeitig braucht, legt
// sie hierher und klickt um, statt sie nebeneinander zu quetschen.
class SideAreaWidget : public QWidget {
    Q_OBJECT

public:
    explicit SideAreaWidget(QWidget* parent = nullptr);

    // Ein Panel hineinlegen. Der Bereich übernimmt es (setParent), der
    // Aufrufer bleibt für seine Lebensdauer zuständig.
    void addPage(const QString& id, const QString& title, QWidget* content);
    // Wieder herausnehmen -- gibt das Widget zurück, ohne Elternteil.
    QWidget* takePage(const QString& id);
    bool hasPage(const QString& id) const { return m_order.contains(id); }
    QStringList pageIds() const { return m_order; }

    QString activeId() const { return m_active; }
    // Zeigt die Seite; klappt einen zugeklappten Bereich auf.
    void setActive(const QString& id);

    bool isCollapsed() const { return m_collapsed; }
    // Zu: nur die Leiste bleibt stehen. Auf: wieder die volle Breite.
    void setCollapsed(bool collapsed);

    // Der Klick auf ein Leistensymbol, wie ihn der Bediener macht:
    // inaktiv → zeigen, aktiv → zu- bzw. aufklappen. Genau Longpaths
    // railClicked().
    void railClicked(const QString& id);

    // Die Breite der Leiste. Sie trägt Symbol UND Namen -- Martin hat
    // am 2026-09-28 aus drei Blättern C gewählt ("Symbol und Name",
    // 150 px) statt der schmalen Fassungen A (38) und B (44). Die Zahl
    // steht an einer Stelle, weil der eingeklappte Bereich genau so
    // breit ist.
    static constexpr int kRailWidth = 150;

    static constexpr const char* kRailObjectName = "sideAreaRail";
    static constexpr const char* kStackObjectName = "sideAreaStack";

signals:
    void activeChanged(const QString& id);
    void collapsedChanged(bool collapsed);
    // Rechtsklick auf ein Leistensymbol: "Aus dem Seitenbereich nehmen".
    void removeRequested(const QString& id);
    // Der Knopf wurde aus der Leiste herausgezogen und an dieser Stelle
    // losgelassen (Bildschirmkoordinaten). Martin, 2026-09-28: "die
    // widgets sollte man aber auch wieder per drag and drop rausziehen
    // können, in dem fall nach rechts."
    void pageDraggedOut(const QString& id, const QPoint& globalPos);

private:
    // Die Knöpfe NEU BAUEN -- nur, wenn sich die Liste der Seiten
    // ändert. Niemals aus einem Klick-Handler heraus: der Knopf, der
    // gerade geklickt wird, würde dabei gelöscht, und Qt arbeitet
    // danach auf einem toten Objekt weiter. Genau dieser Fehler ist mir
    // am selben Tag schon beim Bandmenü passiert (rebuildBandModeControls
    // vs. syncBandModeControls in MainWindow) -- und hier live wieder:
    // ein Klick auf "BA" hob den Knopf hervor, zeigte aber weiter die
    // Skeds.
    void rebuildRail();
    // Nur den Zustand nachziehen (welcher Knopf gedrückt aussieht).
    // Das ist der Weg, den ein Klick nimmt.
    void updateRailState();

    QWidget* m_rail = nullptr;
    QVBoxLayout* m_railLayout = nullptr;
    QStackedWidget* m_stack = nullptr;
    QStringList m_order;
    QHash<QString, QString> m_titles;
    QString m_active;
    bool m_collapsed = false;
    int m_expandedWidth = 320;
};

} // namespace Contestprogramm
