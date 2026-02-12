import os
import logging
import sqlite3
import time
from threading import Lock

log = logging.getLogger(__name__)

class ArchiveManager:
    def __init__(self, db_file='download_archive.db'):
        # Place archive file in the parent directory of the 'core' directory
        # which is the project root.
        base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.db_file = os.path.join(base_dir, db_file)
        self.lock = Lock()
        self._init_db()

    def _init_db(self):
        try:
            with sqlite3.connect(self.db_file) as conn:
                cursor = conn.cursor()
                cursor.execute('''
                    CREATE TABLE IF NOT EXISTS downloads (
                        url TEXT PRIMARY KEY,
                        timestamp REAL
                    )
                ''')
                conn.commit()
        except sqlite3.Error as e:
            log.error(f"Failed to initialize archive database: {e}")

    def is_in_archive(self, url):
        with self.lock:
            try:
                with sqlite3.connect(self.db_file) as conn:
                    cursor = conn.cursor()
                    cursor.execute('SELECT 1 FROM downloads WHERE url = ?', (url.strip(),))
                    return cursor.fetchone() is not None
            except sqlite3.Error as e:
                log.error(f"Failed to check archive: {e}")
                return False

    def add_to_archive(self, url):
        with self.lock:
            try:
                with sqlite3.connect(self.db_file) as conn:
                    cursor = conn.cursor()
                    cursor.execute('INSERT OR REPLACE INTO downloads (url, timestamp) VALUES (?, ?)', (url.strip(), time.time()))
                    conn.commit()
                log.info(f"Added to archive: {url}")
            except sqlite3.Error as e:
                log.error(f"Failed to write to archive: {e}")

    def purge_old_entries(self, max_age_seconds=31536000): # Default: 365 days
        with self.lock:
            try:
                cutoff = time.time() - max_age_seconds
                with sqlite3.connect(self.db_file) as conn:
                    cursor = conn.cursor()
                    cursor.execute('DELETE FROM downloads WHERE timestamp < ?', (cutoff,))
                    deleted_count = cursor.rowcount
                    conn.commit()
                if deleted_count > 0:
                    log.info(f"Purged {deleted_count} old entries from archive.")
            except sqlite3.Error as e:
                log.error(f"Failed to purge old entries: {e}")
