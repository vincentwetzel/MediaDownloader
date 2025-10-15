import logging
import subprocess
from PyQt6.QtCore import QThread, pyqtSignal

log = logging.getLogger(__name__)


class DownloadWorker(QThread):
    progress = pyqtSignal(dict)
    finished = pyqtSignal(str, bool)
    error = pyqtSignal(str, str)

    def __init__(self, url, opts, parent=None):
        super().__init__(parent)
        self.url = url
        self.opts = opts
        self._is_cancelled = False

    def run(self):
        cmd = ["yt-dlp"] + self.opts + [self.url]
        log.debug(f"Running command: {' '.join(cmd)}")

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )

        for line in process.stdout:
            if self._is_cancelled:
                process.terminate()
                log.info(f"Cancelled download: {self.url}")
                return
            self.progress.emit({"url": self.url, "text": line.strip()})
            log.debug(f"{self.url}: {line.strip()}")

        process.wait()
        if process.returncode == 0:
            self.finished.emit(self.url, True)
        else:
            self.error.emit(self.url, f"Error code {process.returncode}")
            log.error(f"Download failed for {self.url} with code {process.returncode}")

    def cancel(self):
        self._is_cancelled = True
