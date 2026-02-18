import logging
import os
import time
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QProgressBar, QPushButton, QListWidget,
    QListWidgetItem, QMessageBox, QSizePolicy
)
from PyQt6.QtCore import Qt, pyqtSignal, QObject, QUrl
from PyQt6.QtGui import QDesktopServices

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
        self.cancel_btn.setToolTip("Cancel or resume the download, or retry after a failure.")
        top.addWidget(self.cancel_btn)
        
        # Open Folder button (hidden initially)
        self.open_folder_btn = QPushButton("Open Folder")
        self.open_folder_btn.setFixedWidth(90)
        self.open_folder_btn.clicked.connect(self._on_open_folder_clicked)
        self.open_folder_btn.setToolTip("Open the folder containing this file.")
        self.open_folder_btn.setVisible(False)
        top.addWidget(self.open_folder_btn)

        layout.addLayout(top)

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
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
        self._set_progress_style("active")
        # Track last numeric percent shown so transient postprocessing
        # messages (which often arrive with no percent) don't cause the
        # bar to drop back to 0 after reaching 100%.
        self._last_percent = 0.0
        # Track whether we're currently in postprocessing (merging,
        # modifying chapters, embedding, deleting originals, etc.). When
        # True, keep the progress chunk visually 'active' (blue) until
        # the worker signals finished().
        self._postprocessing_active = False
        # Track what yt-dlp is currently downloading so subtitle/auxiliary
        # transfers cannot drive the main media progress bar.
        self._last_destination_kind = None  # "media" | "subtitle" | "aux"
        self._saw_primary_destination = False
        # HLS/aria2 progress tracking fallback:
        # derive progress from "Total fragments: N" + ".part-FragK" lines.
        self._hls_total_fragments = None
        self._hls_max_frag_index = -1
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
        
        # If percent is None, it means we received a status update without
        # a numeric progress (e.g. a warning or info line). Preserve the
        # current progress bar value.
        if percent is None:
            val = self._last_percent
        else:
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
                "sponsor", "sponsorblock", "embed", "thumbnail", "tag", "fixup"
            )
            is_postprocessing = any(k in low_text for k in post_keys) or ("extracting audio" in low_text)
            # Remember that we're in postprocessing until finished().
            if is_postprocessing:
                self._postprocessing_active = True
            else:
                # If we see normal downloading progress below 100%, clear
                # the postprocessing flag.
                try:
                    if val < 100.0 and percent is not None:
                        self._postprocessing_active = False
                except Exception:
                    pass
            
            # Keep progress monotonic for an active item so the bar/text
            # never stutter backwards due to noisy yt-dlp output lines.
            if val > self._last_percent:
                self._last_percent = val

            # Determine display value based on monotonic percent or postprocessing state
            if is_postprocessing and self._last_percent >= 100.0:
                display_val = int(round(self._last_percent))
            else:
                display_val = int(round(self._last_percent))
            
            render_text = str(text)
            if self._last_render_value == display_val and self._last_render_text == render_text:
                return

            self.progress.setValue(display_val)

            # Ensure the text also reflects the monotonic percent if it contains "Downloading: XX.X%"
            if "downloading:" in low_text:
                import re
                # Replace the percentage in the text with the monotonic _last_percent
                # Pattern matches "Downloading: 31.2%" or "Downloading: 31%"
                # Use \g<1> to preserve the case of "Downloading: "
                text = re.sub(
                    r"(downloading:\s*)\d+(?:\.\d+)?%", 
                    f"\\g<1>{self._last_percent:.1f}%", 
                    str(text), 
                    flags=re.IGNORECASE
                )
            
        except Exception:
            try:
                self.progress.setValue(int(val))
            except Exception:
                self.progress.setValue(0)

        # Use the provided short text as the progress bar format so it
        # appears centered inside the bar. If not provided, show percent
        # with two decimals.
        try:
            render_text = str(text)
            self.progress.setFormat(render_text)
            self._last_render_value = self.progress.value()
            self._last_render_text = render_text
        except Exception:
            try:
                self.progress.setFormat(f"{val:.1f}%")
            except Exception:
                self.progress.setFormat("0.0%")

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
            if any(k in low for k in post_keys) or "extracting audio" in low:
                self._set_progress_style("active")
        except Exception:
            pass

    def mark_completed(self, title, final_path=None):
        """Mark this item as completed."""
        # Show only the video title (no 'Download Complete:' prefix)
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self.progress.setValue(100)
        self._set_progress_style("completed")
        # Use friendly 'Done' label instead of numeric 100%
        self.progress.setFormat("Done")
        self._last_render_value = 100
        self._last_render_text = "Done"
        self.cancel_btn.setVisible(False)
        
        # Show Open Folder button if path is available
        if final_path:
            self._final_path = final_path
            self.open_folder_btn.setVisible(True)

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
            self._set_progress_style("error")
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
            self._set_progress_style("error")
            try:
                self.cancel_btn.setText("Retry")
                try:
                    self.cancel_btn.clicked.disconnect()
                except Exception:
                    pass
                self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))
            except Exception:
                pass

    def mark_failed(self, title, error_message=None):
        """Mark as failed, allows retry."""
        # Preserve just the video title; show error state on the bar
        try:
            self.title_label.setText(title)
        except Exception:
            self.title_label.setText(str(title))
        self._set_progress_style("error")
        
        # Use specific error message if provided, otherwise generic "Error"
        if error_message:
            # Truncate if too long to fit in progress bar
            display_msg = error_message
            if len(display_msg) > 40:
                display_msg = display_msg[:37] + "..."
            self.progress.setFormat(display_msg)
            self.progress.setToolTip(error_message)
        else:
            self.progress.setFormat("Error")
            
        self.cancel_btn.setText("Retry")
        self.cancel_btn.clicked.disconnect()
        self.cancel_btn.clicked.connect(lambda: self.retry_requested.emit(self.url))

    def _on_cancel_clicked(self):
        self.cancel_requested.emit(self.url)
        
    def _on_open_folder_clicked(self):
        if self._final_path and os.path.exists(self._final_path):
            try:
                # If it's a file, select it in explorer
                if os.path.isfile(self._final_path):
                    # Windows: explorer /select,"path"
                    if os.name == 'nt':
                        import subprocess
                        subprocess.run(['explorer', '/select,', os.path.normpath(self._final_path)])
                    else:
                        # Fallback for other OS (just open parent folder)
                        QDesktopServices.openUrl(QUrl.fromLocalFile(os.path.dirname(self._final_path)))
                else:
                    # It's a directory, just open it
                    QDesktopServices.openUrl(QUrl.fromLocalFile(self._final_path))
            except Exception:
                log.exception(f"Failed to open folder: {self._final_path}")
        else:
            # Fallback if path is missing or invalid
            log.warning(f"Cannot open folder, path invalid: {self._final_path}")

    def mark_active(self):
        """Set widget to active/downloading state (blue chunk, Cancel button)."""
        try:
            self._set_progress_style("active")
        except Exception:
            pass
        try:
            self.cancel_btn.setText("Cancel")
            try:
                self.cancel_btn.clicked.disconnect()
            except Exception:
                pass
            self.cancel_btn.clicked.connect(self._on_cancel_clicked)
            self.cancel_btn.setVisible(True) # Ensure cancel button is visible
        except Exception:
            pass
        # Reset started flag until progress arrives
        try:
            self._started = False
        except Exception:
            pass
        # Reset last percent so new attempts start fresh
        self._last_percent = 0.0
        self._last_render_value = None
        self._last_render_text = None
        self._hls_total_fragments = None
        self._hls_max_frag_index = -1
        self.open_folder_btn.setVisible(False)


class ActiveDownloadsTab(QWidget):
    """Manages list of active, queued, and completed downloads."""

    all_downloads_complete = pyqtSignal()
    _PROGRESS_UPDATE_MIN_INTERVAL_SEC = 0.1
    _PROGRESS_DUPLICATE_INTERVAL_SEC = 0.35

    def __init__(self, main_window):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        self.active_items = {}  # item_id -> DownloadItemWidget
        self._url_to_item_ids = {}  # url -> [item_id, ...]
        self._item_id_counter = 0
        self._build_tab_active()

    def _new_item_id(self) -> str:
        self._item_id_counter += 1
        return f"item-{self._item_id_counter}"

    def _register_widget(self, item_id: str, url: str, widget: DownloadItemWidget):
        self.active_items[item_id] = widget
        self._url_to_item_ids.setdefault(url, []).append(item_id)

    def _unregister_item_id(self, item_id: str):
        widget = self.active_items.get(item_id)
        if not widget:
            return

        url = getattr(widget, "url", None)
        if url:
            ids = self._url_to_item_ids.get(url, [])
            if item_id in ids:
                ids.remove(item_id)
            if not ids and url in self._url_to_item_ids:
                del self._url_to_item_ids[url]

        if item_id in self.active_items:
            del self.active_items[item_id]

    def _iter_widgets_for_url(self, url: str):
        for item_id in self._url_to_item_ids.get(url, []):
            w = self.active_items.get(item_id)
            if w:
                yield w

    def _is_terminal_widget(self, widget: DownloadItemWidget) -> bool:
        return ((widget.progress.format() or "").strip() in ("Done", "Cancelled", "Error"))

    def _remove_widget_instance(self, widget: DownloadItemWidget):
        """Remove a widget instance from list + internal maps."""
        if not widget:
            return
        for idx in range(self.list_widget.count()):
            it = self.list_widget.item(idx)
            w = self.list_widget.itemWidget(it)
            if w is widget:
                self.list_widget.takeItem(idx)
                break

        item_id = getattr(widget, "_item_id", None)
        if item_id:
            self._unregister_item_id(item_id)

    def cleanup_playlist_status_items(self, playlist_url: str):
        """Remove stale playlist status rows for a specific playlist URL."""
        try:
            victims = []
            for w in list(self._iter_widgets_for_url(playlist_url)):
                txt = (w.title_label.text() or "").strip().lower()
                if txt.startswith("preparing playlist") or txt.startswith("calculating playlist"):
                    victims.append(w)
            for w in victims:
                self._remove_widget_instance(w)
        except Exception:
            log.exception("Failed to cleanup playlist status items")

    def _pick_widget_for_url(self, url: str, prefer_unbound=False):
        widgets = list(self._iter_widgets_for_url(url))
        if not widgets:
            return None

        if prefer_unbound:
            for w in widgets:
                if not hasattr(w, "worker") and not self._is_terminal_widget(w):
                    return w

        for w in widgets:
            if not self._is_terminal_widget(w):
                return w

        return widgets[0]

    def ensure_placeholder_for_url(self, url: str):
        """Return a live placeholder for URL, creating one if needed."""
        w = self._pick_widget_for_url(url, prefer_unbound=True)
        if w and not self._is_terminal_widget(w):
            return w
        return self.add_placeholder(url)

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
        Accepts a worker object from core.yt_dlp_worker.
        Creates (or reuses) a DownloadItemWidget and wires signals.
        For worker-backed rows, never reuse terminal rows (Done/Cancelled/Error);
        this ensures re-downloads always get a new UI row.
        """
        url = None
        title = None
        worker = None

        if isinstance(worker_or_url, str):
            url = worker_or_url
        else:
            worker = worker_or_url
            url = getattr(worker, "url", None)

        if not url:
            url = str(worker_or_url)

        widget = self._pick_widget_for_url(url, prefer_unbound=True)
        # If the only match is a terminal row for this URL, create a fresh row
        # so repeated downloads are shown as distinct entries.
        if widget is None or self._is_terminal_widget(widget):
            widget = self.add_placeholder(url)

        if worker is not None:
            try:
                # Explicitly connect to the known signals of DownloadWorker
                worker.progress.connect(lambda data, w=widget: self._on_worker_progress(w, data))
                worker.progress.connect(lambda data, w=widget: self._maybe_set_title_from_progress(w, data))
                worker.title_updated.connect(lambda t, w=widget, wr=worker: self._on_title_updated(w, wr, t))
                # Note: worker.finished is handled by DownloadManager signal in MainWindow
                worker.error.connect(lambda url, msg, w=widget, wr=worker: self._on_worker_failed(w, wr, msg))

                # Store worker reference on widget for speed calculation if needed
                widget.worker = worker

                widget.mark_active()
            except Exception as e:
                log.error(f"Failed to connect signals for worker {url}: {e}")

        return widget


    # ---------- helper callbacks for wiring workers ----------

    def _on_worker_progress(self, widget: DownloadItemWidget, data):
        """
        Accepts either dict-like progress updates or plain strings from yt-dlp.
        Tries to extract a numeric percent and a short human text.
        """
        pct = None
        text = ""
        frag_idx = None
        frag_total = None

        # dict-like (common)
        if isinstance(data, dict):
            # common keys
            pct = data.get("percent")
            if pct is None:
                pct = data.get("pct")
            if pct is None:
                pct = data.get("progress")
            
            # sometimes percent is string "12.3" or embedded in a text line
            if pct is not None:
                try:
                    pct = float(pct)
                except Exception:
                    pct = None
            
            text = data.get("text") or data.get("status") or data.get("msg") or ""
            # If percent not provided but we have a textual progress line, try to extract
            # a percentage like '12.3%' from that text (yt-dlp emits progress in text lines).
            if pct is None and isinstance(text, str) and text:
                try:
                    import re
                    m = re.search(r"(\d{1,3}(?:\.\d+)?)\s?%", text)
                    if m:
                        pct = float(m.group(1))
                except Exception:
                    pass
            # Fallback display text
            if not text and pct is not None:
                text = f"{pct:.1f}%"

            # Update worker's current speed for global indicator
            if hasattr(widget, 'worker'):
                speed = data.get("speed_bytes", 0.0)
                widget.worker.current_speed = speed

        else:
            # fallback: try extract number from text
            text = str(data)
            import re
            m = re.search(r"(\d{1,3}(?:\.\d+)?)\s?%", text)
            if m:
                try:
                    pct = float(m.group(1))
                except Exception:
                    pct = None
            else:
                pct = None

        raw_text = (text or "").strip()
        low_raw = raw_text.lower()

        # HLS + aria2 fallback percent source:
        # - "[hlsnative] Total fragments: N"
        # - "FILE: ...part-FragK"
        # This remains accurate even when aria2 byte summaries are noisy.
        try:
            import re
            total_match = re.search(r"total fragments:\s*(\d+)", raw_text, re.IGNORECASE)
            if total_match:
                total = int(total_match.group(1))
                if total > 0:
                    widget._hls_total_fragments = total

            frag_file_match = re.search(r"\.part-frag(\d+)\b", raw_text, re.IGNORECASE)
            if frag_file_match:
                frag_idx = int(frag_file_match.group(1))
                prev_max = int(getattr(widget, "_hls_max_frag_index", -1))
                if frag_idx > prev_max:
                    widget._hls_max_frag_index = frag_idx
                total = int(getattr(widget, "_hls_total_fragments", 0) or 0)
                if total > 0:
                    frag_pct = (float(widget._hls_max_frag_index + 1) / float(total)) * 100.0
                    if pct is None or frag_pct > float(pct):
                        pct = frag_pct
        except Exception:
            pass

        # aria2 can emit many noisy status lines (per-connection chunks, FILE lines,
        # separators, summary banners) that do not contain a usable percent. If these
        # are rendered, they overwrite the last valid percentage text and make the UI
        # appear to stop showing progress.
        try:
            import re
            is_aria2_noise = (
                pct is None and bool(raw_text) and (
                    raw_text.startswith("FILE: ") or
                    raw_text.startswith("[#") or
                    raw_text.startswith("*** Download Progress Summary") or
                    re.fullmatch(r"[-=]{20,}", raw_text) is not None
                )
            )
            if is_aria2_noise:
                return
        except Exception:
            pass

        # Track whether this progress stream refers to media, subtitles, or aux files.
        try:
            import re
            dest_match = re.search(r"destination:\s*(.+)$", raw_text, re.IGNORECASE)
            if dest_match:
                dest_path = dest_match.group(1).strip().strip('"')
                dest_low = dest_path.lower()
                _, ext = os.path.splitext(dest_low)
                subtitle_exts = {".vtt", ".srt", ".ass", ".ssa", ".ttml", ".lrc", ".sbv", ".json3"}
                aux_exts = {".webp", ".jpg", ".jpeg", ".png", ".gif", ".description", ".ytdl"}
                if ext in subtitle_exts:
                    widget._last_destination_kind = "subtitle"
                elif dest_low.endswith(".info.json") or ext == ".json" or ext in aux_exts:
                    widget._last_destination_kind = "aux"
                elif ext == ".part":
                    # .part is the in-progress media payload for many yt-dlp/aria2 flows.
                    # Treat it as media so transfer percentages are not suppressed.
                    widget._last_destination_kind = "media"
                    widget._saw_primary_destination = True
                else:
                    widget._last_destination_kind = "media"
                    widget._saw_primary_destination = True
            elif "downloading subtitles" in low_raw or "[subtitlesconvertor]" in low_raw:
                widget._last_destination_kind = "subtitle"
        except Exception:
            pass

        # For fragment-based downloads (e.g. HLS), yt-dlp can emit transient
        # lines like "100.0% ... (frag 0/48)" before real transfer starts.
        # If we accept that 100%, monotonic UI logic will pin the bar at 100.
        try:
            import re
            frag_match = re.search(r"\(frag\s+(\d+)\s*/\s*(\d+)\)", text or "", re.IGNORECASE)
            if frag_match:
                frag_idx = int(frag_match.group(1))
                frag_total = int(frag_match.group(2))
                if frag_total > 0 and pct is None:
                    pct = (float(frag_idx) / float(frag_total)) * 100.0
                elif frag_total > 0 and pct is not None and pct >= 99.9 and frag_idx < frag_total:
                    pct = (float(frag_idx) / float(frag_total)) * 100.0
        except Exception:
            pass

        # Ignore percent lines from subtitle/auxiliary phases and suppress
        # pre-media 100% spikes (common before the main media destination appears).
        try:
            if pct is not None:
                destination_kind = getattr(widget, "_last_destination_kind", None)
                saw_primary = bool(getattr(widget, "_saw_primary_destination", False))
                is_sub_or_aux = destination_kind in ("subtitle", "aux") or ("subtitle" in low_raw)
                if is_sub_or_aux:
                    pct = None
                elif not saw_primary and pct >= 99.9 and "(frag" not in low_raw:
                    pct = None
        except Exception:
            pass

        # Clamp and keep percent monotonic for this widget so transient
        # parser noise cannot make display text move backward.
        if pct is not None:
            try:
                pct = max(0.0, min(100.0, float(pct)))
                last_pct = float(getattr(widget, "_last_percent", 0.0))
                if pct < last_pct:
                    pct = last_pct
            except Exception:
                pct = None

        # clamp (integer value for the progress bar widget)
        try:
            if pct is not None:
                percent_int = max(0, min(100, int(round(pct))))
            else:
                percent_int = 0
        except Exception:
            percent_int = 0

        # Build display text for the progress bar. Prefer user-friendly
        # status messages (use yt-dlp verbiage when available). When a
        # numeric percent is present prefer a two-decimal display.
        right_text = f"{pct:.1f}%" if pct is not None else ""
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
            "postproc", "post-process", "merger", "fixup"
        )
        is_postprocessing = any(k in low for k in post_keys) or ("extracting audio" in low)

        # Map certain yt-dlp lines to friendly short statuses. If we're in a
        # postprocessing state prefer that message over a numeric percent so
        # the UI can keep the active/blue styling until the worker signals done.
        if is_postprocessing or "merging" in low:
            right_text = "Postprocessing..."
        elif "destination:" in low or "preparing" in low:
            # e.g. Destination: <file>
            right_text = "Preparing Download..."
        # If we have a numeric percent and we're NOT in postprocessing, prefer
        # the concise downloading line using the numeric percent.
        elif percent_int > 0 and not is_postprocessing:
            try:
                right_text = f"Downloading: {pct:.1f}%"
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
                    right_text = f"Downloading: {pct:.1f}% ({d_mb:.2f}MB/{t_mb:.2f}MB)"
                except Exception:
                    pass

        # Throttle redundant GUI updates. During concurrent postprocessing
        # yt-dlp can emit many near-identical lines, which can saturate the
        # main thread with paint/layout work.
        now = time.monotonic()
        try:
            pct_bucket = percent_int if pct is not None else -1
        except Exception:
            pct_bucket = -1
        render_text = str(right_text or "")
        last_ts = float(getattr(widget, "_last_ui_update_ts", 0.0) or 0.0)
        last_pct = int(getattr(widget, "_last_ui_pct_bucket", -2))
        last_text = str(getattr(widget, "_last_ui_text", "") or "")
        if pct_bucket == last_pct and render_text == last_text:
            if now - last_ts < self._PROGRESS_DUPLICATE_INTERVAL_SEC:
                return
        elif pct_bucket == last_pct and now - last_ts < self._PROGRESS_UPDATE_MIN_INTERVAL_SEC:
            return
        widget._last_ui_update_ts = now
        widget._last_ui_pct_bucket = pct_bucket
        widget._last_ui_text = render_text

        # Pass the float percent so the progress display can show decimals
        widget.update_progress(pct, right_text)
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
            # The calling context should handle path/basename separation.
            # This function should focus on cleaning a filename/title string.
            # s = re.split(r'[/\\]', s)[-1] # THIS LINE IS THE BUG
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
        # This is called by the worker signal, but we want to wait for the move job to finish
        # and provide the final path.
        # Actually, the worker finishes BEFORE the move job.
        # The move job is triggered in DownloadManager._on_worker_finished.
        # So we shouldn't mark completed here if we want the final path.
        # BUT, the UI needs to know it's done.
        # We can mark it as "Processing..." or similar?
        # Or we can just rely on the DownloadManager signal which comes later?
        # Currently DownloadManager emits download_finished AFTER move job starts?
        # No, let's check DownloadManager.
        pass


    def _on_worker_failed(self, widget: DownloadItemWidget, worker, err_text: str):
        """Explicit error path (if worker emits error_occurred)."""
        title = self._strip_title_prefix(widget.title_label.text())
        # show error on widget and allow retry
        widget.mark_failed(title, err_text)
        # Reset speed on failure
        if hasattr(widget, 'worker'):
            widget.worker.current_speed = 0.0

        # optionally pop up a message to user
        try:
            QMessageBox.warning(self, "Download error", f"{title}\n\n{err_text}")
        except Exception:
            pass


    # --- item management ---
    def add_placeholder(self, url):
        """Add a placeholder entry while fetching title."""
        item_id = self._new_item_id()
        item_widget = DownloadItemWidget(url)
        item_widget.cancel_requested.connect(self._cancel_download)
        item_widget.retry_requested.connect(self._retry_download)
        item_widget.resume_requested.connect(self._resume_download)
        lw_item = QListWidgetItem()
        lw_item.setSizeHint(item_widget.sizeHint())
        self.list_widget.addItem(lw_item)
        self.list_widget.setItemWidget(lw_item, item_widget)
        item_widget._item_id = item_id
        self._register_widget(item_id, url, item_widget)
        return item_widget

    def replace_placeholder_with_entries(self, old_url, new_urls):
        """Replace a single placeholder for `old_url` with placeholders for
        each URL in `new_urls`. Keeps ordering by inserting new items where
        the old placeholder was located.
        """
        try:
            old_widget = self.active_items.get(old_url)
            insert_pos = None
            # Find the QListWidgetItem index for the old widget
            for idx in range(self.list_widget.count()):
                it = self.list_widget.item(idx)
                w = self.list_widget.itemWidget(it)
                if w is old_widget:
                    insert_pos = idx
                    break

            # Remove old widget and mapping
            if insert_pos is not None:
                try:
                    taken = self.list_widget.takeItem(insert_pos)
                    # delete the widget if possible
                    try:
                        old_widget.setParent(None)
                    except Exception:
                        pass
                except Exception:
                    pass
            if old_widget is not None:
                try:
                    old_item_id = getattr(old_widget, "_item_id", None)
                    if old_item_id:
                        self._unregister_item_id(old_item_id)
                except Exception:
                    pass

            # Insert new placeholders at the old position (or at end if not found)
            pos = insert_pos if insert_pos is not None else self.list_widget.count()
            for i, nu in enumerate(new_urls):
                try:
                    if isinstance(nu, dict):
                        entry_url = str(nu.get("url", "") or "")
                        entry_title = str(nu.get("title", "") or "").strip()
                    else:
                        entry_url = str(nu)
                        entry_title = ""

                    if not entry_url:
                        continue

                    item_widget = DownloadItemWidget(entry_url, title=(entry_title or None))
                    item_widget.cancel_requested.connect(self._cancel_download)
                    item_widget.retry_requested.connect(self._retry_download)
                    item_widget.resume_requested.connect(self._resume_download)
                    lw_item = QListWidgetItem()
                    lw_item.setSizeHint(item_widget.sizeHint())
                    self.list_widget.insertItem(pos + i, lw_item)
                    self.list_widget.setItemWidget(lw_item, item_widget)
                    item_id = self._new_item_id()
                    item_widget._item_id = item_id
                    self._register_widget(item_id, entry_url, item_widget)
                except Exception:
                    log.exception(f"Failed to create placeholder for {nu}")
        except Exception:
            log.exception("Error replacing playlist placeholder")

    def set_placeholder_message(self, url, message):
        """Set the title/label of an existing placeholder for `url` to `message`.
        No-op if the placeholder does not exist.
        """
        try:
            w = self._pick_widget_for_url(url)
            if not w:
                return
            try:
                w.title_label.setText(message)
            except Exception:
                try:
                    w.setWindowTitle(message)
                except Exception:
                    pass
        except Exception:
            log.exception("Failed to set placeholder message")

    def remove_placeholder(self, url):
        """Find and remove a placeholder widget by its URL."""
        try:
            widget = self._pick_widget_for_url(url, prefer_unbound=True) or self._pick_widget_for_url(url)
            if not widget:
                return

            # Find and remove the item from the list widget
            for idx in range(self.list_widget.count()):
                it = self.list_widget.item(idx)
                w = self.list_widget.itemWidget(it)
                if w is widget:
                    self.list_widget.takeItem(idx)
                    break
            
            # Clean up internal mapping
            item_id = getattr(widget, "_item_id", None)
            if item_id:
                self._unregister_item_id(item_id)

        except Exception:
            log.exception(f"Failed to remove placeholder for {url}")

    def update_progress(self, url, percent, text):
        """Update the download progress for a given URL."""
        w = self._pick_widget_for_url(url)
        if w:
            w.update_progress(percent, text)

    def mark_completed(self, url, title=None, final_path=None):
        w = self._pick_widget_for_url(url)
        if w:
            if not title:
                # If title not provided (e.g. from MainWindow signal), extract from widget
                # and strip any status prefixes
                current_text = w.title_label.text()
                title = self._strip_title_prefix(current_text)

            w.mark_completed(title, final_path)
        self._check_if_all_done()

    def mark_cancelled(self, url, title):
        w = self._pick_widget_for_url(url)
        if w:
            w.mark_cancelled(title)
        self._check_if_all_done()

    def mark_failed(self, url, title):
        w = self._pick_widget_for_url(url)
        if w:
            w.mark_failed(title)
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
            w = self._pick_widget_for_url(url)
            if w:
                try:
                    w.mark_active()
                except Exception:
                    pass
        except Exception:
            pass
        self.main.resume_download(url)

    def _check_if_all_done(self):
        """Emit all_downloads_complete if all active items are finished."""
        if all(
            ((self.active_items[item_id].progress.format() or "").strip() in ("Done", "Cancelled", "Error"))
            for item_id in self.active_items
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

