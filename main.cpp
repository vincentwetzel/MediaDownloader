#include "src/ui/MainWindow.h"
#include "src/utils/LogManager.h"
#include "src/utils/ExtractorJsonParser.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QStringList>
#include <QSystemSemaphore>
#include <QSharedMemory>

int main(int argc, char *argv[]) {
    QSystemSemaphore semaphore("MediaDownloaderSemaphore", 1);
    semaphore.acquire();

    QSharedMemory sharedMemory("MediaDownloaderSingleInstance");
    if (!sharedMemory.create(1)) {
        semaphore.release();
        return 0;
    }

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/app-icon"));

    a.setOrganizationName("MediaDownloader");
    a.setApplicationName("MediaDownloader");

    QStringList libraryPaths = QApplication::libraryPaths();
    libraryPaths.prepend(a.applicationDirPath());
    libraryPaths.prepend(QDir(a.applicationDirPath()).filePath("plugins"));
    QApplication::setLibraryPaths(libraryPaths);

    LogManager::installHandler();

    // Create the parser here so it can be passed down
    ExtractorJsonParser extractorJsonParser;

    MainWindow w(&extractorJsonParser);
    w.show();

    int result = a.exec();
    semaphore.release();
    return result;
}
