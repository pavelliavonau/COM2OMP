#ifndef COM2OMPWIZARD_H
#define COM2OMPWIZARD_H

#include "coclassdata.h"

#include <QWizard>

class QWizardPage;

class COM2OMPWizard : public QWizard
{
    Q_OBJECT
public:
    COM2OMPWizard();

    enum PAGES {
        INTRO_PAGE = 0,
        SELECT_IDL_PAGE,
        CHOOSE_COCLS_PAGE,
        MAIN_PROCESS_PAGE,
        FINAL_PAGE,
    };

signals:
    void sendYesNo(bool);

private slots:
    void onAskYesNo(QString msg);
    void onEnableNext();
    void onCurrentIdChanged(int id);

private:
    QWizardPage* createIntroPage();
    QWizardPage* createConclusionPage();

    QVector<ComClassData> findAllClassesInIDL(QString idl_file_name);
    bool mainProcessing(QVector<QString> selected_classes, QString cls_IDL_filename, QString data_IDL_filename);

    QVector<ComClassData> m_all_classes_in_IDL;
};

#endif // COM2OMPWIZARD_H
