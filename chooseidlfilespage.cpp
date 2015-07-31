#include "chooseidlfilespage.h"
#include "wizardfields.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>

ChooseIdlFilesPage::ChooseIdlFilesPage()
{
    setTitle(tr("Introduction"));

    QLabel *label_cls = new QLabel(tr("Choose IDL file with definitions of COM-classes."));
    label_cls->setWordWrap(true);

    QLineEdit * cls_edit = new QLineEdit;
    cls_edit->setEnabled(false);
    registerField(CLS_IDL_FILENAME, cls_edit);

    QToolButton* choose_cls_button = new QToolButton;
    connect(choose_cls_button,&QToolButton::clicked,[this,cls_edit]
    {
        QString file = QFileDialog::getOpenFileName(this,tr("COM-classes"),"D:\\PrjVC\\Omp8\\_OmIdl\\","*.idl");
        cls_edit->setText(file);
    });

    QLabel *label_type = new QLabel(tr("Choose IDL file with defintions of types of data."));
    label_type->setWordWrap(true);

    QLineEdit * data_types_edit = new QLineEdit;
    data_types_edit->setEnabled(false);
    registerField(DATA_IDL_FILENAME, data_types_edit);

    QToolButton* chooseDataButton = new QToolButton;
    connect(chooseDataButton,&QToolButton::clicked,[this,data_types_edit]
    {
        QString file = QFileDialog::getOpenFileName(this,tr("Types of data"),"D:\\PrjVC\\Omp8\\_OmIdl\\","*.idl");
        data_types_edit->setText(file);
    });

    QGridLayout *page_layout = new QGridLayout;
    page_layout->addWidget(label_cls,0,0);
    page_layout->addWidget(cls_edit,0,1);
    page_layout->addWidget(choose_cls_button,0,2);
    page_layout->addWidget(label_type,1,0);
    page_layout->addWidget(data_types_edit,1,1);
    page_layout->addWidget(chooseDataButton,1,2);
    setLayout(page_layout);
}
