/*
 * Copyright (c) 2013-2019 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
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

#include "bmdbupdatethread.h"
#include <QDir>
#include <QDirIterator>
#include <QSqlQuery>
#include <QFileInfo>
#include <QApplication>
#include <QElapsedTimer>
#include "tagreader.h"
#include <QtConcurrent>

BmDbUpdateThread::BmDbUpdateThread(QObject *parent) :
    QThread(parent)
{
    supportedExtensions.append(".mp3");
    supportedExtensions.append(".wav");
    supportedExtensions.append(".ogg");
    supportedExtensions.append(".flac");
    supportedExtensions.append(".m4a");
    supportedExtensions.append(".mkv");
    supportedExtensions.append(".avi");
    supportedExtensions.append(".mp4");
    supportedExtensions.append(".mpg");
    supportedExtensions.append(".mpeg");
    supportedExtensions.append(".wmv");
    supportedExtensions.append(".wma");
}

QString BmDbUpdateThread::path() const
{
    return m_path;
}

void BmDbUpdateThread::setPath(const QString &path)
{
    m_path = path;
}

QStringList BmDbUpdateThread::findMediaFiles(const QString& directory)
{
    QStringList files;
    QDir dir(directory);
    QDirIterator iterator(dir.absolutePath(), QDirIterator::Subdirectories);
    // Searching a large or networked folder takes a while, so keep the dialog showing what's
    // happening.  The progress bar has no total to work with yet, so it runs as a busy indicator.
    QElapsedTimer guiTimer;
    guiTimer.start();
    emit progressChanged(0, 0);
    while (iterator.hasNext()) {
        iterator.next();
        if (!iterator.fileInfo().isDir()) {
            QString filename = iterator.filePath();
            for (int i=0; i<supportedExtensions.size(); i++)
            {
                if (filename.endsWith(supportedExtensions.at(i),Qt::CaseInsensitive))
                {
                    files.append(filename);
                    break;
                }
            }
        }
        if (guiTimer.elapsed() > 200) {
            guiTimer.restart();
            emit stateChanged(QString("Searching for media files in %1\n    %2 found so far...")
                                      .arg(directory)
                                      .arg(files.size()));
            QApplication::processEvents();
        }
    }
    return files;
}

void BmDbUpdateThread::run()
{
    database.open();
    qInfo() << database.lastError();
    TagReader reader;
    emit progressChanged(0, 0);
    emit progressMessage("Getting list of files in " + m_path);
    emit stateChanged("Finding media files...");
    QStringList files = findMediaFiles(m_path);
    emit progressMessage("Found " + QString::number(files.size()) + " files.");
    QSqlQuery query(database);
    emit stateChanged("Getting metadata and adding songs to the database");
    emit progressMessage("Getting metadata and adding songs to the database");
    qInfo() << "Setting sqlite synchronous mode to OFF";
    query.exec("PRAGMA synchronous=OFF");
    qInfo() << query.lastError();
    qInfo() << "Increasing sqlite cache size";
    query.exec("PRAGMA cache_size=500000");
    qInfo() << query.lastError();
    query.exec("PRAGMA temp_store=2");
    qInfo() << "Beginning transaction";
    query.exec("BEGIN TRANSACTION");
    qInfo() << query.lastError();
    query.prepare("INSERT OR IGNORE INTO bmsongs (artist,title,path,filename,duration,searchstring) VALUES(:artist, :title, :path, :filename, :duration, :searchstring)");
    for (int i=0; i < files.size(); i++)
    {
        QFileInfo fi(files.at(i));
        emit progressMessage("Processing file: " + fi.fileName());
        reader.setMedia(files.at(i));
        QString duration = QString::number(reader.getDuration() / 1000);
        QString artist = reader.getArtist();
        QString title = reader.getTitle();
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", files.at(i));
        query.bindValue(":filename", files.at(i));
        query.bindValue(":duration", duration);
        query.bindValue(":searchstring", artist + title + files.at(i));
        query.exec();
        emit progressChanged(i + 1, files.size());
    }
    query.exec("COMMIT TRANSACTION");
    qInfo() << query.lastError();
    emit progressMessage("Finished processing files for directory: " + m_path);
    database.close();
}

QSet<QString> BmDbUpdateThread::existingPathsUnder(const QString &directory, QSqlDatabase db)
{
    // Paths already in the break music DB for this directory, so an update only reads
    // tags/durations for files that are actually new instead of re-probing the whole library
    // (the INSERT OR IGNORE used to throw that work away for every existing song).
    QSet<QString> paths;
    QString prefix = QDir(directory).absolutePath();
    if (!prefix.endsWith('/'))
        prefix += '/';
    QSqlQuery query(db);
    query.setForwardOnly(true);
    // substr() instead of LIKE: LIKE treats '_' and '%' in folder names as wildcards.
    query.prepare("SELECT path FROM bmsongs WHERE substr(path, 1, :len) = :prefix");
    query.bindValue(":len", prefix.length());
    query.bindValue(":prefix", prefix);
    if (query.exec()) {
        while (query.next())
            paths.insert(query.value(0).toString());
    }
    return paths;
}

void BmDbUpdateThread::startUnthreaded()
{
    TagReader reader;
    emit progressChanged(0, 0);
    emit progressMessage("Getting list of files in " + m_path);
    emit stateChanged("Finding media files...");
    QApplication::processEvents();
    const QStringList allFiles = findMediaFiles(m_path);
    emit stateChanged(QString("Checking %1 files against the database...").arg(allFiles.size()));
    QApplication::processEvents();
    const QSet<QString> existing = existingPathsUnder(m_path, QSqlDatabase::database());
    QStringList files;
    files.reserve(allFiles.size());
    for (const auto &f : allFiles) {
        if (!existing.contains(f))
            files.append(f);
    }
    emit progressMessage("Found " + QString::number(allFiles.size()) + " files, " + QString::number(files.size()) + " new.");
    if (files.isEmpty()) {
        emit stateChanged(QString("No new files found in %1\n    %2 already in the database")
                                  .arg(m_path)
                                  .arg(allFiles.size()));
        emit progressChanged(1, 1);
        QApplication::processEvents();
        return;
    }
    QElapsedTimer guiTimer;
    guiTimer.start();
    QSqlQuery query;
    // The total is known now, so the bar can show real progress from the first file onwards.
    emit stateChanged(QString("Reading tags and adding songs to the database\n    0 of %1").arg(files.size()));
    emit progressChanged(0, files.size());
    emit progressMessage("Getting metadata and adding songs to the database");
    QApplication::processEvents();
    qInfo() << "Setting sqlite synchronous mode to OFF";
    query.exec("PRAGMA synchronous=OFF");
    qInfo() << query.lastError();
    qInfo() << "Increasing sqlite cache size";
    query.exec("PRAGMA cache_size=500000");
    qInfo() << query.lastError();
    query.exec("PRAGMA temp_store=2");
    qInfo() << "Beginning transaction";
    // Note: the 'database' member is never opened for the unthreaded path, so database.transaction()
    // silently failed and every INSERT ran as its own transaction. Use the default connection instead.
    query.exec("BEGIN TRANSACTION");
    qInfo() << query.lastError();
    query.prepare("INSERT OR IGNORE INTO bmsongs (artist,title,path,filename,duration,searchstring) VALUES(:artist, :title, :path, :filename, :duration, :searchstring)");
    for (int i=0; i < files.size(); i++)
    {
        emit progressMessage("Adding: " + files.at(i));
        reader.setMedia(files.at(i));
        QString duration = QString::number(reader.getDuration() / 1000);
        QString artist = reader.getArtist();
        QString title = reader.getTitle();
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", files.at(i));
        query.bindValue(":filename", files.at(i));
        query.bindValue(":duration", duration);
        query.bindValue(":searchstring", artist + title + files.at(i));
        query.exec();
        if (guiTimer.elapsed() > 200) {
            guiTimer.restart();
            emit stateChanged(QString("Reading tags and adding songs to the database\n    %1 of %2")
                                      .arg(i + 1)
                                      .arg(files.size()));
            emit progressChanged(i + 1, files.size());
            QApplication::processEvents();
        }
    }
    emit stateChanged(QString("Saving %1 songs to the database...").arg(files.size()));
    emit progressChanged(files.size(), files.size());
    QApplication::processEvents();
    query.exec("COMMIT");
    qInfo() << query.lastError();
    emit progressMessage("Finished processing files for directory: " + m_path);
}
