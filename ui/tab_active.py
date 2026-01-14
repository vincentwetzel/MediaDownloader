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
    resume_requested = pyqtSignal(str)

    def __init__(self, url: str, title: str = None):
        super().__init__()
        self.url = url
        self.title = title or f"Fetching Title: {url}"
        # Track whether the download has actually started (received progress/starting)
        self._started = False
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
        # Show progress text centered inside the bar instead of a separate label
        self.progress.setTextVisible(True)
        self.progress.setFormat("0%")
        # Center the progress text horizontally inside the bar
        self.progress.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.progress.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        # Make the progress bar visually consistent (same thickness when active
        # and when completed). Use a fixed height and a base stylesheet that
        # defines the bar and chunk appearance; color of the chunk will be
        # adjusted on status changes.
        self.progress.setFixedHeight(18)
        self.progress.setStyleSheet(
            "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
            "QProgressBar::chunk { background-color: #3498db; border-radius: 6px; }"
        )
        # Track last numeric percent shown so transient postprocessing
        # messages (which often arrive with no percent) don't cause the
        # bar to drop back to 0 after reaching 100%.
        self._last_percent = 0.0
        # Track whether we're currently in postprocessing (merging,
        # modifying chapters, embedding, deleting originals, etc.). When
        # True, keep the progress chunk visually 'active' (blue) until
        # the worker signals finished().
        self._postprocessing_active = False
        layout.addWidget(self.progress)
        # Hide the right-hand percent label now that the progress bar shows
        # the percentage centered inside the bar. Keep the widget present so
        # layout spacing remains stable if needed.
        self.percent_label.setVisible(False)
        # Remove reserved width so the hidden label doesn't push content
        try:
            self.percent_label.setFixedWidth(0)
        except Exception:
            pass

        self.setLayout(layout)

    def update_progress(self, percent, text):
        """Update progress bar and text on right."""
        # Update numeric value and show a readable centered format inside
        # the progress bar (percent or percent with bytes).
        # Normalize percent to float for display precision, but the widget
        # value must be an integer 0-100.
        try:
            val = float(percent)
        except Exception:
            val = 0.0
        # If the incoming text indicates postprocessing activity and the
        # widget previously reached 100%, preserve the full bar instead of
        # lowering it to 0 (yt-dlp often emits postprocessing lines without
        # a percent field).
        try:
            low_text = (str(text) or "").lower()
            post_keys = (
                "merg", "merge", "merging", "delet", "remov", "ffmpeg",
                "postproc", "post-process", "merger",
                # chapter / modify operations, sponsorblock, embedding
                "chap", "chapter", "chapters", "modify", "modifychap",
                "sponsor", "sponsorblock", "embed", "thumbnail", "tag"
            )
            is_postprocessing = any(k in low_text for k in post_keys)
            # Remember that we're in postprocessing until finished().
            if is_postprocessing:
                self._postprocessing_active = True
            else:
                # If we see normal downloading progress below 100%, clear
                # the postprocessing flag.
                try:
                    if float(val) < 100.0:
                        self._postprocessing_active = False
                except Exception:
                    pass
            # Preserve 100% display during postprocessing if we previously
            # reached it.
            if is_postprocessing and self._last_percent >= 100.0:
                display_val = int(round(self._last_percent))
            else:
                display_val = int(round(val))
            self.progress.setValue(display_val)
            # remember the highest percent we've shown
            try:
                self._last_percent = max(self._last_percent, float(display_val))
            except Exception:
                pass
        except Exception:
            try:
                self.progress.setValue(int(val))
            except Exception:
                self.progress.setValue(0)

        # Use the provided short text as the progress bar format so it
        # appears centered inside the bar. If not provided, show percent
        # with two decimals.
        try:
            self.progress.setFormat(str(text))
        except Exception:
            try:
                self.progress.setFormat(f"{val:.2f}%")
            except Exception:
                self.progress.setFormat("0.00%")

        # If we've reached 100% but yt-dlp is still doing postprocessing
        # (merging, deleting originals, ffmpeg work, etc.) keep the bar
        # visually 'active' (blue) instead of losing color. When the
        # worker finally emits finished(), mark_completed() will set the
        # final completed style.
        try:
            # If the display text indicates postprocessing activity (merging,
            # ffmpeg work, deleting originals, etc.), ensure the progress
            # chunk remains the active/blue color. Do this regardless of the
            # numeric percent so the UI doesn't go colorless as yt-dlp begins
            # postprocessing steps.
            low = (str(text) or "").lower()
            post_keys = (
                "merg", "merge", "merging", "delet", "remov", "ffmpeg",
                "postproc", "post-process", "merger"
            )
            if any(k in low for k in post_keys):
                self.progress.setStyleSheet(
                    "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
                    "QProgressBar::chunk { background-color: #3498db; border-radius: 6px; }"
                )
        except Exception:
            pass

    def mark_completed(self, title):
        """Mark this item as completed."""
        # Show only the video title (no 'Download Complete:' prefix)
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self.progress.setValue(100)
        self.progress.setStyleSheet(
            "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
            "QProgressBar::chunk { background-color: #2ecc71; border-radius: 6px; }"
        )
        # Use friendly 'Done' label instead of numeric 100%
        self.progress.setFormat("Done")
        self.cancel_btn.setVisible(False)

    def mark_cancelled(self, title):
        """Mark this item as cancelled."""
        # Preserve just the video title; show cancelled status on the bar
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        # If the download had started, present a Resume option and show a red bar.
        started = getattr(self, "_started", False)
        self.progress.setFormat("Cancelled")
        if started:
            self.progress.setStyleSheet(
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
                "QProgressBar::chunk { background-color: #c0392b; border-radius: 6px; }"
            )
            try:
                self.cancel_btn.setText("Resume")
                try:
                    self.cancel_btn.clicked.disconnect()
                except Exception:
                    pass
                self.cancel_btn.clicked.connect(lambda: self.resume_requested.emit(self.url))
            except Exception:
                pass
        else:
            # If it never started, treat as a retryable failure
            self.progress.setStyleSheet(
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
                "QProgressBar::chunk { background-color: #c0392b; border-radius: 6px; }"
            )
            try:
                self.cancel_btn.setText("Retry")
                try:
                    self.cancel_btn.clicked.disconnect()
                except Exception:
                    pass
                self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))
            except Exception:
                pass

    def mark_failed(self, title):
        """Mark as failed, allows retry."""
        # Preserve just the video title; show error state on the bar
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self.progress.setStyleSheet(
            "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
            "QProgressBar::chunk { background-color: #c0392b; border-radius: 6px; }"
        )
        self.progress.setFormat("Error")
        self.cancel_btn.setText("Retry")
        self.cancel_btn.clicked.disconnect()
        self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))

    def _on_cancel_clicked(self):
        self.cancel_requested.emit(self.url)

    def mark_active(self):
        """Set widget to active/downloading state (blue chunk, Cancel button)."""
        try:
            self.progress.setStyleSheet(
                "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
                "QProgressBar::chunk { background-color: #3498db; border-radius: 6px; }"
            )
        except Exception:
            pass
        try:
            self.cancel_btn.setText("Cancel")
            try:
                self.cancel_btn.clicked.disconnect()
            except Exception:
                pass
            self.cancel_btn.clicked.connect(self._on_cancel_clicked)
        except Exception:
            pass
        # Reset started flag until progress arrives
        try:
            self._started = False
        except Exception:
            pass


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

        # Open downloads folder (same placement as Start tab)
        top_row = QHBoxLayout()
        top_row.addStretch()
        self.open_downloads_btn_active = QPushButton("Open Downloads Folder")
        self.open_downloads_btn_active.setFixedHeight(36)
        self.open_downloads_btn_active.clicked.connect(self._on_open_downloads_clicked)
        self.open_downloads_btn_active.setToolTip("Open the configured output/downloads folder.")
        top_row.addWidget(self.open_downloads_btn_active)
        layout.addLayout(top_row)

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
                # Show only the clean video title (no 'Downloading:' prefix)
                widget.title_label.setText(self._clean_display_title(title))
            self.active_items[url] = widget

        # If a worker object is provided, wire up its signals to update the widget
        if worker is not None:
            # Progress signal variations: try multiple common names
            if hasattr(worker, "progress"):
                try:
                    worker.progress.connect(lambda data, w=widget: self._on_worker_progress(w, data))
                    # Also listen for raw progress text lines to detect title/filename early
                    worker.progress.connect(lambda data, w=widget: self._maybe_set_title_from_progress(w, data))
                except Exception:
                    pass
            if hasattr(worker, "progress_updated"):
                try:
                    worker.progress_updated.connect(lambda pct, w=widget: w.update_progress(pct, f"{pct:.2f}%"))
                except Exception:
                    pass
            # Do not have status_updated overwrite the title label (status messages
            # are typically progress-related). Instead, when the worker provides a
            # `title_updated` signal, show the downloading state with the title.
            if hasattr(worker, "title_updated"):
                try:
                    # When title is available before download starts, show queued status;
                    # otherwise show the plain title while downloading.
                    worker.title_updated.connect(lambda t, w=widget, wr=worker: self._on_title_updated(w, wr, t))
                except Exception:
                    pass
            # finished signal variations: finished(bool) or finished()
            if hasattr(worker, "finished"):
                try:
                    # Support multiple finished signal signatures:
                    # - finished()
                    # - finished(success: bool)
                    # - finished(url: str, success: bool)
                    # Use a flexible lambda that inspects the last arg for success.
                    worker.finished.connect(lambda *args, w=widget, wr=worker: self._on_worker_finished(w, wr, args[-1] if args else True))
                except Exception:
                    try:
                        worker.finished.connect(lambda w=widget, wr=worker: self._on_worker_finished(w, wr, True))
                    except Exception:
                        pass
            if hasattr(worker, "error_occurred"):
                try:
                    worker.error_occurred.connect(lambda err, w=widget, wr=worker: self._on_worker_failed(w, wr, err))
                except Exception:
                    pass

            # When a real worker is attached, ensure the widget shows active state
            try:
                widget.mark_active()
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
            # sometimes percent is string "12.3" or embedded in a text line
            try:
                pct = float(pct)
            except Exception:
                pct = 0.0
            text = data.get("text") or data.get("status") or data.get("msg") or ""
            # If percent not provided but we have a textual progress line, try to extract
            # a percentage like '12.3%' from that text (yt-dlp emits progress in text lines).
            if (not pct or pct == 0.0) and isinstance(text, str) and text:
                try:
                    import re
                    m = re.search(r"(\d{1,3}(?:\.\d+)?)\s?%", text)
                    if m:
                        pct = float(m.group(1))
                except Exception:
                    pass
            # Fallback display text
            if not text:
                text = f"{pct:.1f}%"
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

        # clamp (integer value for the progress bar widget)
        try:
            percent_int = max(0, min(100, int(round(pct))))
        except Exception:
            percent_int = 0

        # Build display text for the progress bar. Prefer user-friendly
        # status messages (use yt-dlp verbiage when available). When a
        # numeric percent is present prefer a two-decimal display.
        right_text = f"{pct:.2f}%"
        if isinstance(data, dict):
            raw_text = (data.get("text") or data.get("status") or data.get("msg") or "").strip()
        else:
            raw_text = str(data)

        low = (raw_text or "").lower()
        # Detect postprocessing/status keywords so we can present a friendly
        # message and (critically) preserve that status even when a numeric
        # percent is available (e.g. yt-dlp reports 100% then begins merging).
        post_keys = (
            "merg", "merge", "merging", "delet", "remov", "ffmpeg",
            "postproc", "post-process", "merger"
        )
        is_postprocessing = any(k in low for k in post_keys)

        # Map certain yt-dlp lines to friendly short statuses. If we're in a
        # postprocessing state prefer that message over a numeric percent so
        # the UI can keep the active/blue styling until the worker signals done.
        if is_postprocessing or "merging" in low:
            right_text = "Postprocessing..."
        elif "destination:" in low or "preparing" in low or "extracting" in low:
            # e.g. Destination: <file> or Extracting audio
            right_text = "Preparing Download..."
        # If we have a numeric percent and we're NOT in postprocessing, prefer
        # the concise downloading line using the numeric percent.
        if percent_int > 0 and not is_postprocessing:
            try:
                right_text = f"Downloading: {pct:.2f}%"
            except Exception:
                right_text = f"{percent_int}%"
        elif raw_text:
            # Fall back to the raw text (trimmed) if no percent available
            # but keep it short by taking the first reasonable chunk.
            if len(raw_text) > 60:
                right_text = raw_text[:57].rstrip() + "..."
            else:
                right_text = raw_text

        # If bytes info is present and we already have a percent, append sizes
        if isinstance(data, dict):
            downloaded = data.get("downloaded_bytes") or data.get("downloaded") or None
            total = data.get("total_bytes") or data.get("total") or None
            if downloaded and total and percent_int > 0:
                try:
                    d_mb = float(downloaded) / 1024.0 / 1024.0
                    t_mb = float(total) / 1024.0 / 1024.0
                    right_text = f"Downloading: {pct:.2f}% ({d_mb:.2f}MB/{t_mb:.2f}MB)"
                except Exception:
                    pass

        # Pass the float percent so the progress display can show decimals
        widget.update_progress(pct, right_text)
        # If we detected postprocessing activity, explicitly ensure the
        # progress chunk remains the active/blue color so it doesn't go
        # colorless while yt-dlp is merging/removing files.
        try:
            if is_postprocessing:
                widget.progress.setStyleSheet(
                    "QProgressBar { border: 1px solid #bbb; border-radius: 6px; background: #eeeeee; }"
                    "QProgressBar::chunk { background-color: #3498db; border-radius: 6px; }"
                )
        except Exception:
            pass
        # Mark widget as started if we have meaningful progress or status
        try:
            if percent_int > 0 or "downloading" in (right_text or "").lower() or "starting" in (right_text or "").lower():
                widget._started = True
        except Exception:
            pass

    def _maybe_set_title_from_progress(self, widget: DownloadItemWidget, data):
        """Inspect progress text lines for patterns that reveal the title/filename.

        Expected `data` from `DownloadWorker.progress` to be a dict with a `text` key.
        When a 'Destination:' or 'Merging formats into' line is seen, extract a clean
        title and update the widget title to 'Downloading: <title>'.
        """
        try:
            if not isinstance(data, dict):
                return
            text = data.get("text") or data.get("line") or ""
            if not text:
                return
            # Look for destination/merge lines which include the filename
            import re, os
            m = re.search(r"Destination:\s*(.+)$", text)
            if not m:
                m = re.search(r"Merging formats into\s*(.+)$", text)
            if m:
                fname = m.group(1).strip().strip('\\"')
                # If it's a path, take basename
                base = os.path.basename(fname)
                # Clean to a display title
                title_clean = self._clean_display_title(base)
                # If current widget title is still a placeholder (contains the URL), update it
                cur = widget.title_label.text() or ""
                # Allow replacing both the initial 'Fetching Title:' placeholder
                # and a 'Queued:' placeholder when the real destination is discovered
                if cur.startswith("Fetching Title:") or cur.startswith("Queued:") or cur == widget.url or cur.strip() == widget.url:
                    # Show only the clean video title
                    widget.title_label.setText(title_clean)
        except Exception:
            log.debug("_maybe_set_title_from_progress failed to parse title", exc_info=True)

    def _clean_display_title(self, name: str) -> str:
        """Return a clean video title suitable for display in the UI.

        Steps:
        - If input looks like a filename, remove extension.
        - Strip trailing bracketed metadata groups like " [uploader] [date][id]".
        - Trim whitespace.
        """
        try:
            import re, os
            # Remove surrounding quotes
            s = (name or "").strip().strip('"')
            # If a path, take basename
            s = os.path.basename(s)
            # Remove extension
            s, _ = os.path.splitext(s)
            # Remove trailing bracketed groups
            s = re.sub(r"(\s*\[.*?\])+$", "", s).strip()
            return s
        except Exception:
            return (name or "").strip()

    def _strip_title_prefix(self, title: str) -> str:
        """Remove common UI prefixes from a title string to get the raw title."""
        if not title:
            return ""
        prefixes = (
            "Downloading: ",
            "Fetching Title: ",
            "Queued: ",
            "Fetching title: ",
            "Completed: ",
            "Download Complete: ",
            "Failed: ",
            "Cancelled: ",
        )
        for p in prefixes:
            if title.startswith(p):
                return title[len(p):].strip()
        return title

    def _on_title_updated(self, widget: DownloadItemWidget, worker, title: str):
        """Handle title updates from a worker.

        If the worker is not running yet (queued), show a queued status prefix.
        Otherwise show only the cleaned title.
        """
        try:
            title_clean = self._clean_display_title(title)
            # If worker isn't running, it's queued — show 'Queued: <title>' instead of placeholder
            running = False
            try:
                running = bool(getattr(worker, "isRunning") and worker.isRunning())
            except Exception:
                try:
                    running = bool(getattr(worker, "is_running", False))
                except Exception:
                    running = False

            if running:
                widget.title_label.setText(title_clean)
            else:
                widget.title_label.setText(f"Queued: {title_clean}")
        except Exception:
            try:
                widget.title_label.setText(str(title))
            except Exception:
                pass


    def _on_worker_finished(self, widget: DownloadItemWidget, worker, success: bool):
        """Worker finished handler — mark completed or failed."""
        # try to obtain best title available (prefer worker's video_title)
        raw_title = getattr(worker, "video_title", None) or getattr(worker, "title", None) or widget.title_label.text()
        # strip any UI prefixes like 'Downloading:' or 'Fetching Title:' to obtain clean title
        title = self._strip_title_prefix(raw_title)
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
        item_widget.resume_requested.connect(self._resume_download)
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

    def _resume_download(self, url):
        """Signal up to main window to resume."""
        # For now, resume == retry (restart download). Main can implement smarter resume.
        # Immediately update UI to active state so user sees feedback
        try:
            if url in self.active_items:
                try:
                    self.active_items[url].mark_active()
                except Exception:
                    pass
        except Exception:
            pass
        self.main.resume_download(url)

    def _check_if_all_done(self):
        """Emit all_downloads_complete if all active items are finished."""
        if all(
            self.active_items[url].percent_label.text() in ("100%", "Cancelled", "Error")
            for url in self.active_items
        ):
            log.debug("All downloads complete, emitting signal.")
            self.all_downloads_complete.emit()

    def _on_open_downloads_clicked(self):
        """Safely open the downloads folder via Advanced tab."""
        adv = getattr(self.main, "tab_advanced", None)
        if adv and hasattr(adv, "open_downloads_folder"):
            adv.open_downloads_folder()
        else:
            try:
                QMessageBox.warning(
                    self,
                    "Unavailable",
                    "Advanced Settings are not yet initialized. Please try again after the app finishes loading."
                )
            except Exception:
                pass
