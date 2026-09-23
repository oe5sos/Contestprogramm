#include "ui/ShortcutsWindow.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
enum Column { ColKeys = 0, ColEffect, ColWhere, ColCount };
} // namespace

QVector<ShortcutsWindow::Entry> ShortcutsWindow::entries()
{
    return {
        // The entry row
        {QStringLiteral("Enter"), QStringLiteral("QSO loggen — mit ESM: sendet, was der QSO-Stand verlangt, und loggt am Ende"), QStringLiteral("Log-Eingabezeile")},
        {QStringLiteral("Frequenz + Enter"),
         QStringLiteral("Statt eines Rufzeichens eine Zahl: QSY und Bandwechsel. „14045“ sind Kilohertz, "
                        "„14.045“ Megahertz — ohne CAT der einzige Weg auf ein anderes Band"),
         QStringLiteral("Log-Eingabezeile")},
        {QStringLiteral("Leertaste"), QStringLiteral("Zum nächsten Feld (Rufzeichen → Nr. → Locator …), Inhalt markiert"), QStringLiteral("Log-Eingabezeile")},
        {QStringLiteral("Tab"), QStringLiteral("Zum nächsten Feld, vom letzten zurück zum Rufzeichen"), QStringLiteral("Log-Eingabezeile")},
        {QStringLiteral("Alt+W"), QStringLiteral("Eingabezeile leeren (Wipe)"), QStringLiteral("überall")},
        {QStringLiteral("Tippen"), QStringLiteral("Check Partial schlägt Rufzeichen und Locator vor; Klick übernimmt"), QStringLiteral("Panel „Check“")},
        // CW
        {QStringLiteral("F1 … F6"), QStringLiteral("CW-Makro 1 … 6 senden (F1 CQ, F2 Call + Exchange, F3 Exchange, F4 TU, F5 ?), auch bei ausgeblendeter Makro-Zeile"), QStringLiteral("überall")},
        {QStringLiteral("Esc"), QStringLiteral("CW-Sendung stoppen"), QStringLiteral("überall")},
        {QStringLiteral("Bild ↑ / Bild ↓"), QStringLiteral("CW-Tempo +2 / −2 WpM (geht als KEYSPD ans Funkgerät)"), QStringLiteral("überall")},
        // Corrections
        {QStringLiteral("Doppelklick / Enter"), QStringLiteral("Eintrag korrigieren: Rufzeichen, Nr./Locator, Zeit — Dupes werden danach neu berechnet"), QStringLiteral("Log-Historie")},
        {QStringLiteral("Klick auf die Status-Spalte"), QStringLiteral("QSO „ungültig“ markieren statt löschen (und wieder gültig)"), QStringLiteral("Log-Historie")},
        // Targets
        {QStringLiteral("Klick auf Station"), QStringLiteral("QSY aufs Band/die Frequenz, Rotor dreht hin, Rufzeichen in die Eingabezeile"), QStringLiteral("Karte, Bandmap, Skeds, Kandidaten")},
        {QStringLiteral("Klick auf „Offen in Richtung“"), QStringLiteral("Die nächste offene Station im Beam anfunken (weiteste zuerst, reihum)"), QStringLiteral("Radar, Zahlenspalte")},
        {QStringLiteral("Zahl + Enter im Feld „Ziel“"), QStringLiteral("Rotor auf diese Richtung drehen (0–360°)"), QStringLiteral("Rotor-Skala")},
        {QStringLiteral("Doppelklick auf die Skala"), QStringLiteral("Rotor auf die angeklickte Richtung drehen"), QStringLiteral("Rotor-Skala")},
        {QStringLiteral("⚙ rechts oben"), QStringLiteral("Optionen des Panels (Karte: Ebenen, Antennen, Öffnungswinkel; Rotor: Skalenstil)"), QStringLiteral("jeder Panelkopf")},
        // Panels
        {QStringLiteral("Kopfzeile ziehen"), QStringLiteral("Panel verschieben; Griff rechts unten: Größe; Schloss: festhalten"), QStringLiteral("jedes Panel")},
        {QStringLiteral("1 / 2 / 3 links"), QStringLiteral("Layout-Profil wechseln; + legt ein neues an"), QStringLiteral("Profil-Leiste")},
        {QStringLiteral("Fenster › Panels"), QStringLiteral("Ausgeblendete Panele (Check, Skeds, Bandmap …) einblenden; Fenster zurücksetzen: Standardanordnung"), QStringLiteral("Menü")},
        // Before and after
        {QStringLiteral("Datei › Startcheck"), QStringLiteral("Alles vor dem ersten CQ: Station, Uhr, Contest, Verbindungen, Sicherung"), QStringLiteral("Menü")},
        {QStringLiteral("Datei › Log prüfen"), QStringLiteral("Was der Auswerter beanstanden würde, vor der Abgabe"), QStringLiteral("Menü")},
        {QStringLiteral("Datei › EDI exportieren"), QStringLiteral("Abgabedatei je Band (REG1TEST)"), QStringLiteral("Menü")},
    };
}

ShortcutsWindow::ShortcutsWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Contestprogramm - Tastenkürzel"));
    m_header = new PanelHeaderBar(QStringLiteral("Tastenkürzel und Griffe"), this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Taste / Griff"), QStringLiteral("Wirkung"), QStringLiteral("Wo")});
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColEffect, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setColumnWidth(ColKeys, 230);
    m_table->setColumnWidth(ColWhere, 220);

    const QVector<Entry> all = entries();
    m_table->setRowCount(all.size());
    for (int row = 0; row < all.size(); ++row) {
        auto* keys = new QTableWidgetItem(all.at(row).keys);
        keys->setForeground(QColor(Style::kAmberText()));
        m_table->setItem(row, ColKeys, keys);
        auto* effect = new QTableWidgetItem(all.at(row).effect);
        effect->setToolTip(all.at(row).effect);
        m_table->setItem(row, ColEffect, effect);
        auto* where = new QTableWidgetItem(all.at(row).where);
        where->setForeground(QColor(Style::kTextSecondary()));
        m_table->setItem(row, ColWhere, where);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(10, 10, 10, 10);
    body->addWidget(m_table, 1);
    layout->addLayout(body, 1);

    resize(1000, 560);
}

int ShortcutsWindow::rowCount() const
{
    return m_table->rowCount();
}

QString ShortcutsWindow::rowText(int row, int column) const
{
    const QTableWidgetItem* item = m_table->item(row, column);
    return item ? item->text() : QString();
}

} // namespace Contestprogramm
