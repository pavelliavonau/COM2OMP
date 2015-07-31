#include "Com2OmpWizard.h"
#include "chooseclassespage.h"
#include "chooseidlfilespage.h"
#include "maincalculationthread.h"
#include "mainprocesspage.h"
#include "wizardfields.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QVariant>
#include <QAbstractButton>
#include <QTextStream>
#include <QRegularExpression>
#include <QTextEdit>
#include <QMessageBox>

COM2OMPWizard::COM2OMPWizard()
{
    setPage(INTRO_PAGE       , createIntroPage());
    setPage(SELECT_IDL_PAGE  , new ChooseIdlFilesPage());
    setPage(CHOOSE_COCLS_PAGE, new ChooseClassesPage());
    setPage(MAIN_PROCESS_PAGE, new MainProcessPage());
    setPage(FINAL_PAGE       , createConclusionPage());

    connect(this,&COM2OMPWizard::currentIdChanged, this, &COM2OMPWizard::onCurrentIdChanged);
}

QWizardPage * COM2OMPWizard::createIntroPage()
{
    QWizardPage *page = new QWizardPage;
    page->setTitle(tr("Introduction"));

    QLabel *label = new QLabel(tr("This wizard will help you convert "
                                  "COM objects into OMP objects."));
    label->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    page->setLayout(layout);

    return page;
}

QWizardPage * COM2OMPWizard::createConclusionPage()
{
    QWizardPage *page = new QWizardPage;
    page->setTitle(tr("Conclusion"));
    QLabel *label = new QLabel(tr("You are now finished. Have a nice day!"));
    label->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    page->setLayout(layout);

    return page;
}

void COM2OMPWizard::onCurrentIdChanged(int id)
{
    switch(id)
    {
        case INTRO_PAGE:
        case SELECT_IDL_PAGE: return;

        case CHOOSE_COCLS_PAGE:
            {
                ChooseClassesPage *p = dynamic_cast<ChooseClassesPage*>(page(CHOOSE_COCLS_PAGE));
                if(p)
                {
                    auto classses_if_IDL = findAllClassesInIDL( field(CLS_IDL_FILENAME).toString() );

                    p->fillTable(classses_if_IDL);
                }
                return;
            }
        case MAIN_PROCESS_PAGE:
            {
                ChooseClassesPage *p = dynamic_cast<ChooseClassesPage*>(page(CHOOSE_COCLS_PAGE));

                mainProcessing(p->getSelectedCoClasses(), field(CLS_IDL_FILENAME).toString(), field(DATA_IDL_FILENAME).toString());

                return;
            }
        case FINAL_PAGE: button(QWizard::BackButton)->setEnabled(false); return;

        default: return;
    }
}

QVector<ComClassData> COM2OMPWizard::findAllClassesInIDL(QString idl_file_name)
{
    m_all_classes_in_IDL.empty();

    QFile file(idl_file_name);

    file.open(QIODevice::ReadOnly|QIODevice::Text);

    QByteArray array = file.readAll();
    QTextStream stream(array, QIODevice::ReadOnly|QIODevice::Text);
    QString file_string;
    file_string.append(stream.readAll());

    file.close();

    QRegularExpression coclass_definition_regex("(\\/\\/[^\\/;]*)*\\[([^\\]]*\\]){1}\\s+coclass (?<classname>\\w+)\\s+\\{\\s+(?<default>\\[default\\])(\\s+interface (?<interface>\\w+)\\s*;)+\\s+\\};");
    QRegularExpressionMatchIterator iter = coclass_definition_regex.globalMatch(file_string);

    while( iter.hasNext() )
    {
        QRegularExpressionMatch coclass_definition_match = iter.next();
        ComClassData COM_class;
        COM_class.idl_name = coclass_definition_match.captured("classname");

        COM_class.idl_definition = coclass_definition_match.captured(0);
        QRegularExpression interface_regex("(?<default>\\[default\\])?(\\s+interface (?<interface>\\w+)\\s*;)");

        QRegularExpressionMatchIterator interface_iter = interface_regex.globalMatch(COM_class.idl_definition);

        while( interface_iter.hasNext() )
        {
            QRegularExpressionMatch interface_match = interface_iter.next();
            InterfaceData interface;
            interface.is_default = !interface_match.captured("default").isEmpty();
            interface.name = interface_match.captured("interface");
            COM_class.interfaces.push_back(interface);
        }
        QRegularExpressionMatch CLSID_match;
        if(COM_class.idl_definition.contains(QRegularExpression("uuid\\((?<CLSID>.*)\\)"), &CLSID_match))
            COM_class.CLSID = "{" + CLSID_match.captured("CLSID") + "}";

        m_all_classes_in_IDL << COM_class;
    }

    return m_all_classes_in_IDL;
}

bool COM2OMPWizard::mainProcessing(QVector<QString> selected_classes, QString cls_IDL_filename, QString data_IDL_filename)
{
    MainProcessPage *main_page = dynamic_cast<MainProcessPage*>(page(COM2OMPWizard::MAIN_PROCESS_PAGE));

    button(QWizard::NextButton)->setEnabled(false);
    button(QWizard::BackButton)->setEnabled(false);

    MainCalculationThread* calc = new MainCalculationThread(this);

    connect(calc, SIGNAL(appendToLog(QString)),main_page->m_log_widget,SLOT(append(QString)),    Qt::QueuedConnection);
    connect(calc, SIGNAL(askUserYesOrNo(QString)),                     SLOT(onAskYesNo(QString)),Qt::QueuedConnection);
    connect(this, SIGNAL(sendYesNo(bool)),     calc,                   SLOT(answerUserYesOrNoSlot(bool)));
    connect(calc, SIGNAL(finished()),                                  SLOT(onEnableNext()),     Qt::QueuedConnection);

    calc->setCalculationData(m_all_classes_in_IDL, selected_classes, cls_IDL_filename, data_IDL_filename);

    calc->start();
    return true;
}

void COM2OMPWizard::onAskYesNo(QString msg)
{
    int res = QMessageBox::question( this, "Answer, please.", msg, QMessageBox::Yes, QMessageBox::No );
    emit sendYesNo( QMessageBox::Yes == res );
}

void COM2OMPWizard::onEnableNext()
{
    button(QWizard::NextButton)->setEnabled(true);
}

