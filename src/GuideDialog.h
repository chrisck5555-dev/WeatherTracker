#pragma once

#include <QDialog>

// The built-in user guide: the main functions, in plain words.
class GuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GuideDialog(QWidget *parent = nullptr);
};
