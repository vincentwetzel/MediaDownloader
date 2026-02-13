import logging
import os
from PyQt6.QtWidgets import QMessageBox, QDialog, QVBoxLayout, QTextEdit, QPushButton
import re

log = logging.getLogger(__name__)

class ArchiveManager:
    def __init__(self, config_manager, parent_widget=None):
        self.config_manager = config_manager
        self.parent_widget = parent_widget
        self.archive_filename = "download_archive.txt"

    def get_archive_path(self):
        """Returns the full path to the download archive file."""
        config_dir = self.config_manager.get_config_dir()
        if not config_dir:
            return None
        return os.path.join(config_dir, self.archive_filename)

    def add_to_archive(self, url):
        """Adds a video ID to the archive file."""
        if self.config_manager.get("General", "download_archive", "False") != "True":
            return

        archive_path = self.get_archive_path()
        if not archive_path:
            return

        video_id = self._extract_video_id(url)
        if not video_id:
            log.warning(f"Could not extract video ID from URL: {url}")
            return

        try:
            with open(archive_path, "a", encoding="utf-8") as f:
                f.write(f"youtube {video_id}\n")
            log.info(f"Added to archive: youtube {video_id}")
        except Exception as e:
            log.error(f"Failed to write to archive file: {e}")

    def _extract_video_id(self, url):
        """Extracts the YouTube video ID from a URL."""
        patterns = [
            r"(?:v=|\/)([0-9A-Za-z_-]{11}).*",
            r"youtu\.be\/([0-9A-Za-z_-]{11})"
        ]
        for pattern in patterns:
            match = re.search(pattern, url)
            if match:
                return match.group(1)
        return None

    def view_archive(self):
        """Displays the contents of the archive file in a dialog."""
        archive_path = self.get_archive_path()
        if not archive_path or not os.path.exists(archive_path):
            QMessageBox.information(
                self.parent_widget,
                "Archive Not Found",
                "The download archive file does not exist yet. It will be created when you download a file with the archive option enabled."
            )
            return

        try:
            with open(archive_path, "r", encoding="utf-8") as f:
                content = f.read()

            dialog = QDialog(self.parent_widget)
            dialog.setWindowTitle("Download Archive")
            dialog.setGeometry(100, 100, 700, 500)
            
            layout = QVBoxLayout()
            
            text_edit = QTextEdit()
            text_edit.setReadOnly(True)
            text_edit.setPlainText(content)
            
            close_btn = QPushButton("Close")
            close_btn.clicked.connect(dialog.accept)
            
            layout.addWidget(text_edit)
            layout.addWidget(close_btn)
            
            dialog.setLayout(layout)
            dialog.exec()

        except Exception as e:
            log.error(f"Failed to read or display archive file: {e}")
            QMessageBox.critical(
                self.parent_widget,
                "Error",
                f"An error occurred while trying to view the archive file:\n\n{str(e)}"
            )

    def clear_archive(self):
        """Deletes the archive file after confirmation."""
        archive_path = self.get_archive_path()
        if not archive_path or not os.path.exists(archive_path):
            QMessageBox.information(
                self.parent_widget,
                "Archive Not Found",
                "The download archive is already empty."
            )
            return

        reply = QMessageBox.question(
            self.parent_widget,
            "Confirm Clear",
            "Are you sure you want to clear the download archive? This will remove the record of all previously downloaded files.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            try:
                os.remove(archive_path)
                log.info(f"Download archive cleared: {archive_path}")
                QMessageBox.information(
                    self.parent_widget,
                    "Archive Cleared",
                    "The download archive has been successfully cleared."
                )
            except Exception as e:
                log.error(f"Failed to clear archive file: {e}")
                QMessageBox.critical(
                    self.parent_widget,
                    "Error",
                    f"An error occurred while trying to clear the archive file:\n\n{str(e)}"
                )
