import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QProgressBar, QPushButton, QListWidget,
    QListWidgetItem, QMessageBox, QSizePolicy
)
from PyQt6.QtCore import Qt, pyqtSignal, QObject

log = logging.getLogger(__name__)


class DownloadItemWidget(QWidget):
    """A visual row for an active or finished download."""

    cancel_requested = pyqtSignal(str)
    retry_requested = pyqtSignal(str)

    def __init__(self, url: str, title: str = None):
        super().__init__()
        self.url = url
        self.title = title or f"Fetching title: {url}"
        self._build()

    def _build(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(4, 2, 4, 2)

        # Row: title + percent + cancel
        top = QHBoxLayout()
        self.title_label = QLabel(self.title)
        self.title_label.setWordWrap(True)
        top.addWidget(self.title_label, stretch=1)

        self.percent_label = QLabel("0%")
        self.percent_label.setAlignment(Qt.AlignmentFlag.AlignRight)
        self.percent_label.setFixedWidth(100)
        top.addWidget(self.percent_label)

        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.setFixedWidth(70)
        self.cancel_btn.clicked.connect(self._on_cancel_clicked)
        top.addWidget(self.cancel_btn)

        layout.addLayout(top)

        self.progress = QProgressBar()
        self.progress.setValue(0)
        self.progress.setTextVisible(False)
        self.progress.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        layout.addWidget(self.progress)

        self.setLayout(layout)

    def update_progress(self, percent, text):
        """Update progress bar and text on right."""
        self.progress.setValue(int(percent))
        self.percent_label.setText(text)

    def mark_completed(self, title):
        """Mark this item as completed."""
        self.title_label.setText(f"Completed: {title}")
        self.progress.setValue(100)
        self.progress.setStyleSheet("QProgressBar::chunk { background-color: #2ecc71; }")
        self.percent_label.setText("100%")
        self.cancel_btn.setVisible(False)

    def mark_cancelled(self, title):
        """Mark this item as cancelled."""
        self.title_label.setText(f"Cancelled: {title}")
        self.progress.setStyleSheet("QProgressBar::chunk { background-color: #e67e22; }")
        self.percent_label.setText("Cancelled")
        self.cancel_btn.setText("Retry")
        self.cancel_btn.clicked.disconnect()
        self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))

    def mark_failed(self, title):
        """Mark as failed, allows retry."""
        self.title_label.setText(f"Failed: {title}")
        self.progress.setStyleSheet("QProgressBar::chunk { background-color: #c0392b; }")
        self.percent_label.setText("Error")
        self.cancel_btn.setText("Retry")
        self.cancel_btn.clicked.disconnect()
        self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))

    def _on_cancel_clicked(self):
        self.cancel_requested.emit(self.url)


class ActiveDownloadsTab(QWidget):
    """Manages list of active, queued, and completed downloads."""

    all_downloads_complete = pyqtSignal()

    def __init__(self, main_window):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        self.active_items = {}  # url → DownloadItemWidget
        self._build_tab_active()

    def _build_tab_active(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)

        title = QLabel("Active Downloads")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-weight: bold; font-size: 14pt;")
        layout.addWidget(title)

        self.list_widget = QListWidget()
        layout.addWidget(self.list_widget)

        self.setLayout(layout)

    def add_download_widget(self, worker_or_url):
        """
        Adapter expected by main_window.download_added.
        Accepts:
          - a worker object with attributes/signals (common case),
          - a (url, title) tuple,
          - or a raw url string.

        Creates (or reuses) a DownloadItemWidget and wires signals.
        """
        # --- determine url and title from input ---
        url = None
        title = None
        worker = None

        # tuple (url, title)
        if isinstance(worker_or_url, (list, tuple)) and len(worker_or_url) >= 1:
            url = worker_or_url[0]
            if len(worker_or_url) > 1:
                title = worker_or_url[1]
        # string url
        elif isinstance(worker_or_url, str):
            url = worker_or_url
        else:
            # probably a worker object
            worker = worker_or_url
            # try common attribute names
            url = getattr(worker, "url", None) or getattr(worker, "raw_url", None) or getattr(worker, "download_url", None)
            title = getattr(worker, "video_title", None) or getattr(worker, "title", None)

        if not url:
            # fallback to string repr to avoid crash
            url = str(worker_or_url)

        # If widget already exists for this URL, reuse it
        if url in self.active_items:
            widget = self.active_items[url]
        else:
            widget = self.add_placeholder(url)
            if title:
                widget.title_label.setText(title)
            self.active_items[url] = widget

        # If a worker object is provided, wire up its signals to update the widget
        if worker is not None:
            # Progress signal variations: try multiple common names
            if hasattr(worker, "progress"):
                try:
                    worker.progress.connect(lambda data, w=widget: self._on_worker_progress(w, data))
                except Exception:
                    pass
            if hasattr(worker, "progress_updated"):
                try:
                    worker.progress_updated.connect(lambda pct, w=widget: w.update_progress(pct, f"{pct:.1f}%"))
                except Exception:
                    pass
            if hasattr(worker, "status_updated"):
                try:
                    worker.status_updated.connect(lambda s, w=widget: w.title_label.setText(s if s else w.title_label.text()))
                except Exception:
                    pass
            if hasattr(worker, "title_updated"):
                try:
                    worker.title_updated.connect(lambda t, w=widget: w.title_label.setText(t))
                except Exception:
                    pass
            # finished signal variations: finished(bool) or finished()
            if hasattr(worker, "finished"):
                try:
                    # try signature finished(success: bool)
                    worker.finished.connect(lambda success, w=widget, wr=worker: self._on_worker_finished(w, wr, success))
                except TypeError:
                    # maybe emits no-arg finished
                    try:
                        worker.finished.connect(lambda w=widget, wr=worker: self._on_worker_finished(w, wr, True))
                    except Exception:
                        pass
            if hasattr(worker, "error_occurred"):
                try:
                    worker.error_occurred.connect(lambda err, w=widget, wr=worker: self._on_worker_failed(w, wr, err))
                except Exception:
                    pass

        return widget


    # ---------- helper callbacks for wiring workers ----------

    def _on_worker_progress(self, widget: DownloadItemWidget, data):
        """
        Accepts either dict-like progress updates or plain strings from yt-dlp.
        Tries to extract a numeric percent and a short human text.
        """
        pct = 0.0
        text = ""

        # dict-like (common)
        if isinstance(data, dict):
            # common keys
            pct = data.get("percent") or data.get("pct") or data.get("progress") or 0
            # sometimes percent is string "12.3"
            try:
                pct = float(pct)
            except Exception:
                pct = 0.0
            text = data.get("text") or data.get("status") or data.get("msg") or f"{pct:.1f}%"
        else:
            # fallback: try extract number from text
            text = str(data)
            import re
            m = re.search(r"(\d{1,3}(?:\.\d+)?)\s?%", text)
            if m:
                try:
                    pct = float(m.group(1))
                except Exception:
                    pct = 0.0
            else:
                pct = 0.0

        # clamp
        try:
            percent_int = max(0, min(100, int(round(pct))))
        except Exception:
            percent_int = 0

        # Build a short right-hand label (percentage + maybe bytes if present)
        right_text = f"{percent_int}%"
        # if data contains bytes info, show it
        if isinstance(data, dict):
            downloaded = data.get("downloaded_bytes") or data.get("downloaded") or None
            total = data.get("total_bytes") or data.get("total") or None
            if downloaded and total:
                try:
                    d_mb = float(downloaded) / 1024.0 / 1024.0
                    t_mb = float(total) / 1024.0 / 1024.0
                    right_text = f"{percent_int}% ({d_mb:.2f} MB / {t_mb:.2f} MB)"
                except Exception:
                    pass

        widget.update_progress(percent_int, right_text)


    def _on_worker_finished(self, widget: DownloadItemWidget, worker, success: bool):
        """Worker finished handler — mark completed or failed."""
        # try to obtain best title available
        title = getattr(worker, "video_title", None) or getattr(worker, "title", None) or widget.title_label.text()
        if success:
            widget.mark_completed(title)
        else:
            widget.mark_failed(title)


    def _on_worker_failed(self, widget: DownloadItemWidget, worker, err_text: str):
        """Explicit error path (if worker emits error_occurred)."""
        title = getattr(worker, "video_title", None) or getattr(worker, "title", None) or widget.title_label.text()
        # show error on widget and allow retry
        widget.mark_failed(title)
        # optionally pop up a message to user
        try:
            QMessageBox.warning(self, "Download error", f"{title}\n\n{err_text}")
        except Exception:
            pass


    # --- item management ---
    def add_placeholder(self, url):
        """Add a placeholder entry while fetching title."""
        item_widget = DownloadItemWidget(url)
        item_widget.cancel_requested.connect(self._cancel_download)
        item_widget.retry_requested.connect(self._retry_download)
        lw_item = QListWidgetItem()
        lw_item.setSizeHint(item_widget.sizeHint())
        self.list_widget.addItem(lw_item)
        self.list_widget.setItemWidget(lw_item, item_widget)
        self.active_items[url] = item_widget
        return item_widget

    def update_progress(self, url, percent, text):
        """Update the download progress for a given URL."""
        if url in self.active_items:
            self.active_items[url].update_progress(percent, text)

    def mark_completed(self, url, title):
        if url in self.active_items:
            self.active_items[url].mark_completed(title)
        self._check_if_all_done()

    def mark_cancelled(self, url, title):
        if url in self.active_items:
            self.active_items[url].mark_cancelled(title)
        self._check_if_all_done()

    def mark_failed(self, url, title):
        if url in self.active_items:
            self.active_items[url].mark_failed(title)
        self._check_if_all_done()

    def _cancel_download(self, url):
        """Signal up to main window."""
        self.main.cancel_download(url)

    def _retry_download(self, url):
        """Signal up to main window to retry."""
        self.main.retry_download(url)

    def _check_if_all_done(self):
        """Emit all_downloads_complete if all active items are finished."""
        if all(
            self.active_items[url].percent_label.text() in ("100%", "Cancelled", "Error")
            for url in self.active_items
        ):
            log.debug("All downloads complete, emitting signal.")
            self.all_downloads_complete.emit()
