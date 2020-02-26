//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#ifndef REFERENCEVIEW_H
#define REFERENCEVIEW_H

#include <QTreeView>

namespace git {
class Reference;
class Repository;
}

class ReferenceView : public QTreeView
{
  Q_OBJECT

public:
  enum Kind
  {
    InvalidRef = 0x1, // invalid element
    DetachedHead = 0x2,
    LocalBranches = 0x4,
    RemoteBranches = 0x8,
    Tags = 0x10,
    Stash = 0x20,
    ExcludeHead = 0x40,
    AllRefs = DetachedHead | LocalBranches | RemoteBranches | Tags | Stash
  };

  Q_DECLARE_FLAGS(Kinds, Kind);

  ReferenceView(
    const git::Repository &repo,
    Kinds kinds = AllRefs,
    bool popup = false,
    QWidget *parent = nullptr);

  bool isPopup() const { return mPopup; }
  void resetTabIndex();

  git::Reference currentReference() const;

  bool eventFilter(QObject *watched, QEvent *event) override;

  static QString kindString(const git::Reference &ref);

protected:
  void showEvent(QShowEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  bool mPopup;
  void *mSource;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ReferenceView::Kinds);

#endif
