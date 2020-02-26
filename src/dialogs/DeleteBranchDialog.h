//
//          Copyright (c) 2017, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#ifndef DELETEBRANCHDIALOG_H
#define DELETEBRANCHDIALOG_H

#include <QMessageBox>
#include "git/Branch.h"

class DeleteBranchDialogEvents : public QObject
{
  friend class DeleteBranchDialog;
  Q_OBJECT

signals:
  void remoteBranchDeleted(git::Branch branch);
  void allDone();

private:
  int mOperations = 0;
  bool mDeleteAfterDone = false;

  void emitRemoteBranchDeleted(git::Branch branch)
  {
    emit remoteBranchDeleted(branch);
  }

  void finishOperation()
  {
    --mOperations;
    if (mOperations == 0) {
      emit allDone();

      if (mDeleteAfterDone)
        deleteLater();
    }
  }

  void beginOperation()
  {
    ++mOperations;
  }

public:
  void setDeleteAfterDone(bool value = true)
  {
    mDeleteAfterDone = value;
  }

  bool deleteAfterDone() const
  {
    return mDeleteAfterDone;
  }
};

class DeleteBranchDialog : public QMessageBox
{
  Q_OBJECT

private:
  DeleteBranchDialogEvents *mEvents;

  void DeleteRemoteBranch(git::Branch branch);

public:
  DeleteBranchDialog(const git::Branch &branch,
    QWidget *parent = nullptr,
    DeleteBranchDialogEvents *events = nullptr);
};

#endif 
