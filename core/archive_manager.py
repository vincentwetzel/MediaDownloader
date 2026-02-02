
import os
import logging
from threading import Lock

log = logging.getLogger(__name__)

class ArchiveManager:
    def __init__(self, archive_file='download_archive.txt'):
        # Place archive file in the parent directory of the 'core' directory
        # which is the project root.
        base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.archive_file = os.path.join(base_dir, archive_file)
        self.lock = Lock()
        self._ensure_file_exists()

    def _ensure_file_exists(self):
        if not os.path.exists(self.archive_file):
            try:
                with open(self.archive_file, 'w') as f:
                    pass  # create empty file
                log.info(f"Created archive file: {self.archive_file}")
            except IOError as e:
                log.error(f"Failed to create archive file: {e}")

    def is_in_archive(self, url):
        with self.lock:
            try:
                with open(self.archive_file, 'r') as f:
                    for line in f:
                        if url.strip() == line.strip():
                            return True
            except IOError as e:
                log.error(f"Failed to read from archive: {e}")
            return False

    def add_to_archive(self, url):
        with self.lock:
            try:
                with open(self.archive_file, 'a') as f:
                    f.write(url + '\n')
                log.info(f"Added to archive: {url}")
            except IOError as e:
                log.error(f"Failed to write to archive: {e}")
