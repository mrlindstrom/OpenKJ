#include "durationlazyupdater.h"

#include <QSqlQuery>
#include <utility>
#include <QVariant>
#include "mzarchive.h"
#include "karaokefileinfo.h"


void LazyDurationUpdateWorker::getDurations(const QStringList &files) {
    if (files.isEmpty())
        return;
    std::string m_loggingPrefix{"[LazyDurationThread]"};
    std::shared_ptr<spdlog::logger> logger;
    logger = spdlog::get("logger");
    logger->info("{} Starting scan", m_loggingPrefix);
    MzArchive archive;
    KaraokeFileInfo parser;
    for (const auto &path : files)
    {
        unsigned int duration = 0;
        if (path.endsWith(".zip", Qt::CaseInsensitive))
        {
            archive.setArchiveFile(path);
            duration = archive.getSongDuration();
        }
        if (duration == 0)
        {
            parser.setFile(path);
            duration = parser.getDuration();
        }
        if (duration == 0)
            logger->warn("{} Unable to get duration for file {}. - File is likely corrupted or invalid", m_loggingPrefix, path);
        else
            logger->trace("{} Got duration: {} for file: {}", m_loggingPrefix, duration, path);
        emit gotDuration(path, duration);
        if (QThread::currentThread()->isInterruptionRequested()) {
            logger->info("{} Scan interrupt requested", m_loggingPrefix);
            break;
        }
    }
    logger->info("{} Scan complete", m_loggingPrefix);
}

LazyDurationUpdateController::LazyDurationUpdateController(QObject *parent) : QObject(parent) {
    m_logger = spdlog::get("logger");
    auto *worker = new LazyDurationUpdateWorker;
    workerThread.setObjectName("DurationUpdater");
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &LazyDurationUpdateController::operate, worker, &LazyDurationUpdateWorker::getDurations);
    connect(worker, &LazyDurationUpdateWorker::gotDuration, this, &LazyDurationUpdateController::updateDbDuration);
    workerThread.start();
    workerThread.setPriority(QThread::IdlePriority);
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(1000);
    connect(&m_flushTimer, &QTimer::timeout, this, &LazyDurationUpdateController::flushPendingDurations);
}

LazyDurationUpdateController::~LazyDurationUpdateController() {
    workerThread.requestInterruption();
    workerThread.quit();
    workerThread.wait();
    flushPendingDurations();
}

void LazyDurationUpdateController::getSongsRequiringUpdate()
{
    m_logger->info("{} Finding songs with missing durations", m_loggingPrefix);
    files.clear();
    QSqlQuery query;
    // -2 means "not read yet" and -1 means "tried and failed", so a file whose duration can't be
    // read is not re-opened on every launch.  0 is what older versions stored for both cases, so
    // those get one more attempt and are then marked one way or the other.
    query.exec("SELECT path FROM dbsongs WHERE duration = -2 OR duration = 0 ORDER BY artist, title");
    files.reserve(query.size());
    while (query.next())
    {
        files.append(query.value(0).toString());
    }
    m_logger->info("{} Done, found {} songs with missing durations", m_loggingPrefix, files.size());
}

void LazyDurationUpdateController::stopWork()
{
    workerThread.requestInterruption();
}

void LazyDurationUpdateController::updateDbDuration(const QString& file, int duration)
{
    // Batch DB writes instead of one auto-committed UPDATE (i.e. one SQLite transaction) per song.
    // A failed read is stored as -1 so the file isn't probed again on the next run.
    m_pendingDurations.append(qMakePair(file, duration > 0 ? duration : -1));
    emit gotDuration(file, duration);
    if (m_pendingDurations.size() >= 250)
        flushPendingDurations();
    else if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void LazyDurationUpdateController::flushPendingDurations()
{
    m_flushTimer.stop();
    if (m_pendingDurations.isEmpty())
        return;
    QSqlQuery query;
    // SAVEPOINT (not BEGIN) so this nests safely if DbUpdater already has a transaction open
    // when this runs from inside one of its processEvents() calls.
    query.exec("SAVEPOINT lazyduration");
    query.prepare("UPDATE dbsongs SET duration = :duration WHERE path = :path");
    for (const auto &item : std::as_const(m_pendingDurations)) {
        query.bindValue(":path", item.first);
        query.bindValue(":duration", item.second);
        query.exec();
    }
    query.exec("RELEASE SAVEPOINT lazyduration");
    m_pendingDurations.clear();
}

void LazyDurationUpdateController::getDurations()
{
    getSongsRequiringUpdate();
    emit operate(files);
}
