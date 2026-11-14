#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QVBoxLayout>
#include <QtTest>

#include "ChoiceGroup.h"
#include "ChoiceRow.h"
#include "EnterKey.h"

namespace {

QList<Choice> compassChoices()
{
    QList<Choice> l;
    for (const char *s : {"N", "NE", "E", "SE", "S", "SW", "W", "NW"})
        l << Choice{s, s, QString(), QString()};
    return l;
}

QList<Choice> named(const QStringList &names)
{
    QList<Choice> l;
    for (const QString &n : names)
        l << Choice{n, n, QString(), QString()};
    return l;
}

const QStringList kPrecip{"None", "Rain", "Drizzle", "Snow", "Sleet", "Freezing rain", "Hail", "Mixed"};
const QStringList kCloud{"Clear", "Mostly clear", "Partly cloudy", "Mostly cloudy", "Overcast"};

// A page like the editor's: a field, the groups, another field.
struct Page {
    QWidget w;
    QLineEdit *before, *after;
    ChoiceGroup *precip, *wind, *cloud;
    Page()
    {
        auto *v = new QVBoxLayout(&w);
        before = new QLineEdit;
        precip = new ChoiceGroup("Type", "Precipitation type", named(kPrecip), ChoiceGroup::Shape::Rows, 4);
        wind = new ChoiceGroup("Going to", "Wind direction, going to", compassChoices(), ChoiceGroup::Shape::Compass);
        cloud = new ChoiceGroup("Cloud cover", "Cloud cover", named(kCloud), ChoiceGroup::Shape::Rows, 5);
        after = new QLineEdit;
        for (QWidget *x : std::initializer_list<QWidget *>{before, precip, wind, cloud, after})
            v->addWidget(x, 0, Qt::AlignLeft);
        w.resize(800, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
    }
    QList<ChoiceGroup *> groups() const { return {precip, wind, cloud}; }
};

void press(QWidget *w, Qt::Key k) { QTest::keyClick(w, k); }

} // namespace

class TestChoiceGroup : public QObject
{
    Q_OBJECT

private slots:
    void cellsShareColumns()
    {
        Page page;
        int gridLeft = -1;
        for (ChoiceGroup *g : page.groups()) {
            for (ChoiceCell *c : g->cells()) {
                QCOMPARE(c->size(), QSize(108, 56));
                QCOMPARE(c->x(), c->column * (108 + 8));              // the same columns everywhere
                QCOMPARE(c->y(), c->row * (56 + 8));
            }
            const int left = g->cells().first()->parentWidget()->mapTo(&page.w, QPoint()).x();
            if (gridLeft < 0)
                gridLeft = left;
            QCOMPARE(left, gridLeft);                                   // and the same left edge
        }
    }

    void layouts()
    {
        Page page;
        QCOMPARE(page.precip->columns(), 4);
        QCOMPARE(page.precip->rows(), 2);
        QCOMPARE(page.cloud->columns(), 5);
        QCOMPARE(page.cloud->rows(), 1);
        QCOMPARE(page.wind->columns(), 3);
        QCOMPARE(page.wind->rows(), 3);
        // The compass: NW N NE / W . E / SW S SE, centre empty.
        auto at = [&](const QString &v) { auto *c = page.wind->cell(v); return QPoint(c->column, c->row); };
        QCOMPARE(at("NW"), QPoint(0, 0));
        QCOMPARE(at("N"), QPoint(1, 0));
        QCOMPARE(at("E"), QPoint(2, 1));
        QCOMPARE(at("S"), QPoint(1, 2));
        for (ChoiceCell *c : page.wind->cells())
            QVERIFY(QPoint(c->column, c->row) != QPoint(1, 1));
        QCOMPARE(ChoiceGroup::widthFor(5), 572);
        QCOMPARE(ChoiceGroup::widthFor(3), 340);
    }

    void allSwitchToThreeColumnsTogether()
    {
        Page page;
        ChoiceGroup::fitAll(page.groups(), 571);                       // one px short of the cloud row
        for (ChoiceGroup *g : page.groups()) {
            QVERIFY(g->isNarrow());
            QCOMPARE(g->columns(), 3);
        }
        QCOMPARE(page.precip->rows(), 3);                              // 3-3-2
        QCOMPARE(page.cloud->rows(), 2);                               // 3-2
        QCOMPARE(page.wind->rows(), 3);                                // the compass never changes
        QApplication::processEvents();
        for (ChoiceGroup *g : page.groups())
            for (ChoiceCell *c : g->cells())
                QVERIFY(c->x() + c->width() <= ChoiceGroup::widthFor(3));
        ChoiceGroup::fitAll(page.groups(), 572);
        for (ChoiceGroup *g : page.groups())
            QVERIFY(!g->isNarrow());
        QCOMPARE(page.cloud->columns(), 5);
    }

    void oneTabStop()
    {
        Page page;
        page.w.activateWindow();
        page.before->setFocus();
        // Nothing chosen: Clear is off, and Tab enters at the first cell, then leaves.
        QVERIFY(!page.precip->clearButton()->isEnabled());
        press(page.before, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), page.precip->cells().first());
        press(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), page.wind->cells().first());   // straight to the next group

        // Something chosen: Tab goes to Clear, then the chosen cell, then on.
        page.precip->setValue("Snow");
        page.before->setFocus();
        press(page.before, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), page.precip->clearButton());
        press(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), page.precip->cell("Snow"));
        press(QApplication::focusWidget(), Qt::Key_Tab);
        QVERIFY(!page.precip->isAncestorOf(QApplication::focusWidget()));
    }

    void arrows()
    {
        Page page;
        page.w.activateWindow();
        QSignalSpy changed(page.wind, &ChoiceGroup::changed);
        page.wind->setValue("N");
        page.wind->cell("N")->setFocus();
        // KB2: the arrows move only the focus; nothing is chosen on the way.
        press(page.wind->cell("N"), Qt::Key_Down);                     // over the empty centre
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("S"));
        press(QApplication::focusWidget(), Qt::Key_Right);
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("SE"));
        press(QApplication::focusWidget(), Qt::Key_Right);              // the last stays
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("SE"));
        press(QApplication::focusWidget(), Qt::Key_Up);
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("E"));
        press(QApplication::focusWidget(), Qt::Key_Home);
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("NW"));
        press(QApplication::focusWidget(), Qt::Key_End);
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("SE"));
        QCOMPARE(page.wind->value(), QString("N"));
        QCOMPARE(changed.count(), 0);

        // Space and Enter choose.
        press(QApplication::focusWidget(), Qt::Key_Space);
        QCOMPARE(page.wind->value(), QString("SE"));
        press(QApplication::focusWidget(), Qt::Key_Left);
        press(QApplication::focusWidget(), Qt::Key_Return);
        QCOMPARE(page.wind->value(), QString("S"));
        QCOMPARE(changed.count(), 2);

        // Tab leaves from wherever the arrows stopped, and comes back in at the choice.
        press(QApplication::focusWidget(), Qt::Key_Left);               // on SW, not chosen
        press(QApplication::focusWidget(), Qt::Key_Tab);
        QVERIFY(!page.wind->isAncestorOf(QApplication::focusWidget()));
        press(QApplication::focusWidget(), Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), page.wind->cell("S"));

        // Precipitation in rows: Down moves by column.
        page.precip->setValue("Rain");
        page.precip->cell("Rain")->setFocus();
        press(page.precip->cell("Rain"), Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget(), page.precip->cell("Freezing rain"));
        QCOMPARE(page.precip->value(), QString("Rain"));
    }

    void enterPressesButtons()
    {
        // KB3: Enter presses the focused button, as Space does (the app installs EnterKey).
        EnterKey enterKey;
        qApp->installEventFilter(&enterKey);
        Page page;
        page.w.activateWindow();
        page.cloud->setValue("Overcast");
        page.cloud->clearButton()->setFocus();
        press(page.cloud->clearButton(), Qt::Key_Return);
        QTRY_COMPARE(page.cloud->value(), QString());
        page.cloud->cell("Clear")->setFocus();
        press(page.cloud->cell("Clear"), Qt::Key_Enter);
        QTRY_COMPARE(page.cloud->value(), QString("Clear"));
        auto *box = new QCheckBox("First snow", &page.w);           // check boxes too
        box->show();
        box->setFocus();
        press(box, Qt::Key_Return);
        QTRY_VERIFY(box->isChecked());
        press(box, Qt::Key_Return);
        QTRY_VERIFY(!box->isChecked());
        // The arrows never move between separate buttons.
        auto *one = new QPushButton("One", &page.w);
        auto *two = new QPushButton("Two", &page.w);
        one->show();
        two->show();
        one->setFocus();
        press(one, Qt::Key_Down);
        press(QApplication::focusWidget(), Qt::Key_Right);
        QCOMPARE(QApplication::focusWidget(), one);
        // Inside a group they still move (KB2).
        page.cloud->cell("Clear")->setFocus();
        press(page.cloud->cell("Clear"), Qt::Key_Right);
        QCOMPARE(QApplication::focusWidget(), page.cloud->cell("Mostly clear"));
        qApp->removeEventFilter(&enterKey);
    }

    void choiceRowKeys()
    {
        // Settings' rows (KB2, D66): one Tab stop at the choice; arrows move, Space chooses.
        QWidget w;
        auto *layout = new QVBoxLayout(&w);
        auto *before = new QLineEdit;
        auto *row = new ChoiceRow("Text size");
        row->addChoice(0, "Normal", "");
        row->addChoice(1, "Large", "");
        row->addChoice(2, "Extra large", "");
        auto *after = new QLineEdit;
        layout->addWidget(before);
        layout->addWidget(row);
        layout->addWidget(after);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        row->setChosen(1);
        QSignalSpy chosen(row, &ChoiceRow::chosenChanged);
        before->setFocus();
        press(before, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), row->buttons()[1]);
        press(QApplication::focusWidget(), Qt::Key_Right);
        QCOMPARE(QApplication::focusWidget(), row->buttons()[2]);
        QCOMPARE(row->chosen(), 1);
        press(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), after);
        press(after, Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), row->buttons()[1]);
        press(QApplication::focusWidget(), Qt::Key_Home);
        press(QApplication::focusWidget(), Qt::Key_Space);
        QCOMPARE(row->chosen(), 0);
        QCOMPARE(chosen.count(), 1);
        // At least 88 wide and 44 high.
        QVERIFY(row->buttons()[0]->width() >= 88);
        QVERIFY(row->buttons()[0]->height() >= 44);
    }

    void clearAndRepeatClick()
    {
        Page page;
        QSignalSpy changed(page.cloud, &ChoiceGroup::changed);
        QTest::mouseClick(page.cloud->cell("Overcast"), Qt::LeftButton);
        QCOMPARE(page.cloud->value(), QString("Overcast"));
        QVERIFY(page.cloud->clearButton()->isEnabled());
        QTest::mouseClick(page.cloud->cell("Overcast"), Qt::LeftButton);   // D42: stays chosen
        QCOMPARE(page.cloud->value(), QString("Overcast"));
        QTest::mouseClick(page.cloud->clearButton(), Qt::LeftButton);
        QCOMPARE(page.cloud->value(), QString());
        QVERIFY(!page.cloud->clearButton()->isEnabled());
        QCOMPARE(changed.count(), 3);
        page.cloud->setValue("Clear");                                  // setValue doesn't emit
        QCOMPARE(changed.count(), 3);
    }

    void spokenNames()
    {
        Page page;
        QCOMPARE(page.wind->accessibleName(), QString("Wind direction, going to"));
        QCOMPARE(page.wind->clearButton()->accessibleName(), QString("Clear: Wind direction, going to"));
        QCOMPARE(page.precip->cell("Freezing rain")->accessibleName(), QString("Freezing rain"));
        QVERIFY(page.precip->cell("Rain")->isCheckable());               // Narrator reads chosen or not
    }
};

QTEST_MAIN(TestChoiceGroup)
#include "tst_choicegroup.moc"
