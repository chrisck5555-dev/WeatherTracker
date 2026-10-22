#pragma once

#include <QList>
#include <QWidget>

#include "Records.h"

class AppState;
class Repository;

// The painted Records panel. Rows that point at a day open it when clicked.
class RecordsView : public QWidget
{
    Q_OBJECT

public:
    RecordsView(AppState &state, Repository &repo, QWidget *parent = nullptr);

signals:
    void dayActivated(const QDate &date);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void reload();
    int rowAt(const QPoint &pos) const;       // -1 when no row is there

    AppState &m_state;
    Repository &m_repo;
    QList<Records::Item> m_rows;
    int m_pressedRow = -1;
};
