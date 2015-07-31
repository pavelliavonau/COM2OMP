#ifndef MAINPROCESSPAGE_H
#define MAINPROCESSPAGE_H

#include <QWizard>

class QTextEdit;
class QProgressBar;

class MainProcessPage : public QWizardPage
{
public:
    MainProcessPage();

    QTextEdit* m_log_widget;
    QProgressBar* m_progress_bar;
};

#endif // MAINPROCESSPAGE_H
