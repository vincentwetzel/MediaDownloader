import logging
from core.yt_dlp_worker import DownloadWorker
from PyQt6.QtCore import QObject, pyqtSignal

log = logging.getLogger(__name__)


class DownloadManager(QObject):
    download_added = pyqtSignal(object)
    download_finished = pyqtSignal(str, bool)
    download_error = pyqtSignal(str, str)

    def __init__(self):
        super().__init__()
        self.active_downloads = []

    def add_download(self, url, opts):
        worker = DownloadWorker(url, opts)
        worker.progress.connect(self._on_progress)
        worker.finished.connect(self._on_finished)
        worker.error.connect(self._on_error)

        self.active_downloads.append(worker)
        self.download_added.emit(worker)
        worker.start()
        log.debug(f"Download added: {url}")

    def _on_progress(self, data):
        log.debug(f"Progress: {data}")

    def _on_finished(self, url, success):
        log.info(f"Download finished: {url} success={success}")
        self.download_finished.emit(url, success)

    def _on_error(self, url, message):
        log.error(f"Error on {url}: {message}")
        self.download_error.emit(url, message)
