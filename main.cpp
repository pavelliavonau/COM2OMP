#include "Com2OmpWizard.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name());
    app.installTranslator(&qtTranslator);

    QTranslator myappTranslator;
    myappTranslator.load("com2omp_" + QLocale::system().name());
    app.installTranslator(&myappTranslator);

    COM2OMPWizard wizard;

    wizard.setWindowTitle("COM2OMPWizard");
    wizard.show();

    return app.exec();
}
