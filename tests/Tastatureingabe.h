#pragma once

// Tastatureingabe in Prüfständen -- das Ziel wird am FENSTER erfragt,
// nicht an der Anwendung.
//
// Am 2026-10-20 fielen im vollen Prüflauf sieben Prüfstände aus, die
// einzeln alle grün sind; zwei davon mit SIGABRT. Im Protokoll stand
// jedes Mal:
//
//     QFATAL : ASSERT: "widget" in qtestkeyboard.h, line 198
//
// Das ist QTest::keyClicks(nullptr, ...). Der Nullzeiger kam von
// `QApplication::focusWidget()`: das liefert das Fokus-Widget der
// AKTIVEN Anwendung und ist nullptr, sobald das Fenster des Prüfstands
// den Tastaturfokus nicht bekommen hat. Einzeln bekommt es ihn (nichts
// anderes läuft), im vollen Lauf hinter 40 anderen Fenstern nicht
// zuverlässig -- in der CI, wo kein Mensch am Rechner sitzt, fiel es nie
// auf.
//
// `QWidget::focusWidget()` kennt dagegen den Fokus INNERHALB seines
// Fensters, ganz gleich ob das Fenster aktiviert wurde. Dazu eine kurze
// Frist, weil der Fokus nach einem Tab erst im nächsten Umlauf steht.
//
// Siehe tests/test_rate_sofort.cpp und test_vorgangsprotokoll.cpp.

#include <QElapsedTimer>
#include <QWidget>
#include <QtTest>

namespace Contestprogramm::Pruefstand {

// Das Widget mit dem Tastaturfokus in diesem Fenster, oder nullptr,
// wenn innerhalb der Frist keines eines bekommt. Der Aufrufer prüft das
// mit QVERIFY -- eine klare Meldung ist besser als ein Abbruch im
// Qt-Innern.
inline QWidget* fokusIn(QWidget* fenster, int fristMs = 5000)
{
    if (!fenster) {
        return nullptr;
    }
    QElapsedTimer uhr;
    uhr.start();
    do {
        if (QWidget* ziel = fenster->focusWidget()) {
            return ziel;
        }
        QTest::qWait(10);
    } while (uhr.elapsed() < fristMs);
    return fenster->focusWidget();
}

// Tippt in das Widget, das in diesem Fenster gerade den Fokus hat.
// Nach jedem Tab wandert der Fokus weiter, deshalb wird er vor jedem
// Schritt neu erfragt statt einmal gemerkt.
inline void tippeInFokus(QWidget* fenster, const QString& text)
{
    QWidget* ziel = fokusIn(fenster);
    QVERIFY2(ziel, qPrintable(QStringLiteral("kein Fokus im Fenster -- \"%1\" ging ins Leere").arg(text)));
    QTest::keyClicks(ziel, text);
}

inline void tasteInFokus(QWidget* fenster, Qt::Key taste)
{
    QWidget* ziel = fokusIn(fenster);
    QVERIFY2(ziel, "kein Fokus im Fenster -- der Tastendruck ging ins Leere");
    QTest::keyClick(ziel, taste);
}

} // namespace Contestprogramm::Pruefstand
