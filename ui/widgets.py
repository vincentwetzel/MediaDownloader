from PyQt6.QtWidgets import QTextEdit, QProgressBar, QPushButton, QHBoxLayout, QWidget, QLabel
from PyQt6.QtCore import Qt, pyqtSignal
import logging

log = logging.getLogger(__name__)

class UrlTextEdit(QTextEdit):
    def focusInEvent(self, ev):
        super().focusInEvent(ev)
        try:
            from PyQt6.QtGui import QGuiApplication
            cb = QGuiApplication.clipboard()
            txt = cb.text().strip()
            if txt.startswith("http://") or txt.startswith("https://"):
                log.debug("Auto-paste from clipboard: %s", txt)
                self.setPlainText(txt)
        except Exception:
            pass

class DownloadItemWidget(QWidget):
    """A visual row for an active or finished download."""

    cancel_requested = pyqtSignal(str)
    retry_requested = pyqtSignal(str)
    resume_requested = pyqtSignal(str)

    def __init__(self, url: str, title: str = None):
        super().__init__()
        self.url = url
        self.title = title or f"Fetching Title: {url}"
        # Track whether the download has actually started (received progress/starting)
        self._started = False
        self._final_path = None # Store final path for "Open Folder"
        self._style_state = None
        self._last_render_value = None
        self._last_render_text = None
        self._build()

    def _set_progress_style(self, state: str):
        """Apply progress style only when state changes to avoid expensive repaints."""
        if getattr(self, "_style_state", None) == state:
            return
        styles = {
            "active": (
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; color: #000000; }"
                "QProgressBar::chunk { background-color: #3498db; border-radius: 6px; }"
            ),
            "completed": (
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; color: #000000; }"
                "QProgressBar::chunk { background-color: #2ecc71; border-radius: 6px; }"
            ),
            "error": (
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; color: #000000; }"
                "QProgressBar::chunk { background-color: #c0392b; border-radius: 6px; }"
            ),
        }
        self.progress.setStyleSheet(styles.get(state, styles["active"]))
        self._style_state = state

    def _build(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(4, 2, 4, 2)

        # Title
        self.title_label = QLabel(self.title)
        self.title_label.setWordWrap(True)
        layout.addWidget(self.title_label, stretch=1)

        # Progress Bar
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.progress.setTextVisible(True)
        self.progress.setFormat("0%")
        self.progress.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.progress.setFixedWidth(200)
        self._set_progress_style("active")
        layout.addWidget(self.progress)

        # Cancel Button
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.setFixedWidth(70)
        self.cancel_btn.clicked.connect(self._on_cancel_clicked)
        self.cancel_btn.setToolTip("Cancel or resume the download, or retry after a failure.")
        layout.addWidget(self.cancel_btn)
        
        # Open Folder button (hidden initially)
        self.open_folder_btn = QPushButton("Open Folder")
        self.open_folder_btn.setFixedWidth(90)
        self.open_folder_btn.clicked.connect(self._on_open_folder_clicked)
        self.open_folder_btn.setToolTip("Open the folder containing this file.")
        self.open_folder_btn.setVisible(False)
        layout.addWidget(self.open_folder_btn)

        self.setLayout(layout)

    def update_progress(self, percent, text):
        """Update progress bar and text."""
        if percent is None:
            val = self.progress.value()
        else:
            try:
                val = float(percent)
            except Exception:
                val = 0.0

        try:
            self.progress.setValue(int(val))
        except Exception:
            self.progress.setValue(0)

        try:
            render_text = str(text)
            self.progress.setFormat(render_text)
        except Exception:
            try:
                self.progress.setFormat(f"{val:.1f}%")
            except Exception:
                self.progress.setFormat("0.0%")

    def mark_completed(self, title, final_path=None):
        """Mark this item as completed."""
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self.progress.setValue(100)
        self._set_progress_style("completed")
        self.progress.setFormat("Done")
        self.cancel_btn.setVisible(False)
        
        if final_path:
            self._final_path = final_path
            self.open_folder_btn.setVisible(True)

    def mark_cancelled(self, title):
        """Mark this item as cancelled."""
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self.progress.setFormat("Cancelled")
        self._set_progress_style("error")
        self.cancel_btn.setText("Resume")
        try:
            self.cancel_btn.clicked.disconnect()
        except Exception:
            pass
        self.cancel_btn.clicked.connect(lambda: self.resume_requested.emit(self.url))

    def mark_failed(self, title):
        """Mark as failed, allows retry."""
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self._set_progress_style("error")
        self.progress.setFormat("Error")
        self.cancel_btn.setText("Retry")
        try:
            self.cancel_btn.clicked.disconnect()
        except Exception:
            pass
        self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))

    def _on_cancel_clicked(self):
        self.cancel_requested.emit(self.url)
        
    def _on_open_folder_clicked(self):
        if self._final_path:
            # Logic to open folder would go here, but this widget is just UI.
            # The signal connection in ActiveDownloadsTab handles the actual opening.
            pass

    def mark_active(self):
        """Set widget to active/downloading state."""
        self._set_progress_style("active")
        self.cancel_btn.setText("Cancel")
        try:
            self.cancel_btn.clicked.disconnect()
        except Exception:
            pass
        self.cancel_btn.clicked.connect(self._on_cancel_clicked)
        self.open_folder_btn.setVisible(False)
