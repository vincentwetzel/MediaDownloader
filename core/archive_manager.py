import logging
import os
import re
import sqlite3
import time
from threading import Lock
from urllib.parse import parse_qs, urlparse

log = logging.getLogger(__name__)


class ArchiveManager:
    """SQLite-backed archive manager."""

    def __init__(self, config_manager, parent_widget=None):
        self.config_manager = config_manager
        self.parent_widget = parent_widget
        self.db_filename = "download_archive.db"
        self._lock = Lock()

    def get_archive_path(self):
        """Compatibility alias; returns the SQLite archive path."""
        return self.get_archive_db_path()

    def get_archive_db_path(self):
        """Returns the full path to the SQLite archive database."""
        config_dir = self.config_manager.get_config_dir()
        if not config_dir:
            return None
        return os.path.join(config_dir, self.db_filename)

    def add_to_archive(self, url):
        """Record a URL in the SQLite archive."""
        db_path = self.get_archive_db_path()
        if not db_path:
            return

        provider, media_id, normalized = self._build_identity(url)
        if not normalized:
            log.warning(f"Could not normalize URL for archive: {url}")
            return

        try:
            with self._lock:
                with sqlite3.connect(db_path) as conn:
                    self._ensure_schema(conn)
                    conn.execute(
                        """
                        INSERT INTO downloads(url, normalized_url, provider, media_id, timestamp)
                        VALUES (?, ?, ?, ?, ?)
                        ON CONFLICT(url) DO UPDATE SET
                            normalized_url = excluded.normalized_url,
                            provider = excluded.provider,
                            media_id = excluded.media_id,
                            timestamp = excluded.timestamp
                        """,
                        (url, normalized, provider, media_id, time.time())
                    )
                    conn.commit()
            log.info(f"Added to archive DB: provider={provider} media_id={media_id} url={url}")
        except Exception as e:
            log.error(f"Failed writing to archive DB: {e}")

    def is_in_archive(self, url):
        """Return True if URL/media already exists in archive DB."""
        db_path = self.get_archive_db_path()
        if not db_path or not os.path.exists(db_path):
            return False

        provider, media_id, normalized = self._build_identity(url)
        if not normalized:
            return False

        try:
            with self._lock:
                with sqlite3.connect(db_path) as conn:
                    self._ensure_schema(conn)
                    cur = conn.cursor()
                    if provider == "youtube" and media_id:
                        row = cur.execute(
                            "SELECT 1 FROM downloads WHERE provider = ? AND media_id = ? LIMIT 1",
                            ("youtube", media_id)
                        ).fetchone()
                        if row:
                            return True
                    row = cur.execute(
                        "SELECT 1 FROM downloads WHERE normalized_url = ? OR url = ? LIMIT 1",
                        (normalized, url)
                    ).fetchone()
                    return bool(row)
        except Exception as e:
            log.warning(f"Archive lookup failed for {url}: {e}")
            return False

    def _ensure_schema(self, conn):
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS downloads (
                url TEXT PRIMARY KEY,
                normalized_url TEXT,
                provider TEXT,
                media_id TEXT,
                timestamp REAL
            )
            """
        )
        cols = {row[1] for row in conn.execute("PRAGMA table_info(downloads)").fetchall()}
        if "normalized_url" not in cols:
            conn.execute("ALTER TABLE downloads ADD COLUMN normalized_url TEXT")
        if "provider" not in cols:
            conn.execute("ALTER TABLE downloads ADD COLUMN provider TEXT")
        if "media_id" not in cols:
            conn.execute("ALTER TABLE downloads ADD COLUMN media_id TEXT")
        if "timestamp" not in cols:
            conn.execute("ALTER TABLE downloads ADD COLUMN timestamp REAL")
        self._backfill_identity_columns(conn)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_downloads_norm ON downloads(normalized_url)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_downloads_provider_media ON downloads(provider, media_id)")

    def _backfill_identity_columns(self, conn):
        """Populate normalized/provider/media_id for rows from older DB schema."""
        rows = conn.execute(
            """
            SELECT url, normalized_url, provider, media_id
            FROM downloads
            WHERE normalized_url IS NULL OR normalized_url = ''
               OR provider IS NULL OR provider = ''
               OR (provider = 'youtube' AND (media_id IS NULL OR media_id = ''))
            """
        ).fetchall()
        if not rows:
            return

        for row in rows:
            url = row[0] or ""
            provider, media_id, normalized = self._build_identity(url)
            conn.execute(
                """
                UPDATE downloads
                SET normalized_url = ?, provider = ?, media_id = ?
                WHERE url = ?
                """,
                (normalized, provider, media_id, url)
            )

    def _build_identity(self, url):
        """Return (provider, media_id, normalized_url) for consistent matching."""
        media_id = self._extract_video_id(url)
        normalized = self._normalize_url(url)
        if media_id:
            return "youtube", media_id, normalized
        return "generic", None, normalized

    def _extract_video_id(self, url):
        try:
            parsed = urlparse(url)
            host = (parsed.hostname or "").lower()
            if "youtube.com" in host:
                qs = parse_qs(parsed.query)
                vid = (qs.get("v") or [None])[0]
                if vid and re.fullmatch(r"[0-9A-Za-z_-]{11}", vid):
                    return vid
                m = re.search(r"/(?:shorts|live|embed)/([0-9A-Za-z_-]{11})(?:[/?#]|$)", parsed.path or "")
                if m:
                    return m.group(1)
            if "youtu.be" in host:
                seg = (parsed.path or "").strip("/").split("/")[0]
                if re.fullmatch(r"[0-9A-Za-z_-]{11}", seg):
                    return seg
        except Exception:
            pass

        patterns = [
            r"(?:v=|/)([0-9A-Za-z_-]{11}).*",
            r"youtu\.be/([0-9A-Za-z_-]{11})",
        ]
        for pattern in patterns:
            match = re.search(pattern, url)
            if match:
                return match.group(1)
        return None

    def _normalize_url(self, url):
        try:
            parsed = urlparse(url)
            host = (parsed.hostname or "").lower()
            path = re.sub(r"/+", "/", parsed.path or "/").rstrip("/")
            query = parse_qs(parsed.query, keep_blank_values=False)
            drop = {
                "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
                "si", "feature", "pp"
            }
            kept = []
            for key in sorted(query.keys()):
                if key.lower() in drop:
                    continue
                for val in sorted(query.get(key) or []):
                    kept.append(f"{key}={val}")
            q = ("?" + "&".join(kept)) if kept else ""
            if not host:
                return None
            return f"{host}{path}{q}".lower()
        except Exception:
            return None
