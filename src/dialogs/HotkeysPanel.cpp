//
//          Copyright (c) 2018, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#include "HotkeysPanel.h"
#include "ui/HotkeyManager.h"
#include <QAbstractItemModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMap>
#include <QModelIndex>
#include <QRegularExpression>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

namespace
{
class HotkeyGroupData;
class HotkeyKeyData;

class HotkeyData : public QObject
{
public:
  enum class ObjectType
  {
    Key,
    Group
  };

  HotkeyData(QObject *parent, HotkeyGroupData *group, const QString &label)
    : QObject(parent), mGroup(group), mLabel(label)
  {
  }

  QString label() const
  {
    return mLabel;
  }

  HotkeyGroupData *group() const
  {
    return mGroup;
  }

  virtual ObjectType type() const = 0;

private:
  HotkeyGroupData *mGroup;
  QString mLabel;
};

class HotkeyGroupData : public HotkeyData
{
public:
  HotkeyGroupData(
    QObject *parent,
    const QString &label,
    HotkeyGroupData *group = nullptr
  ) : HotkeyData(parent, group, label)
  {
  }

  virtual ObjectType type() const override
  {
    return ObjectType::Group;
  }

  QVector<HotkeyData*> childrenData() const
  {
    return mChildren;
  }

  void addChildData(HotkeyData *data)
  {
    mChildren.append(data);
  }

private:
  QVector<HotkeyData*> mChildren;
};

class HotkeyKeyData : public HotkeyData
{
public:
  HotkeyKeyData(HotkeyGroupData *group, const QString &label, Hotkey hotkey, HotkeyManager *manager)
    : HotkeyData(group, group, label), mHotkey(hotkey), mManager(manager)
  {
    mKeys = hotkey.currentKeys(manager);
  }

  virtual ObjectType type() const override
  {
    return ObjectType::Key;
  }

  QKeySequence keys() const
  {
    return mKeys;
  }

  void setKeys(QKeySequence keys)
  {
    mHotkey.setKeys(keys, mManager);
    mKeys = keys;
  }

private:
  Hotkey mHotkey;
  HotkeyManager *mManager;
  QKeySequence mKeys;
};

class HotkeyModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  enum class ColumnIndex : int
  {
    Min = 0,
    Label = 0,
    Keys = 1,
    Max = 1
  };

  HotkeyModel(QObject *parent, HotkeyGroupData *root): QAbstractItemModel(parent), mRoot(root)
  {
  }

  virtual int columnCount(const QModelIndex &parent) const override
  {
    return 2;
  }

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if (!parent.isValid())
      return mRoot->childrenData().length();
    
    HotkeyData *data = (HotkeyData*)parent.internalPointer();
    if(data->type() != HotkeyData::ObjectType::Group)
      return 0;

    return ((HotkeyGroupData*)data)->childrenData().length();
  }

  virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if (row < 0 || column < (int)ColumnIndex::Min || column > (int)ColumnIndex::Max)
      return QModelIndex();

    HotkeyGroupData *group = nullptr;
    if (parent.isValid()) {
      HotkeyData *data = (HotkeyData*)parent.internalPointer();
      if(data->type() == HotkeyData::ObjectType::Group)
        group = (HotkeyGroupData*)data;

    } else {
      group = mRoot;
    }

    if(!group || row >= group->childrenData().length())
      return QModelIndex();

    return createIndex(row, column, group->childrenData()[row]);
  }

  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if (
      !index.isValid()
      || index.column() < (int)ColumnIndex::Min
      || index.column() > (int)ColumnIndex::Max
    )
      return QVariant();

    HotkeyData *data = (HotkeyData*)index.internalPointer();

    switch ((ColumnIndex)index.column()) {
      case ColumnIndex::Label:
        if (role == Qt::DisplayRole)
          return QVariant(data->label());
        else
          return QVariant();
      
      case ColumnIndex::Keys:
        if(data->type() != HotkeyData::ObjectType::Key)
          return QVariant();
        else if(role == Qt::DisplayRole)
          return QVariant(((HotkeyKeyData*)data)->keys().toString(QKeySequence::NativeText));
        else if(role == Qt::UserRole)
          return QVariant::fromValue(((HotkeyKeyData*)data)->keys());
        else
          return QVariant();
    }

    return QVariant();
  }

  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
  {
    if (!index.isValid() || index.column() != (int)ColumnIndex::Keys || role != Qt::UserRole)
      return false;

    HotkeyData *data = (HotkeyData*)index.internalPointer();
    if (data->type() != HotkeyData::ObjectType::Key)
      return false;

    ((HotkeyKeyData*)data)->setKeys(value.value<QKeySequence>());
    return true;
  }

  virtual QModelIndex parent(const QModelIndex &index) const override
  {
    if (
      !index.isValid()
      || index.column() < (int)ColumnIndex::Min
      || index.column() > (int)ColumnIndex::Max
    )
      return QModelIndex();

    HotkeyData *data = (HotkeyData*)index.internalPointer();
    HotkeyGroupData *parent = data->group();

    int parentRow = 0;
    if (parent && parent->group())
      parentRow = parent->group()->childrenData().indexOf(parent);

    if(!parent)
      return QModelIndex();
    else
      return createIndex(parentRow, 0, parent);
  }

  virtual Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if (
      !index.isValid()
      || index.column() < (int)ColumnIndex::Min
      || index.column() > (int)ColumnIndex::Max
    )
      return Qt::ItemFlag::NoItemFlags;
    
    Qt::ItemFlags res = Qt::ItemFlag::ItemIsEnabled;

    if (((HotkeyData*)index.internalPointer())->type() == HotkeyData::ObjectType::Key)
      res |= Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemNeverHasChildren;

    return res;
  }

  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
  {
    if (
      orientation != Qt::Orientation::Horizontal
      || section < (int)ColumnIndex::Min
      || section > (int)ColumnIndex::Max
      || role != Qt::DisplayRole
    )
      return QVariant();

    switch ((ColumnIndex)section) {
      case ColumnIndex::Label:
        return QVariant(tr("Action"));
      
      case ColumnIndex::Keys:
        return QVariant(tr("Keys"));
    }

    return QVariant();
  }

private:
  HotkeyGroupData *mRoot;
};

class SimpleKeyEdit : public QKeySequenceEdit
{
public:
  SimpleKeyEdit(QWidget *parent): QKeySequenceEdit(parent)
  {
  }

protected:
  virtual void keyPressEvent(QKeyEvent *e) override
  {
    if (e->modifiers() == Qt::KeyboardModifier::NoModifier || e->modifiers() == Qt::KeyboardModifier::KeypadModifier)
    {
      switch (e->key()) {
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
          setKeySequence(QKeySequence());

        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
          e->ignore();
          return;
      }
    }

    QKeySequenceEdit::keyPressEvent(e);
  }
};

class KeybindDialog : public QDialog
{
  Q_OBJECT

public:
  KeybindDialog(QWidget *parent): QDialog(parent)
  {
    QVBoxLayout *layout = new QVBoxLayout(this);

    mKeys = new SimpleKeyEdit(this);

    layout->addWidget(new QLabel(tr("Please press the desired hotkey"), this));
    layout->addWidget(mKeys);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttons);

    mKeys->setFocus();
  }

  QKeySequence keys() const
  {
    return mKeys->keySequence();
  }

  void setKeys(const QKeySequence &keys)
  {
    mKeys->setKeySequence(keys);
  }

private:
  SimpleKeyEdit *mKeys;
};
}

HotkeysPanel::HotkeysPanel(QWidget *parent) : QTreeView(parent)
{
  static QRegularExpression slashRegex("/+");

  HotkeyManager *manager = HotkeyManager::instance();

  QMap<QString, HotkeyGroupData*> groups;
  HotkeyGroupData *root = new HotkeyGroupData(this, "<Root>");

  // Build hotkey hierarchy
  for (Hotkey hotkey: manager->knownHotkeys()) {
    QString label = hotkey.label().replace(slashRegex, "/");
    int lastSep = label.lastIndexOf('/');

    HotkeyGroupData *group;

    // Look for existing group along hierarchy
    int pos = lastSep;
    while (pos >= 0) {
      group = groups.value(label.left(pos));
      if (group)
        break;

      pos = label.lastIndexOf('/', pos - 1);
    }

    // Use root if no matching group has been found
    if (!group) {
      Q_ASSERT(pos < 0);
      group = root;
      pos = 0;
    }

    // Build and insert missing groups
    while (pos < lastSep && pos >= 0) {
      pos = label.indexOf('/', pos + 1);
      QString path = label.left(pos);

      int subPos = path.lastIndexOf('/');

      group = new HotkeyGroupData(group, tr(path.mid(subPos + 1).toUtf8()), group);
      group->group()->addChildData(group);

      Q_ASSERT(!groups.contains(path));
      groups.insert(path, group);
    }

    // Add hotkey to group
    group->addChildData(new HotkeyKeyData(group, tr(label.mid(lastSep + 1).toUtf8()), hotkey, manager));
  }

  setModel(new HotkeyModel(this, root));
  setUniformRowHeights(true);
  setAllColumnsShowFocus(true);
  setEditTriggers(EditTrigger::DoubleClicked | EditTrigger::EditKeyPressed);
  expandAll();

  header()->setSectionsMovable(false);
  header()->setSectionResizeMode(0, QHeaderView::ResizeMode::ResizeToContents);
  header()->setSectionResizeMode(1, QHeaderView::ResizeMode::Stretch);
}

bool HotkeysPanel::edit(const QModelIndex &index, QAbstractItemView::EditTrigger trigger, QEvent *event)
{
  if (!index.isValid() || !(trigger & editTriggers()))
    return false;

  QModelIndex keyIndex = index.siblingAtColumn((int)HotkeyModel::ColumnIndex::Keys);
  QVariant data = keyIndex.data(Qt::UserRole);

  if(!data.isValid())
    return false;

  KeybindDialog *dialog = new KeybindDialog(this);

  dialog->setKeys(data.value<QKeySequence>());

  QPersistentModelIndex idx(keyIndex);
  connect(dialog, &QDialog::accepted, [this, idx, dialog]() {
    model()->setData(idx, QVariant::fromValue(dialog->keys()), Qt::UserRole);
  });

  dialog->setModal(true);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  return true;
}

QSize HotkeysPanel::sizeHint() const
{
  return QSize(600, 320);
}

void HotkeysPanel::keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
    QModelIndexList indexes = selectedIndexes();
    if (!indexes.isEmpty())
      edit(indexes.first(), EditTrigger::EditKeyPressed, e);

    return;
  }

  QTreeView::keyPressEvent(e);
}

#include "HotkeysPanel.moc"