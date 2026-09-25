#include "dlgdbupdate.h"
#include "ui_dlgdbupdate.h"
#include <qscrollbar.h>

DlgDbUpdate::DlgDbUpdate(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgDbUpdate)
{
    ui->setupUi(this);
    reset();
    m_logFlushTimer.start(200, this);
}

DlgDbUpdate::~DlgDbUpdate()
{
    delete ui;
}

void DlgDbUpdate::addLogMsg(const QString& msg)
{
    m_log += msg;
    m_log += "\n";
}

void DlgDbUpdate::changeStatusTxt(QString txt)
{
    ui->lblCurrentActivity->setText(txt);
}

void DlgDbUpdate::changeProgress(int progress, int max)
{
    if (max <= 0) {
        // No total known yet (still searching for files) - a busy indicator is more honest
        // than a bar sitting at zero.
        ui->progressBar->setFormat("Working...");
        ui->progressBar->setRange(0, 0);
        return;
    }
    ui->progressBar->setFormat("%v of %m  (%p%)");
    ui->progressBar->setRange(0, std::max(max, progress));
    ui->progressBar->setValue(progress);
}

void DlgDbUpdate::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_logFlushTimer.timerId()) {
        if (!m_log.isEmpty()) {
            ui->txtLog->appendPlainText(m_log);
            m_log.clear();
        }
    } else {
        QWidget::timerEvent(event);
    }
}

void DlgDbUpdate::reset()
{
    m_log.clear();
    ui->progressBar->setFormat("%p%");
    ui->progressBar->setRange(0, 1);
    ui->progressBar->setValue(0);
    ui->txtLog->clear();
    ui->lblCurrentActivity->setText("");

}
