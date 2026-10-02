#include "controllers/GitStatusController.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using Statuses = QHash<QString, QString>;

class GitStatusControllerTest : public QObject
{
    Q_OBJECT
private:
    QString git;

    bool runGit(const QString &directory, const QStringList &arguments)
    {
        QProcess process;
        process.start(git, QStringList{"-C", directory} + arguments);
        return process.waitForFinished(5000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    bool write(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

    bool initializeRepository(const QString &directory)
    {
        return runGit(directory, {"init", "--initial-branch=main"})
            && write(directory + "/tracked.txt", "original\n")
            && runGit(directory, {"add", "tracked.txt"})
            && runGit(directory, {"-c", "user.name=tfx-test", "-c", "user.email=tfx@example.invalid",
                                  "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null",
                                  "commit", "-m", "initial"});
    }

    Statuses lastStatuses(const QSignalSpy &spy) const
    {
        return spy.isEmpty() ? Statuses{} : qvariant_cast<Statuses>(spy.last().first());
    }

private slots:
    void initTestCase()
    {
        git = QStandardPaths::findExecutable("git");
        if (git.isEmpty()) QSKIP("git is not installed");
        qRegisterMetaType<Statuses>();
    }

    void throttledRefreshKeepsBadgesAndBranch()
    {
        QTemporaryDir repository;
        QVERIFY(repository.isValid());
        QVERIFY(initializeRepository(repository.path()));
        const QString path = repository.path() + "/tracked.txt";
        QVERIFY(write(path, "modified\n"));

        GitStatusController controller;
        QSignalSpy statuses(&controller, &GitStatusController::statusesReady);
        QSignalSpy branch(&controller, &GitStatusController::branchChanged);
        controller.refresh(repository.path());
        // A watcher update inside the throttle window must keep its path
        // when the queued refresh clears m_pendingDirectory.
        controller.refresh(repository.path());
        QTRY_COMPARE(lastStatuses(statuses).value(path), QString("M"));
        QTRY_VERIFY(!branch.isEmpty() && branch.last().first().toString() == "main");
        statuses.clear();
        branch.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!statuses.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!branch.isEmpty(), 5000);
        for (const auto &event : statuses) {
            QCOMPARE(qvariant_cast<Statuses>(event.first()).value(path), QString("M"));
        }
        for (const auto &event : branch) {
            QCOMPARE(event.first().toString(), QString("main"));
        }

        // Keeping the old result while querying must not keep stale badges
        // once git reports that the working tree is clean.
        QVERIFY(write(path, "original\n"));
        statuses.clear();
        controller.refresh(repository.path());
        QVERIFY(statuses.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!statuses.isEmpty(), 5000);
        QVERIFY(lastStatuses(statuses).isEmpty());
    }

    void navigationClearsBadgesAndCancelsQueuedRefresh()
    {
        QTemporaryDir repository;
        QTemporaryDir outside;
        QVERIFY(repository.isValid());
        QVERIFY(outside.isValid());
        QVERIFY(initializeRepository(repository.path()));
        const QString path = repository.path() + "/tracked.txt";
        QVERIFY(write(path, "modified\n"));
        GitStatusController controller;
        QSignalSpy statuses(&controller, &GitStatusController::statusesReady);
        QSignalSpy branch(&controller, &GitStatusController::branchChanged);
        controller.refresh(repository.path());
        QTRY_COMPARE(lastStatuses(statuses).value(path), QString("M"));
        QTRY_VERIFY(!branch.isEmpty() && branch.last().first().toString() == "main");
        statuses.clear();
        branch.clear();
        controller.refresh(repository.path());
        controller.refresh(outside.path());
        QVERIFY(!statuses.isEmpty());
        QVERIFY(lastStatuses(statuses).isEmpty());
        QVERIFY(!branch.isEmpty());
        QVERIFY(branch.last().first().toString().isEmpty());
        QTest::qWait(1200);
        for (const auto &event : statuses) QVERIFY(qvariant_cast<Statuses>(event.first()).isEmpty());
        for (const auto &event : branch) QVERIFY(event.first().toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(GitStatusControllerTest)
#include "GitStatusControllerTest.moc"
