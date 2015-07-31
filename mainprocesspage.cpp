#include "mainprocesspage.h"

#include <QProgressBar>
#include <QTextEdit>
#include <QVBoxLayout>

MainProcessPage::MainProcessPage()
{
    QLayout* lay = new QVBoxLayout(this);
    m_log_widget = new QTextEdit;
    m_log_widget->setReadOnly(true);
    lay->addWidget(m_log_widget);
    m_progress_bar = new QProgressBar;
    lay->addWidget(m_progress_bar);
    setLayout(lay);
}
