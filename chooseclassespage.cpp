#include "chooseclassespage.h"

#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>

ChooseClassesPage::ChooseClassesPage()
{
    setSubTitle(tr("Choose classes for convertion."));
    table = new QTableWidget(0,2);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::MultiSelection);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(table);
    setLayout(layout);
}

void ChooseClassesPage::fillTable(const QVector<ComClassData> &vector)
{
    table->clear();
    table->setHorizontalHeaderLabels(QStringList() << tr("Class") << tr("Interfaces") );
    table->setRowCount(vector.size());

    uint row_number = 0;
    for(auto cls : vector)
    {
        auto item = new QTableWidgetItem(cls.idl_name);

        QString interfaces_str;

        for(auto iface : cls.interfaces)
        {
            interfaces_str.append( iface.name + "\n" );
        }

        table->setItem(row_number, 0, item);
        table->setItem(row_number, 1, new QTableWidgetItem(interfaces_str));

        row_number++;
    }

    table->resizeColumnsToContents();
    table->resizeRowsToContents();
}

QVector<QString> ChooseClassesPage::getSelectedCoClasses()
{
    QVector<QString> selected_classes;

    int row_count = table->rowCount();
    for( int row = 0; row < row_count; ++row )
    {
        QTableWidgetItem* item = table->item(row, 0);
        if(item->isSelected())
            selected_classes.push_back(item->data(Qt::DisplayRole).toString());
    }

    return selected_classes;
}
