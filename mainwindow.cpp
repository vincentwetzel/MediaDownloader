#include "mainwindow.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMessageBox>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), reply(nullptr), outputFile(nullptr) {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    urlInput = new QLineEdit(this);
    urlInput->setPlaceholderText("Enter media URL here...");
    urlInput->setToolTip("Paste the web address (URL) of the video or audio you want to download here. For example, a YouTube link.");
    layout->addWidget(urlInput);

    downloadButton = new QPushButton("Download", this);
    downloadButton->setToolTip("Click this button to start downloading the media from the URL you entered.");
    layout->addWidget(downloadButton);

    progressBar = new QProgressBar(this);
    progressBar->setValue(0);
    progressBar->setToolTip("This bar shows how much of your download is complete. When it reaches 100%, your file is ready!");
    layout->addWidget(progressBar);

    statusLabel = new QLabel("Ready", this);
    statusLabel->setToolTip("This message tells you what the app is doing, like 'Ready', 'Downloading...', or if there was an error.");
    layout->addWidget(statusLabel);

    manager = new QNetworkAccessManager(this);

    connect(downloadButton, &QPushButton::clicked, this, &MainWindow::startDownload);

    setWindowTitle("Media Downloader");
    resize(400, 200);
}

MainWindow::~MainWindow() {
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
    if (outputFile) {
        outputFile->close();
        delete outputFile;
    }
}

void MainWindow::startDownload() {
    QString urlString = urlInput->text();
    if (urlString.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a URL.");
        return;
    }

    QUrl url(urlString);
    if (!url.isValid()) {
        QMessageBox::warning(this, "Error", "Invalid URL.");
        return;
    }

    QString fileName = url.fileName();
    if (fileName.isEmpty()) {
        fileName = "downloaded_file";
    }

    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString filePath = QDir(downloadPath).filePath(fileName);

    outputFile = new QFile(filePath);
    if (!outputFile->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Could not open file for writing: " + filePath);
        delete outputFile;
        outputFile = nullptr;
        return;
    }

    statusLabel->setText("Downloading...");
    downloadButton->setEnabled(false);
    progressBar->setValue(0);

    QNetworkRequest request(url);
    reply = manager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::onDownloadProgress);
    connect(reply, &QNetworkReply::readyRead, this, &MainWindow::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onDownloadFinished);
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        progressBar->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
    }
}

void MainWindow::onReadyRead() {
    if (outputFile && outputFile->isOpen()) {
        outputFile->write(reply->readAll());
    }
}

void MainWindow::onDownloadFinished() {
    if (outputFile) {
        outputFile->close();
        delete outputFile;
        outputFile = nullptr;
    }

    if (reply->error() == QNetworkReply::NoError) {
        statusLabel->setText("Download finished!");
    } else {
        statusLabel->setText("Download error: " + reply->errorString());
    }

    downloadButton->setEnabled(true);
    reply->deleteLater();
    reply = nullptr;
}