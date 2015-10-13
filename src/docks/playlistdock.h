/*
 * Copyright (c) 2012-2015 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYLISTDOCK_H
#define PLAYLISTDOCK_H

#include <QDockWidget>
#include <QQuickWidget>
#include <QUndoCommand>
#include "models/playlistmodel.h"

class PlaylistDock : public QDockWidget
{
    Q_OBJECT
    Q_PROPERTY(QPoint widgetPos READ widgetPos)
    Q_PROPERTY(int activeClipActions READ activeClipActions NOTIFY clipActionsEvaluationRequested)
    Q_ENUMS(ClipActions)

public:
    enum ClipActions
    {
        InsertCut = 0x1,
        InsertBlank = 0x2,
        Goto = 0x4,
        Open = 0x8,
        Update = 0x10,
        Remove = 0x20
    };
    explicit PlaylistDock(QWidget *parent = 0);
    ~PlaylistDock();
    PlaylistModel* model() {
        return &m_model;
    }
    int position();
    int currentIndex() const;
    QPoint widgetPos() const;
    int activeClipActions() const;
    void startEditingComment();

signals:
    void clipOpened(void* producer);
    void itemActivated(int start);
    void showStatusMessage(QString);
    void addAllTimeline(Mlt::Playlist*);
    void clipActionsEvaluationRequested();

public slots:
    void incrementIndex();
    void decrementIndex();
    void setIndex(int row);
    void moveClipUp();
    void moveClipDown();
    void on_actionOpen_triggered();
    void on_actionInsertCut_triggered();
    void on_actionInsertBlank_triggered();
    void appendCut();
    void replaceCut();
    void removeCut();
    void addAll();
    void on_actionGoto_triggered();
    void removeAll();
    void setUpdateButtonEnabled(bool modified);
    void openClipAt(int row);
    void updateCut();
    void reevaluateClipActions();
    QVariantMap handleDrop(QVariantMap data);

private slots:
    void on_addButton_clicked();
    void on_actionAppendBlank_triggered();
    void onPlaylistCreated();
    void onPlaylistLoaded();
    void onPlaylistCleared();
    void onMoveClip(int from, int to);

private:
    PlaylistModel m_model;
    QQuickWidget m_quickWidget;
    int m_defaultRowHeight;
};

#endif // PLAYLISTDOCK_H
