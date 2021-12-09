#include "InformationDialog.h"
#include "ui_InformationDialog.h"
#include <QIcon>

InformationDialog::InformationDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::InformationDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

InformationDialog::~InformationDialog()
{
    delete ui;
}

void InformationDialog::setInformationText(const QString& text)
{
    ui->informationTextEdit->setPlainText(text);
}
