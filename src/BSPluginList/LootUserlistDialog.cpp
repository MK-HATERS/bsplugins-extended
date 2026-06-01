#include "LootUserlistDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

namespace BSPluginList
{

using namespace Qt::Literals::StringLiterals;

// ---------------------------------------------------------------------------
// Very simple userlist.yaml read/write.
// Format assumed:
//   plugins:
//   - name: 'Plugin.esp'
//     group: 'GroupName'
//     after:
//     - name: 'OtherMod.esp'
//     require:
//     - name: 'Framework.esm'
//     conflict:
//     - name: 'Bad.esp'
//
// We find the plugin's block by its name line and parse the keys that follow
// until the next top-level list item (- name:) or end-of-file. Writing back
// replaces the block in-place, or appends a new block.
// ---------------------------------------------------------------------------

static QString quotedName(const QString& s)
{
  return u"'%1'"_s.arg(QString(s).replace(u'\'', u"\\'"_s));
}

static QStringList extractBlock(const QStringList& lines, int pluginLineIdx)
{
  QStringList block;
  block << lines.at(pluginLineIdx);
  for (int i = pluginLineIdx + 1; i < lines.size(); ++i) {
    const QString& line = lines.at(i);
    // A new top-level plugin entry starts with "- name:" at the two-space indent
    if (line.startsWith(u"- name:"_s)) break;
    block << line;
  }
  return block;
}

static QStringList parseNameList(const QStringList& block, const QString& key)
{
  QStringList result;
  bool inKey = false;
  for (const QString& line : block) {
    const QString trimmed = line.trimmed();
    if (trimmed == key + u":"_s) { inKey = true; continue; }
    if (inKey) {
      if (trimmed.startsWith(u"- name:"_s)) {
        QString n = trimmed.mid(7).trimmed();
        n.remove(u'\''_s);
        result << n;
      } else if (!trimmed.startsWith(u"-"_s) && trimmed.contains(u":"_s)) {
        inKey = false;  // hit another key
      }
    }
  }
  return result;
}

static QString parseGroup(const QStringList& block)
{
  for (const QString& line : block) {
    const QString t = line.trimmed();
    if (t.startsWith(u"group:"_s)) {
      QString g = t.mid(6).trimmed();
      g.remove(u'\''_s);
      return g;
    }
  }
  return {};
}

static QStringList buildBlock(const QString& name, const QString& group,
                              const QStringList& after, const QStringList& require,
                              const QStringList& conflict)
{
  QStringList out;
  out << u"- name: %1"_s.arg(quotedName(name));
  if (!group.isEmpty())
    out << u"  group: %1"_s.arg(quotedName(group));
  auto section = [&](const QString& key, const QStringList& list) {
    if (list.isEmpty()) return;
    out << u"  %1:"_s.arg(key);
    for (const QString& n : list)
      out << u"  - name: %1"_s.arg(quotedName(n));
  };
  section(u"after"_s,    after);
  section(u"require"_s,  require);
  section(u"conflict"_s, conflict);
  return out;
}

// ---------------------------------------------------------------------------

LootUserlistDialog::LootUserlistDialog(const QString& pluginName,
                                       const QString& userlistPath,
                                       QWidget* parent)
    : QDialog(parent), m_PluginName(pluginName), m_UserlistPath(userlistPath)
{
  setWindowTitle(tr("LOOT Rule — %1").arg(pluginName));
  setMinimumWidth(500);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(8);

  auto* info = new QLabel(
      tr("Add or edit load-order rules for <b>%1</b> in LOOT's userlist.yaml. "
         "These rules override the masterlist and take effect on the next LOOT sort.")
          .arg(pluginName),
      this);
  info->setWordWrap(true);
  root->addWidget(info);

  // Group field
  auto* form = new QFormLayout;
  m_GroupEdit = new QLineEdit(this);
  m_GroupEdit->setPlaceholderText(tr("e.g. Default (leave empty to keep current)"));
  m_GroupEdit->setToolTip(
      tr("Assign this plugin to a LOOT group. Plugins in a group load after plugins "
         "in earlier groups in the LOOT group graph."));
  form->addRow(tr("LOOT Group:"), m_GroupEdit);
  root->addLayout(form);

  auto makeListGroup = [&](const QString& title, const QString& tip,
                           QListWidget*& listOut) {
    auto* gb  = new QGroupBox(title, this);
    auto* lay = new QVBoxLayout(gb);
    gb->setToolTip(tip);
    auto* list = new QListWidget(gb);
    list->setMaximumHeight(100);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(list, 1);
    auto* btnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("+ Add"), gb);
    auto* delBtn = new QPushButton(tr("Remove"), gb);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);
    connect(addBtn, &QPushButton::clicked, this, [this, list]{ addListItem(list); });
    connect(delBtn, &QPushButton::clicked, this, [this, list]{ removeListItem(list); });
    listOut = list;
    return gb;
  };

  root->addWidget(makeListGroup(
      tr("Load after:"),
      tr("This plugin will be forced to load after all plugins in this list."),
      m_AfterList));
  root->addWidget(makeListGroup(
      tr("Requires:"),
      tr("LOOT will warn if any of these plugins are missing or disabled."),
      m_RequireList));
  root->addWidget(makeListGroup(
      tr("Conflicts with:"),
      tr("LOOT will warn if any of these plugins are also active."),
      m_ConflictList));

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, [this]{
    save();
    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  load();
}

void LootUserlistDialog::load()
{
  QFile f(m_UserlistPath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;

  QTextStream ts(&f);
  const QStringList lines = ts.readAll().split(u'\n');
  f.close();

  // Find the plugin's entry
  const QString marker = u"- name: %1"_s.arg(quotedName(m_PluginName));
  int idx = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines.at(i).trimmed() == marker.trimmed() ||
        lines.at(i).trimmed() == u"- name: %1"_s.arg(m_PluginName).trimmed()) {
      idx = i; break;
    }
  }
  if (idx < 0) return;

  const QStringList block = extractBlock(lines, idx);
  m_GroupEdit->setText(parseGroup(block));
  for (const QString& n : parseNameList(block, u"after"_s))
    m_AfterList->addItem(n);
  for (const QString& n : parseNameList(block, u"require"_s))
    m_RequireList->addItem(n);
  for (const QString& n : parseNameList(block, u"conflict"_s))
    m_ConflictList->addItem(n);
}

void LootUserlistDialog::save()
{
  QStringList after, require, conflict;
  for (int i = 0; i < m_AfterList->count();   ++i) after   << m_AfterList->item(i)->text();
  for (int i = 0; i < m_RequireList->count();  ++i) require << m_RequireList->item(i)->text();
  for (int i = 0; i < m_ConflictList->count(); ++i) conflict << m_ConflictList->item(i)->text();

  const QStringList newBlock = buildBlock(m_PluginName,
                                          m_GroupEdit->text().trimmed(),
                                          after, require, conflict);

  // Read existing file (or create)
  QStringList lines;
  QFile f(m_UserlistPath);
  if (f.open(QIODevice::ReadOnly)) {
    QTextStream ts(&f);
    lines = ts.readAll().split(u'\n');
    f.close();
  }

  // Ensure "plugins:" header exists
  if (!lines.contains(u"plugins:"_s)) {
    lines.prepend(u"plugins:"_s);
  }

  // Find and replace or append the plugin's block
  const QString namePattern = u"- name: %1"_s.arg(quotedName(m_PluginName));
  int startIdx = -1;
  for (int i = 0; i < lines.size(); ++i) {
    const QString t = lines.at(i).trimmed();
    if (t == namePattern.trimmed() ||
        t == u"- name: %1"_s.arg(m_PluginName).trimmed()) {
      startIdx = i; break;
    }
  }

  if (startIdx >= 0) {
    // Find end of this plugin's block
    int endIdx = startIdx + 1;
    while (endIdx < lines.size() &&
           !lines.at(endIdx).trimmed().startsWith(u"- name:"_s)) {
      ++endIdx;
    }
    lines.erase(lines.begin() + startIdx, lines.begin() + endIdx);
    for (int i = 0; i < newBlock.size(); ++i)
      lines.insert(startIdx + i, newBlock.at(i));
  } else {
    lines << newBlock;
  }

  // Write back
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Could not write userlist.yaml:\n%1").arg(m_UserlistPath));
    return;
  }
  QTextStream out(&f);
  out << lines.join(u'\n');
  f.close();
}

void LootUserlistDialog::addListItem(QListWidget* list)
{
  const QString name = QInputDialog::getText(
      this, tr("Add Plugin"), tr("Plugin filename:"));
  if (!name.trimmed().isEmpty())
    list->addItem(name.trimmed());
}

void LootUserlistDialog::removeListItem(QListWidget* list)
{
  const auto sel = list->selectedItems();
  for (auto* item : sel) delete item;
}

}  // namespace BSPluginList
