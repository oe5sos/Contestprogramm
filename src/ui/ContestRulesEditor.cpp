#include "ui/ContestRulesEditor.h"

#include "core/BandUtils.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
constexpr int kColKey = 0;
constexpr int kColLabel = 1;
constexpr int kColType = 2;
constexpr int kColAutoIncrement = 3;

// Die Betriebsarten, die eine Ausschreibung vorschreiben kann. Keine
// angekreuzt heißt "alle erlaubt" -- so liest ContestDefinition::modes()
// eine leere Liste, und so steht es auch unter den Kästchen.
const char* const kModeKeys[] = {"CW", "SSB", "FM", "RTTY", "DIGITAL"};

QTableWidgetItem* makeAutoIncrementItem(bool checked)
{
    auto* item = new QTableWidgetItem();
    item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(Style::capsFont(label->font()));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));
    return label;
}

void fillChoiceCombo(QComboBox* combo, const QVector<QPair<QString, QString>>& choices)
{
    for (const auto& choice : choices) {
        combo->addItem(choice.second, choice.first);
    }
}

void selectChoice(QComboBox* combo, const QString& key)
{
    const int index = combo->findData(key);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}
} // namespace

ContestRulesEditor::ContestRulesEditor(const QVector<ContestDefinition>& availableContests,
                                       const QString& initialContestId,
                                       QWidget* parent)
    : QDialog(parent)
    , m_availableContests(availableContests)
    , m_contestCombo(new QComboBox(this))
    , m_newButton(new QPushButton(QStringLiteral("Neuer Contest..."), this))
    , m_idLabel(new QLabel(this))
    , m_nameEdit(new QLineEdit(this))
    , m_fieldsTable(new QTableWidget(this))
    , m_addButton(new QPushButton(QStringLiteral("+ Feld"), this))
    , m_removeButton(new QPushButton(QStringLiteral("- Feld"), this))
    , m_moveUpButton(new QPushButton(QStringLiteral("Auf"), this))
    , m_moveDownButton(new QPushButton(QStringLiteral("Ab"), this))
    , m_scoringCombo(new QComboBox(this))
    , m_serialScopeCombo(new QComboBox(this))
    , m_dupeBandCheck(new QCheckBox(QStringLiteral("Band"), this))
    , m_dupeModeCheck(new QCheckBox(QStringLiteral("Betriebsart"), this))
    , m_multiplierCombo(new QComboBox(this))
    , m_cabrilloEdit(new QLineEdit(this))
    , m_resetButton(new QPushButton(QStringLiteral("Zurücksetzen"), this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Contest-Regeln"));
    setModal(true);

    for (const ContestDefinition& def : m_availableContests) {
        m_contestCombo->addItem(def.name(), def.id());
    }
    const int initialIdx = m_contestCombo->findData(initialContestId);
    m_contestCombo->setCurrentIndex(initialIdx >= 0 ? initialIdx : 0);

    m_contestCombo->setObjectName(QStringLiteral("contestCombo"));
    m_newButton->setObjectName(QStringLiteral("contestNew"));
    m_nameEdit->setObjectName(QStringLiteral("contestName"));
    m_cabrilloEdit->setObjectName(QStringLiteral("contestCabrillo"));
    m_scoringCombo->setObjectName(QStringLiteral("contestScoring"));
    m_serialScopeCombo->setObjectName(QStringLiteral("contestSerialScope"));
    m_multiplierCombo->setObjectName(QStringLiteral("contestMultiplier"));
    m_dupeBandCheck->setObjectName(QStringLiteral("contestDupeBand"));
    m_dupeModeCheck->setObjectName(QStringLiteral("contestDupeMode"));
    m_resetButton->setObjectName(QStringLiteral("contestReset"));
    m_fieldsTable->setObjectName(QStringLiteral("contestFields"));

    m_idLabel->setFont(Style::monoFont(m_idLabel->font(), Style::kFontSmall));
    m_idLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    m_idLabel->setToolTip(QStringLiteral("Die Kennung steht in jedem geloggten QSO und ändert sich nicht mehr."));

    fillChoiceCombo(m_scoringCombo, ContestDefinition::scoringChoices());
    fillChoiceCombo(m_serialScopeCombo, ContestDefinition::serialScopeChoices());
    fillChoiceCombo(m_multiplierCombo, ContestDefinition::multiplierChoices());

    m_fieldsTable->setColumnCount(4);
    m_fieldsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Key"), QStringLiteral("Label"), QStringLiteral("Typ"), QStringLiteral("Serial (auto)")});
    m_fieldsTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldsTable->horizontalHeader()->setFont(Style::capsFont(m_fieldsTable->horizontalHeader()->font()));
    m_fieldsTable->setFont(Style::monoFont(m_fieldsTable->font(), Style::kFontSmall));
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fieldsTable->setMinimumHeight(140);

    // --- Bänder: ein Kästchen je Band, das die Bandtabelle kennt.
    auto* bandBox = new QGroupBox(QStringLiteral("Bänder"), this);
    auto* bandGrid = new QGridLayout(bandBox);
    bandGrid->setContentsMargins(10, 6, 10, 8);
    bandGrid->setHorizontalSpacing(14);
    int column = 0;
    int row = 0;
    for (const QString& band : knownBands()) {
        auto* check = new QCheckBox(band, bandBox);
        check->setObjectName(QStringLiteral("band_%1").arg(band));
        check->setProperty("band", band);
        m_bandChecks.append(check);
        bandGrid->addWidget(check, row, column);
        if (++column == 6) {
            column = 0;
            ++row;
        }
    }

    // --- Exchange-Felder mit ihren vier Knöpfen.
    auto* fieldBox = new QGroupBox(QStringLiteral("Exchange-Felder (in dieser Reihenfolge)"), this);
    auto* fieldLayout = new QVBoxLayout(fieldBox);
    fieldLayout->setContentsMargins(10, 6, 10, 8);
    fieldLayout->addWidget(m_fieldsTable, 1);
    auto* toolRow = new QWidget(fieldBox);
    auto* toolLayout = new QHBoxLayout(toolRow);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->addWidget(m_addButton);
    toolLayout->addWidget(m_removeButton);
    toolLayout->addWidget(m_moveUpButton);
    toolLayout->addWidget(m_moveDownButton);
    toolLayout->addStretch();
    fieldLayout->addWidget(toolRow);

    // --- Wertung, Nummernkreis, Dupe, Multiplikator.
    auto* ruleBox = new QGroupBox(QStringLiteral("Wertung"), this);
    auto* ruleGrid = new QGridLayout(ruleBox);
    ruleGrid->setContentsMargins(10, 6, 10, 8);
    ruleGrid->setHorizontalSpacing(12);
    ruleGrid->addWidget(caption(QStringLiteral("Punkte"), ruleBox), 0, 0);
    ruleGrid->addWidget(m_scoringCombo, 0, 1, 1, 3);
    ruleGrid->addWidget(caption(QStringLiteral("Laufende Nummer"), ruleBox), 1, 0);
    ruleGrid->addWidget(m_serialScopeCombo, 1, 1, 1, 3);
    ruleGrid->addWidget(caption(QStringLiteral("Multiplikator"), ruleBox), 2, 0);
    ruleGrid->addWidget(m_multiplierCombo, 2, 1, 1, 3);
    ruleGrid->addWidget(caption(QStringLiteral("Dupe je"), ruleBox), 3, 0);
    auto* dupeFixed = new QLabel(QStringLiteral("Rufzeichen"), ruleBox);
    dupeFixed->setEnabled(false);
    dupeFixed->setToolTip(QStringLiteral("Ohne Rufzeichen wäre jedes QSO auf demselben Band ein Doppel."));
    ruleGrid->addWidget(dupeFixed, 3, 1);
    ruleGrid->addWidget(m_dupeBandCheck, 3, 2);
    ruleGrid->addWidget(m_dupeModeCheck, 3, 3);
    ruleGrid->setColumnStretch(3, 1);

    // --- Betriebsarten und Cabrillo-Name.
    auto* modeBox = new QGroupBox(QStringLiteral("Erlaubte Betriebsarten (keine angekreuzt = alle)"), this);
    auto* modeLayout = new QHBoxLayout(modeBox);
    modeLayout->setContentsMargins(10, 6, 10, 8);
    for (const char* key : kModeKeys) {
        auto* check = new QCheckBox(QString::fromLatin1(key), modeBox);
        check->setObjectName(QStringLiteral("mode_%1").arg(QString::fromLatin1(key)));
        check->setProperty("mode", QString::fromLatin1(key));
        m_modeChecks.append(check);
        modeLayout->addWidget(check);
    }
    modeLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    // Qts eigene Beschriftungen kommen nur mit geladener Übersetzung
    // auf Deutsch -- ohne sie stünde hier "Save"/"Cancel" mitten in
    // einem deutschen Fenster.
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Speichern"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
    buttons->addButton(m_resetButton, QDialogButtonBox::ResetRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &ContestRulesEditor::onSave);

    // Kein abgerundeter Panelrahmen: wie MultiplierWindow ein echtes
    // Fenster, und ein border-radius auf dessen eigenem Rechteck ließe
    // an den Ecken durchscheinen, was das Betriebssystem dahinter malt.
    auto* header = new PanelHeaderBar(QStringLiteral("Contest-Regeln"), this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->setSpacing(8);
    auto* pickRow = new QWidget(body);
    auto* pickLayout = new QHBoxLayout(pickRow);
    pickLayout->setContentsMargins(0, 0, 0, 0);
    pickLayout->addWidget(m_contestCombo, 1);
    pickLayout->addWidget(m_newButton);
    bodyLayout->addWidget(pickRow);
    auto* nameRow = new QWidget(body);
    auto* nameLayout = new QHBoxLayout(nameRow);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->addWidget(caption(QStringLiteral("Name"), nameRow));
    nameLayout->addWidget(m_nameEdit, 1);
    nameLayout->addWidget(m_idLabel);
    bodyLayout->addWidget(nameRow);
    bodyLayout->addWidget(bandBox);
    bodyLayout->addWidget(fieldBox, 1);
    bodyLayout->addWidget(ruleBox);
    bodyLayout->addWidget(modeBox);
    auto* cabrilloRow = new QWidget(body);
    auto* cabrilloLayout = new QHBoxLayout(cabrilloRow);
    cabrilloLayout->setContentsMargins(0, 0, 0, 0);
    cabrilloLayout->addWidget(caption(QStringLiteral("Cabrillo-Name"), cabrilloRow));
    m_cabrilloEdit->setPlaceholderText(QStringLiteral("z. B. CQ-WW-CW — leer: die Kennung"));
    cabrilloLayout->addWidget(m_cabrilloEdit, 1);
    bodyLayout->addWidget(cabrilloRow);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);

    auto* buttonRow = new QWidget(this);
    auto* buttonLayout = new QVBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(10, 0, 10, 10);
    buttonLayout->addWidget(buttons);
    layout->addWidget(buttonRow);

    connect(m_contestCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ContestRulesEditor::onContestSelectionChanged);
    connect(m_newButton, &QPushButton::clicked, this, &ContestRulesEditor::onNewContest);
    connect(m_addButton, &QPushButton::clicked, this, &ContestRulesEditor::onAddField);
    connect(m_removeButton, &QPushButton::clicked, this, &ContestRulesEditor::onRemoveField);
    connect(m_moveUpButton, &QPushButton::clicked, this, &ContestRulesEditor::onMoveFieldUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ContestRulesEditor::onMoveFieldDown);
    connect(m_resetButton, &QPushButton::clicked, this, &ContestRulesEditor::onResetToShipped);

    onContestSelectionChanged(m_contestCombo->currentIndex());

    resize(620, 760);
}

const ContestDefinition* ContestRulesEditor::selectedDefinition() const
{
    const QString id = m_contestCombo->currentData().toString();
    for (const ContestDefinition& def : m_availableContests) {
        if (def.id() == id) {
            return &def;
        }
    }
    return nullptr;
}

QString ContestRulesEditor::proposeIdFor(const QString& name) const
{
    QString base;
    for (const QChar& c : name.toUpper()) {
        if (c.isLetterOrNumber() && c.unicode() < 128) {
            base.append(c);
        } else if (c == QChar(0x00C4)) { // Ä
            base.append(QStringLiteral("AE"));
        } else if (c == QChar(0x00D6)) { // Ö
            base.append(QStringLiteral("OE"));
        } else if (c == QChar(0x00DC)) { // Ü
            base.append(QStringLiteral("UE"));
        } else if (!base.isEmpty() && !base.endsWith(QLatin1Char('_'))) {
            base.append(QLatin1Char('_'));
        }
    }
    while (base.endsWith(QLatin1Char('_'))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        base = QStringLiteral("CONTEST");
    }
    QSet<QString> taken;
    for (const ContestDefinition& def : m_availableContests) {
        taken.insert(def.id());
    }
    QString candidate = base;
    int suffix = 2;
    while (taken.contains(candidate)) {
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
    }
    return candidate;
}

void ContestRulesEditor::loadRulesIntoForm(const ContestDefinition& definition)
{
    const ContestDefinition::Rules rules = definition.rules();
    m_idLabel->setText(definition.id());
    m_nameEdit->setText(rules.name);
    for (QCheckBox* check : m_bandChecks) {
        check->setChecked(rules.bands.contains(check->property("band").toString()));
    }
    loadFieldsIntoTable(rules.exchangeFields);
    selectChoice(m_scoringCombo, rules.scoring);
    selectChoice(m_serialScopeCombo, rules.serialScope);
    selectChoice(m_multiplierCombo, rules.multiplierField);
    m_dupeBandCheck->setChecked(rules.dupeScope.contains(QStringLiteral("band")));
    m_dupeModeCheck->setChecked(rules.dupeScope.contains(QStringLiteral("mode")));
    for (QCheckBox* check : m_modeChecks) {
        check->setChecked(rules.modes.contains(check->property("mode").toString()));
    }
    m_cabrilloEdit->setText(rules.cabrilloName);
    m_resetButton->setEnabled(QFile::exists(ContestDefinition::overrideFilePath(definition.id())));
}

ContestDefinition::Rules ContestRulesEditor::rulesFromForm() const
{
    ContestDefinition::Rules rules;
    rules.name = m_nameEdit->text().trimmed();
    for (const QCheckBox* check : m_bandChecks) {
        if (check->isChecked()) {
            rules.bands.append(check->property("band").toString());
        }
    }
    rules.exchangeFields = fieldsFromTable();
    rules.scoring = m_scoringCombo->currentData().toString();
    rules.serialScope = m_serialScopeCombo->currentData().toString();
    rules.multiplierField = m_multiplierCombo->currentData().toString();
    rules.dupeScope.append(QStringLiteral("callsign"));
    if (m_dupeBandCheck->isChecked()) {
        rules.dupeScope.append(QStringLiteral("band"));
    }
    if (m_dupeModeCheck->isChecked()) {
        rules.dupeScope.append(QStringLiteral("mode"));
    }
    for (const QCheckBox* check : m_modeChecks) {
        if (check->isChecked()) {
            rules.modes.append(check->property("mode").toString());
        }
    }
    rules.cabrilloName = m_cabrilloEdit->text().trimmed();
    return rules;
}

void ContestRulesEditor::loadFieldsIntoTable(const QVector<ContestDefinition::ExchangeField>& fields)
{
    m_fieldsTable->setRowCount(0);
    for (const ContestDefinition::ExchangeField& field : fields) {
        const int row = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(row);
        m_fieldsTable->setItem(row, kColKey, new QTableWidgetItem(field.key));
        m_fieldsTable->setItem(row, kColLabel, new QTableWidgetItem(field.label));
        m_fieldsTable->setItem(row, kColType, new QTableWidgetItem(field.type));
        m_fieldsTable->setItem(row, kColAutoIncrement, makeAutoIncrementItem(field.autoIncrement));
    }
}

QVector<ContestDefinition::ExchangeField> ContestRulesEditor::fieldsFromTable() const
{
    QVector<ContestDefinition::ExchangeField> fields;
    for (int row = 0; row < m_fieldsTable->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_fieldsTable->item(row, kColKey);
        const QString key = keyItem ? keyItem->text().trimmed() : QString();
        if (key.isEmpty()) {
            // Eine leere Zeile ("+ Feld" geklickt, nie ausgefüllt) --
            // weggelassen statt als namenloses Feld gespeichert.
            continue;
        }
        ContestDefinition::ExchangeField field;
        field.key = key;
        QTableWidgetItem* labelItem = m_fieldsTable->item(row, kColLabel);
        field.label = labelItem ? labelItem->text().trimmed() : QString();
        QTableWidgetItem* typeItem = m_fieldsTable->item(row, kColType);
        field.type = typeItem ? typeItem->text().trimmed() : QString();
        QTableWidgetItem* autoItem = m_fieldsTable->item(row, kColAutoIncrement);
        field.autoIncrement = autoItem && autoItem->checkState() == Qt::Checked;
        fields.append(field);
    }
    return fields;
}

void ContestRulesEditor::onContestSelectionChanged(int /*index*/)
{
    if (const ContestDefinition* def = selectedDefinition()) {
        loadRulesIntoForm(*def);
    } else {
        m_fieldsTable->setRowCount(0);
        m_idLabel->clear();
        m_nameEdit->clear();
        m_resetButton->setEnabled(false);
    }
}

void ContestRulesEditor::onNewContest()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Neuer Contest"),
        QStringLiteral("Name des Contests.\nEr beginnt mit den Regeln des gerade gewählten und wird dann angepasst."),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    createDraftContest(name);
}

QString ContestRulesEditor::createDraftContest(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        return QString();
    }
    const ContestDefinition* current = selectedDefinition();
    ContestDefinition::Rules rules = current ? current->rules() : ContestDefinition::Rules();
    rules.name = name.trimmed();
    if (rules.exchangeFields.isEmpty()) {
        ContestDefinition::ExchangeField rst;
        rst.key = QStringLiteral("rst");
        rst.label = QStringLiteral("RST");
        rst.type = QStringLiteral("rst");
        ContestDefinition::ExchangeField serial;
        serial.key = QStringLiteral("serial");
        serial.label = QStringLiteral("Nr.");
        serial.type = QStringLiteral("int");
        serial.autoIncrement = true;
        rules.exchangeFields = {rst, serial};
    }
    if (rules.bands.isEmpty()) {
        rules.bands = {QStringLiteral("144")};
    }
    if (rules.dupeScope.isEmpty()) {
        rules.dupeScope = {QStringLiteral("callsign"), QStringLiteral("band")};
    }
    // Ein neuer Contest erbt den Zeitplan NICHT: die Termine der
    // Vorlage sind ihre, nicht seine.
    QString error;
    const ContestDefinition draft = ContestDefinition::fromRules(proposeIdFor(rules.name), rules, &error);
    if (!draft.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), error);
        return QString();
    }
    m_availableContests.append(draft);
    m_contestCombo->addItem(draft.name(), draft.id());
    m_contestCombo->setCurrentIndex(m_contestCombo->count() - 1);
    return draft.id();
}

void ContestRulesEditor::onAddField()
{
    const int row = m_fieldsTable->rowCount();
    m_fieldsTable->insertRow(row);
    m_fieldsTable->setItem(row, kColKey, new QTableWidgetItem(QString()));
    m_fieldsTable->setItem(row, kColLabel, new QTableWidgetItem(QString()));
    m_fieldsTable->setItem(row, kColType, new QTableWidgetItem(QStringLiteral("text")));
    m_fieldsTable->setItem(row, kColAutoIncrement, makeAutoIncrementItem(false));
    m_fieldsTable->setCurrentCell(row, kColKey);
    m_fieldsTable->editItem(m_fieldsTable->item(row, kColKey));
}

void ContestRulesEditor::onRemoveField()
{
    const int row = m_fieldsTable->currentRow();
    if (row >= 0) {
        m_fieldsTable->removeRow(row);
    }
}

void ContestRulesEditor::onMoveFieldUp()
{
    const int row = m_fieldsTable->currentRow();
    if (row <= 0) {
        return;
    }
    for (int col = 0; col < m_fieldsTable->columnCount(); ++col) {
        QTableWidgetItem* above = m_fieldsTable->takeItem(row - 1, col);
        QTableWidgetItem* current = m_fieldsTable->takeItem(row, col);
        m_fieldsTable->setItem(row - 1, col, current);
        m_fieldsTable->setItem(row, col, above);
    }
    m_fieldsTable->setCurrentCell(row - 1, 0);
}

void ContestRulesEditor::onMoveFieldDown()
{
    const int row = m_fieldsTable->currentRow();
    if (row < 0 || row >= m_fieldsTable->rowCount() - 1) {
        return;
    }
    for (int col = 0; col < m_fieldsTable->columnCount(); ++col) {
        QTableWidgetItem* below = m_fieldsTable->takeItem(row + 1, col);
        QTableWidgetItem* current = m_fieldsTable->takeItem(row, col);
        m_fieldsTable->setItem(row + 1, col, current);
        m_fieldsTable->setItem(row, col, below);
    }
    m_fieldsTable->setCurrentCell(row + 1, 0);
}

void ContestRulesEditor::onResetToShipped()
{
    const ContestDefinition* def = selectedDefinition();
    if (!def) {
        return;
    }
    const QString path = ContestDefinition::overrideFilePath(def->id());
    if (!QFile::exists(path)) {
        return;
    }
    // Der Text deckt beide Fälle ab, weil dieser Dialog nicht weiß, ob
    // es zu diesem Contest eine ausgelieferte Fassung gibt -- der
    // Aufrufer lädt danach neu und sieht das Ergebnis.
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Contestprogramm"),
        QStringLiteral("Eigene Einstellungen für »%1« verwerfen?\n\n"
                       "Gibt es eine ausgelieferte Fassung, gilt danach wieder diese. "
                       "Hast du den Contest selbst angelegt, ist er damit weg.\n\n"
                       "Geloggte QSOs bleiben in jedem Fall erhalten.")
            .arg(def->name()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    if (!QFile::remove(path)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Die eigene Datei konnte nicht gelöscht werden:\n%1").arg(path));
        return;
    }
    m_savedContestId = def->id();
    m_deletedContest = true;
    accept();
}

void ContestRulesEditor::onSave()
{
    const ContestDefinition* def = selectedDefinition();
    if (!def) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), QStringLiteral("Kein Contest ausgewählt."));
        return;
    }

    QString error;
    const ContestDefinition updated = def->withRules(rulesFromForm(), &error);
    if (!updated.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), error);
        return;
    }
    if (!updated.saveToFile(ContestDefinition::overrideFilePath(def->id()), &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Konnte nicht gespeichert werden:\n%1").arg(error));
        return;
    }

    m_savedContestId = def->id();
    accept();
}

} // namespace Contestprogramm
