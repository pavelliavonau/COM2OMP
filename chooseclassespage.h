#ifndef CHOOSECLASSESPAGE_H
#define CHOOSECLASSESPAGE_H

#include "coclassdata.h"

#include <QWizard>

class QTableWidget;

class ChooseClassesPage : public QWizardPage
{
    Q_OBJECT
public:
    ChooseClassesPage();

    void fillTable(const QVector<ComClassData>& vector);
    QVector<QString> getSelectedCoClasses();

private:
    QTableWidget *table;
};

#endif // CHOOSECLASSESPAGE_H
