#pragma once

#include <QDialog>

namespace Ui {
class InformationDialog;
}

class InformationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InformationDialog(QWidget* parent = nullptr);
    ~InformationDialog();
    void setInformationText(const QString& text);

private:
    Ui::InformationDialog *ui;
};
