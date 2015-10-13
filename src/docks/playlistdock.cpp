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

#include "playlistdock.h"
#include "dialogs/durationdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "qmltypes/qmlutilities.h"
#include "shotcut_mlt_properties.h"
#include <commands/playlistcommands.h>
#include <QMenu>
#include <QDebug>
#include <QQmlContext>
#include <QCursor>
#include <QGuiApplication>
#include <QQuickItem>

PlaylistDock::PlaylistDock(QWidget *parent) :
    QDockWidget(parent),
    m_quickWidget(QmlUtilities::sharedEngine(), this)
{
    qDebug() << "begin";
    setObjectName("Playlist");
    setWindowTitle(tr("Playlist"));

    connect(&m_model, SIGNAL(cleared()), this, SLOT(onPlaylistCleared()));
    connect(&m_model, SIGNAL(created()), this, SLOT(onPlaylistCreated()));
    connect(&m_model, SIGNAL(loaded()), this, SLOT(onPlaylistLoaded()));
    connect(&m_model, SIGNAL(modified()), this, SLOT(onPlaylistLoaded()));
    connect(&m_model, SIGNAL(moveClip(int,int)), SLOT(onMoveClip(int,int)));

    QDir sourcePath = QmlUtilities::qmlDir();
    sourcePath.cd("playlist");

    m_quickWidget.rootContext()->setContextProperty("playlistmodel", &m_model);
    m_quickWidget.rootContext()->setContextProperty("playlistdock", this);
    m_quickWidget.rootContext()->setContextProperty("playlistwidget", &m_quickWidget);
    m_quickWidget.rootContext()->setContextProperty("playlistwindow", m_quickWidget.quickWindow());
    m_quickWidget.setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget.setSource(QUrl::fromLocalFile(sourcePath.absoluteFilePath("playlist.qml")));

    setWidget(&m_quickWidget);
    setFocusProxy(&m_quickWidget);
    qDebug() << "end";
}

PlaylistDock::~PlaylistDock()
{
}

int PlaylistDock::position()
{
    int result = -1;
    if (currentIndex() != -1 && m_model.playlist()) {
        Mlt::ClipInfo* i = m_model.playlist()->clip_info(currentIndex());
        if (i) result = i->start;
        delete i;
    }
    return result;
}

int PlaylistDock::currentIndex() const
{
    QVariant idx = -1;
    QMetaObject::invokeMethod(m_quickWidget.rootObject(), "index", Qt::DirectConnection, Q_RETURN_ARG(QVariant, idx));
    Q_ASSERT(idx.type() == QVariant::Int);
    return idx.toInt();
}

QPoint PlaylistDock::widgetPos() const
{
    return m_quickWidget.mapToGlobal(QPoint());
}

int PlaylistDock::activeClipActions() const
{
    int actions = 0;
    if (currentIndex() > -1 && m_model.playlist()) {
        actions |= PlaylistDock::Goto;
        if (MLT.isClip())
            actions |= PlaylistDock::InsertCut | PlaylistDock::InsertBlank;
        actions |= PlaylistDock::Open;
        QScopedPointer<Mlt::ClipInfo> info(m_model.playlist()->clip_info(currentIndex()));
        if (info && MLT.producer()->get_int(kPlaylistIndexProperty) == currentIndex() + 1) {
            if (MLT.producer()->get_in() != info->frame_in || MLT.producer()->get_out() != info->frame_out)
                actions |= PlaylistDock::Update;
        }
        actions |= PlaylistDock::Remove;
    }
    return actions;
}

void PlaylistDock::startEditingComment()
{
    QMetaObject::invokeMethod(m_quickWidget.rootObject(), "startEditingComment", Qt::DirectConnection);
}

void PlaylistDock::incrementIndex()
{
    QMetaObject::invokeMethod(m_quickWidget.rootObject(), "incrementIndex");
    currentIndex();
}

void PlaylistDock::decrementIndex()
{
    QMetaObject::invokeMethod(m_quickWidget.rootObject(), "decrementIndex");
}

void PlaylistDock::setIndex(int row)
{
    QMetaObject::invokeMethod(m_quickWidget.rootObject(), "decrementIndex", Q_ARG(int, row));
}

void PlaylistDock::moveClipUp()
{
    int row = currentIndex();
    if (row > 0)
        MAIN.undoStack()->push(new Playlist::MoveCommand(m_model, row, row - 1));
}

void PlaylistDock::moveClipDown()
{
    int row = currentIndex();
    if (row + 1 < m_model.rowCount())
        MAIN.undoStack()->push(new Playlist::MoveCommand(m_model, row, row + 1));
}

void PlaylistDock::on_actionInsertCut_triggered()
{
    if (MLT.isClip() || MLT.savedProducer()) {
        QString xml = MLT.XML(MLT.isClip()? 0 : MLT.savedProducer());
        MAIN.undoStack()->push(new Playlist::InsertCommand(m_model, xml, currentIndex()));
    }
}

void PlaylistDock::appendCut()
{
    if (MLT.producer() && MLT.producer()->is_valid()) {
        if (!MLT.isClip()) {
            emit showStatusMessage(tr("You cannot insert a playlist into a playlist!"));
        } else if (MLT.isSeekable()) {
            MAIN.undoStack()->push(new Playlist::AppendCommand(m_model, MLT.XML()));
            MLT.producer()->set(kPlaylistIndexProperty, m_model.playlist()->count());
            setUpdateButtonEnabled(true);
        } else {
            DurationDialog dialog(this);
            dialog.setDuration(MLT.profile().fps() * 5);
            if (dialog.exec() == QDialog::Accepted) {
                MLT.producer()->set_in_and_out(0, dialog.duration() - 1);
                if (MLT.producer()->get("mlt_service") && !strcmp(MLT.producer()->get("mlt_service"), "avformat"))
                    MLT.producer()->set("mlt_service", "avformat-novalidate");
                MAIN.undoStack()->push(new Playlist::AppendCommand(m_model, MLT.XML()));
            }
        }
    }
    else {
        MAIN.openVideo();
    }
}

void PlaylistDock::on_actionInsertBlank_triggered()
{
    DurationDialog dialog(this);
    dialog.setDuration(MLT.profile().fps() * 5);
    if (dialog.exec() == QDialog::Accepted) {
        int idx = currentIndex();
        if (idx > 0)
            m_model.insertBlank(dialog.duration(), idx);
        else
            m_model.appendBlank(dialog.duration());
    }
}

void PlaylistDock::on_actionAppendBlank_triggered()
{
    DurationDialog dialog(this);
    dialog.setDuration(MLT.profile().fps() * 5);
    if (dialog.exec() == QDialog::Accepted)
        m_model.appendBlank(dialog.duration());
}

void PlaylistDock::replaceCut()
{
    int row = currentIndex();
    if (row == -1 || !m_model.playlist()) return;
    Mlt::ClipInfo* info = m_model.playlist()->clip_info(row);
    if (!info) return;
    if (MLT.producer()->type() != playlist_type) {
        if (MLT.isSeekable()) {
            MAIN.undoStack()->push(new Playlist::UpdateCommand(m_model, MLT.XML(), row));
            MLT.producer()->set(kPlaylistIndexProperty, row + 1);
            setUpdateButtonEnabled(true);
        }
        else {
            // change the duration
            DurationDialog dialog(this);
            dialog.setDuration(info->frame_count);
            if (dialog.exec() == QDialog::Accepted) {
                MLT.producer()->set_in_and_out(0, dialog.duration() - 1);
                if (MLT.producer()->get("mlt_service") && !strcmp(MLT.producer()->get("mlt_service"), "avformat"))
                    MLT.producer()->set("mlt_service", "avformat-novalidate");
                MAIN.undoStack()->push(new Playlist::UpdateCommand(m_model, MLT.XML(), row));
            }
        }
    }
    else {
        emit showStatusMessage(tr("You cannot insert a playlist into a playlist!"));
        setUpdateButtonEnabled(false);
    }
    delete info;
}

void PlaylistDock::removeCut()
{
    int idx = currentIndex();
    if (idx == -1 || !m_model.playlist()) return;
    MAIN.undoStack()->push(new Playlist::RemoveCommand(m_model, idx));
    int count = m_model.playlist()->count();
    if (count == 0) return;
    int i = idx >= count ? count-1 : idx;
    QScopedPointer<Mlt::ClipInfo> info(m_model.playlist()->clip_info(i));
    if (info) {
        emit itemActivated(info->start);
        int j = MLT.producer()->get_int(kPlaylistIndexProperty);
        if (j > i + 1) {
            MLT.producer()->set(kPlaylistIndexProperty, j - 1);
        } else if (j == i + 1) {
            // Remove the playlist index property on the producer.
            MLT.producer()->set(kPlaylistIndexProperty, 0, 0);
            setUpdateButtonEnabled(false);
        }
    }
}

void PlaylistDock::setUpdateButtonEnabled(bool modified)
{
    m_quickWidget.rootObject()->setProperty("updateButtonEnabled", QVariant::fromValue(modified));
}

void PlaylistDock::on_actionOpen_triggered()
{
    int idx = currentIndex();
    if (idx == -1 || !m_model.playlist()) return;
    Mlt::ClipInfo* i = m_model.playlist()->clip_info(idx);
    if (i) {
        QString xml = MLT.XML(i->producer);
        Mlt::Producer* p = new Mlt::Producer(MLT.profile(), "xml-string", xml.toUtf8().constData());
        p->set_in_and_out(i->frame_in, i->frame_out);
        p->set(kPlaylistIndexProperty, idx + 1);
        emit clipOpened(p);
        delete i;
    }
}

void PlaylistDock::openClipAt(int row)
{
    if (!m_model.playlist()) return;
    Mlt::ClipInfo* i = m_model.playlist()->clip_info(row);
    if (i) {
        if (qApp->keyboardModifiers() == Qt::ShiftModifier) {
            emit itemActivated(i->start);
        } else {
            QString xml = MLT.XML(i->producer);
            Mlt::Producer* p = new Mlt::Producer(MLT.profile(), "xml-string", xml.toUtf8().constData());
            p->set_in_and_out(i->frame_in, i->frame_out);
            p->set(kPlaylistIndexProperty, row + 1);
            emit clipOpened(p);
        }
        delete i;
    } else {
        MAIN.openVideo();
    }
}

void PlaylistDock::on_actionGoto_triggered()
{
    Mlt::ClipInfo* i = m_model.playlist()->clip_info(currentIndex());
    if (i) {
        emit itemActivated(i->start);
        delete i;
    }
}

void PlaylistDock::removeAll()
{
    MAIN.undoStack()->push(new Playlist::ClearCommand(m_model));
}

void PlaylistDock::onPlaylistCreated()
{
    return;
    /* ui->removeButton->setEnabled(true); */
    /* ui->updateButton->setEnabled(false); */
    /* ui->menuButton->setEnabled(true); */
    /* ui->stackedWidget->setCurrentIndex(1); */
}

void PlaylistDock::onPlaylistLoaded()
{
    return;
    /* onPlaylistCreated(); */
    /* ui->tableView->resizeColumnsToContents(); */
}

void PlaylistDock::onPlaylistCleared()
{
    return;
    /* ui->removeButton->setEnabled(false); */
    /* ui->updateButton->setEnabled(false); */
    /* ui->menuButton->setEnabled(false); */
    /* ui->stackedWidget->setCurrentIndex(0); */
}

QVariantMap PlaylistDock::handleDrop(QVariantMap data)
{
    Q_ASSERT(data.contains("urls"));
    Q_ASSERT(data.contains("index"));
    Q_ASSERT(data.contains("mimeData"));

    QVariantMap result;
    QList<QUrl> urls = data.value("urls").value<QList<QUrl> >();
    int index = data.value("index").toInt();
    QVariantMap mimeData = data.value("mimeData").toMap();
    if (mimeData.contains("application/x-qabstractitemmodeldatalist")) {
        QByteArray encoded = mimeData.value("application/x-qabstractitemmodeldatalist").toByteArray();
        QDataStream stream(&encoded, QIODevice::ReadOnly);
        QMap<int,  QVariant> roleDataMap;
        while (!stream.atEnd()) {
            int row, col;
            stream >> row >> col >> roleDataMap;
        }
        if (roleDataMap.contains(Qt::ToolTipRole)) {
            QMimeData *mimeData = new QMimeData;
            urls.clear();
            // DisplayRole is just basename, ToolTipRole contains full path
            urls.append(roleDataMap[Qt::ToolTipRole].toUrl());
        }
    }
    else if (mimeData.contains("application/playlist-rowindex")) {
        int from = mimeData.value("application/playlist-rowindex").toInt();
        if (from != index && index != from + 1)
            MAIN.undoStack()->push(new Playlist::MoveCommand(m_model, from, index));
        return result;
    }
    if (!urls.isEmpty()) {
        int insertNextAt = index;
        foreach (QUrl url, urls) {
            QString path = MAIN.removeFileScheme(url);
            Mlt::Producer p(MLT.profile(), path.toUtf8().constData());
            if (p.is_valid()) {
                // Convert avformat to avformat-novalidate so that XML loads faster.
                if (!qstrcmp(p.get("mlt_service"), "avformat")) {
                    p.set("mlt_service", "avformat-novalidate");
                    p.set("mute_on_pause", 0);
                }
                MLT.setImageDurationFromDefault(&p);
                if (index == -1)
                    MAIN.undoStack()->push(new Playlist::AppendCommand(m_model, MLT.XML(&p)));
                else
                    MAIN.undoStack()->push(new Playlist::InsertCommand(m_model, MLT.XML(&p), insertNextAt++));
            }
        }
    }
    else if (mimeData.contains("application/mlt+xml")) {
        QString xml = mimeData.value("application/mlt+xml").toString();
        if (MLT.producer() && MLT.producer()->is_valid()) {
            if (MLT.producer()->type() == playlist_type) {
                emit showStatusMessage(tr("You cannot insert a playlist into a playlist!"));
            } else if (MLT.isSeekable()) {
                if (index == -1) {
                    MAIN.undoStack()->push(new Playlist::AppendCommand(m_model, xml));
                    MLT.producer()->set(kPlaylistIndexProperty, m_model.playlist()->count());
                } else {
                    MAIN.undoStack()->push(new Playlist::InsertCommand(m_model, xml, index));
                    MLT.producer()->set(kPlaylistIndexProperty, index + 1);
                }
                setUpdateButtonEnabled(true);
            } else {
                DurationDialog dialog(this);
                dialog.setDuration(MLT.profile().fps() * 5);
                if (dialog.exec() == QDialog::Accepted) {
                    MLT.producer()->set_in_and_out(0, dialog.duration() - 1);
                    if (MLT.producer()->get("mlt_service") && !strcmp(MLT.producer()->get("mlt_service"), "avformat"))
                        MLT.producer()->set("mlt_service", "avformat-novalidate");
                    if (index == -1)
                        MAIN.undoStack()->push(new Playlist::AppendCommand(m_model, MLT.XML()));
                    else
                        MAIN.undoStack()->push(new Playlist::InsertCommand(m_model, MLT.XML(), index));
                }
            }
        }
    }
    return result;
}

void PlaylistDock::onMoveClip(int from, int to)
{
    if (from == to || to == from + 1)
        return;
    MAIN.undoStack()->push(new Playlist::MoveCommand(m_model, from, to));
}

void PlaylistDock::on_addButton_clicked()
{
    appendCut();
}

void PlaylistDock::addAll()
{
    emit addAllTimeline(m_model.playlist());
}

void PlaylistDock::updateCut()
{
    int index = MLT.producer()->get_int(kPlaylistIndexProperty);
    if (index > 0 && index <= m_model.playlist()->count()) {
        MAIN.undoStack()->push(new Playlist::UpdateCommand(m_model, MLT.XML(), index - 1));
        setUpdateButtonEnabled(false);
    }
}

void PlaylistDock::reevaluateClipActions()
{
    emit clipActionsEvaluationRequested();
}
