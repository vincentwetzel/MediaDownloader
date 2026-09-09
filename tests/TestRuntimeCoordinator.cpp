#include "core/RuntimeCoordinator.h"

#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

class TestRuntimeCoordinator : public QObject {
    Q_OBJECT

private slots:
    void testOwnerReceivesClientCommand();
    void testRejectsUnexpectedCommand();
};

void TestRuntimeCoordinator::testOwnerReceivesClientCommand()
{
    const QString name = QStringLiteral("LzyDownloaderTestCoordinator-%1")
                             .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    RuntimeCoordinator owner(name);
    if (owner.startOrNotify(QStringLiteral("show-ui")) != RuntimeCoordinator::StartResult::Owner) {
        QSKIP("QLocalServer is unavailable in this execution environment.");
    }

    QSignalSpy spy(&owner, &RuntimeCoordinator::commandReceived);
    RuntimeCoordinator client(name);
    QVERIFY(client.startOrNotify(QStringLiteral("ensure-api")) == RuntimeCoordinator::StartResult::ClientNotified);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("ensure-api"));
}

void TestRuntimeCoordinator::testRejectsUnexpectedCommand()
{
    RuntimeCoordinator coordinator(QStringLiteral("LzyDownloaderInvalidCommand"));
    QVERIFY(coordinator.startOrNotify(QStringLiteral("enqueue arbitrary input"))
            == RuntimeCoordinator::StartResult::Unavailable);
}

QTEST_GUILESS_MAIN(TestRuntimeCoordinator)
#include "TestRuntimeCoordinator.moc"
