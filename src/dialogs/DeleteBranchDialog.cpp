//
//          Copyright (c) 2017, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#include "DeleteBranchDialog.h"
#include "git/Branch.h"
#include "git/Config.h"
#include "git/Remote.h"
#include "log/LogEntry.h"
#include "ui/RemoteCallbacks.h"
#include "ui/RepoView.h"
#include <QCheckBox>
#include <QFutureWatcher>
#include <QPushButton>
#include <QtConcurrent>

void DeleteBranchDialog::DeleteRemoteBranch(git::Branch branch)
{
  DeleteBranchDialogEvents *events = mEvents;

  if(events != nullptr)
    events->beginOperation();

  git::Repository repo = branch.repo();
  RepoView * view = RepoView::parentView(this);

  QString name = branch.name().section('/', 1);

  git::Remote remote = branch.remote();
  QString remoteName = remote.name();
  QString text = QObject::tr("delete '%1' from '%2'").arg(name, remoteName);
  LogEntry *entry = view->addLogEntry(text, QObject::tr("Push"));
  QFutureWatcher<git::Result> *watcher =
    new QFutureWatcher<git::Result>(view);
  RemoteCallbacks *callbacks = new RemoteCallbacks(
    RemoteCallbacks::Send, entry, remote.url(), remoteName, watcher, repo);

  entry->setBusy(true);
  QStringList refspecs(QString(":refs/heads/%1").arg(name));
  watcher->setFuture(
    QtConcurrent::run(remote, &git::Remote::push, callbacks, refspecs));

  QObject::connect(watcher, &QFutureWatcher<git::Result>::finished, watcher, 
  [entry, watcher, callbacks, remoteName, branch, events] {
    entry->setBusy(false);
    git::Result result = watcher->result();
    if (callbacks->isCanceled()) {
      entry->addEntry(LogEntry::Error, QObject::tr("Push canceled."));
    } else if (!result) {
      QString err = result.errorString();
      QString fmt = QObject::tr("Unable to push to %1 - %2");
      entry->addEntry(LogEntry::Error, fmt.arg(remoteName, err));
    } else if(events != nullptr) {
      events->emitRemoteBranchDeleted(branch);
    }

    if (events != nullptr)
      events->finishOperation();
    watcher->deleteLater();
  });
}

DeleteBranchDialog::DeleteBranchDialog(
  const git::Branch &branch,
  QWidget *parent,
  DeleteBranchDialogEvents *events)
  : QMessageBox(parent)
{
  mEvents = events;

  QString text;
  setWindowTitle(tr("Delete Branch?"));
  setStandardButtons(QMessageBox::Cancel);

  git::Branch upstream;
  if (branch.isLocalBranch()) {
    text = tr("Are you sure you want to delete local branch '%1'?");
    upstream = branch.upstream();

  } else {
     text = tr("Are you sure you want to delete remote branch '%1'?");
  }

  setText(text.arg(branch.name()));

  if (upstream.isValid()) {
    QString text = tr("Also delete the upstream branch from its remote");
    setCheckBox(new QCheckBox(text, this));
  }

  QPushButton *remove = addButton(tr("Delete"), QMessageBox::AcceptRole);
  connect (remove, &QPushButton::clicked, [this, branch, upstream] {
    if (mEvents != nullptr)
      mEvents->beginOperation();

    if (upstream.isValid() && checkBox()->isChecked())
      DeleteRemoteBranch(upstream);

    if (branch.isRemoteBranch())
      DeleteRemoteBranch(branch);
    else
      git::Branch(branch).remove();

    if (mEvents != nullptr)
      mEvents->finishOperation();
  });

  remove->setFocus();

  if (!branch.isMerged()) {
    setInformativeText(
      tr("The branch is not fully merged. Deleting "
         "it may cause some commits to be lost."));
    setDefaultButton(QMessageBox::Cancel);
  }
}
