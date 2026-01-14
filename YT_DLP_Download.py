# YT_DLP_Download.py
# Worker backend using yt-dlp. Designed to be moved to a QThread.
# Emits primitive signals only. Includes logging and overwrite prompt mechanism.

import os
import traceback
import threading
import logging
from typing import Optional

from PyQt6.QtCore import QObject, pyqtSignal

import yt_dlp
import subprocess
import requests
import shutil
import tempfile

logger = logging.getLogger("YT_DLP_Download")
logger.setLevel(logging.DEBUG)
if not logger.handlers:
    ch = logging.StreamHandler()
    ch.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
    logger.addHandler(ch)


class YT_DLP_Download(QObject):
    # Signals (only primitives)
    progress_updated = pyqtSignal(float)    # percent 0-100
    status_updated = pyqtSignal(str)        # status text
    title_updated = pyqtSignal(str)         # video/playlist title
    error_occurred = pyqtSignal(str)        # error text
    prompt_overwrite = pyqtSignal(str)      # filepath (worker will wait for response)
    finished = pyqtSignal(bool)             # success boolean

    def __init__(
        self,
        raw_url: str,
        temp_dl_loc: str,
        final_destination_dir: str,
        download_mp3: bool = False,
        allow_playlist: bool = False,
        rate_limit: str = "0",
        video_quality: str = "best",
        video_ext: Optional[str] = None,
        audio_ext: Optional[str] = None,
        video_codec: Optional[str] = None,
        audio_codec: Optional[str] = None,
        restrict_filenames: bool = False,
        cookies: Optional[str] = None,
        audio_quality: str = "192k",
        sponsorblock: bool = True,
        use_part_files: bool = True,
    ):
        super().__init__()
        self.raw_url = raw_url
        self.temp_dl_loc = temp_dl_loc
        self.final_destination_dir = final_destination_dir
        self.download_mp3 = bool(download_mp3)
        self.allow_playlist = bool(allow_playlist)

        self.rate_limit = rate_limit or "0"
        self.video_quality = video_quality or "best"
        self.video_ext = video_ext or ""
        self.audio_ext = audio_ext or ""
        self.video_codec = video_codec or ""
        self.audio_codec = audio_codec or ""
        self.restrict_filenames = bool(restrict_filenames)
        self.cookies = cookies
        self.audio_quality = audio_quality
        self.sponsorblock = bool(sponsorblock)
        self.use_part_files = bool(use_part_files)

        self._cancelled = False
        self._prompt_event = None
        self._prompt_response = None
        self._prompt_lock = threading.Lock()

        self.video_title = ""
        self._success = False

        logger.debug("YT_DLP_Download initialized for %s (temp=%s final=%s)", self.raw_url, self.temp_dl_loc, self.final_destination_dir)

    def _progress_hook(self, d: dict):
        """Called frequently by yt-dlp; emits progress updates."""
        if self._cancelled:
            logger.debug("Progress hook: cancellation detected")
            raise RuntimeError("Download cancelled by user")

        status = d.get("status")
        if status == "downloading":
            downloaded = d.get("downloaded_bytes", 0) or 0
            total = d.get("total_bytes") or d.get("total_bytes_estimate")
            if total and total > 0:
                pct = (downloaded / total) * 100.0
                self.progress_updated.emit(float(pct))
                self.status_updated.emit(f"{pct:.1f}% ({downloaded} / {total})")
            else:
                # Unknown total
                self.status_updated.emit(f"Downloaded {downloaded} bytes")
        elif status == "finished":
            self.status_updated.emit("Download finished (processing)")

    def provide_overwrite_response(self, allow: bool):
        """Called by GUI to respond to a prompt; unblocks worker."""
        with self._prompt_lock:
            if self._prompt_event:
                self._prompt_response = bool(allow)
                logger.debug("provide_overwrite_response: %s", allow)
                self._prompt_event.set()

    def _ask_overwrite_blocking(self, filepath: str) -> bool:
        """Emit prompt_overwrite and block until GUI responds via provide_overwrite_response."""
        ev = threading.Event()
        with self._prompt_lock:
            self._prompt_event = ev
            self._prompt_response = None
        logger.debug("Asking GUI to confirm overwrite: %s", filepath)
        self.prompt_overwrite.emit(filepath)
        ev.wait()
        with self._prompt_lock:
            resp = bool(self._prompt_response)
            self._prompt_event = None
            self._prompt_response = None
        logger.debug("Overwrite decision received: %s", resp)
        return resp

    def start_yt_download(self):
        """Run yt-dlp to extract metadata and perform download. This should run in a QThread."""
        logger.debug("start_yt_download called for %s", self.raw_url)
        try:
            # ensure directories exist
            os.makedirs(self.temp_dl_loc, exist_ok=True)
            os.makedirs(self.final_destination_dir, exist_ok=True)
            # ensure working directories exist
            os.makedirs(self.temp_dl_loc, exist_ok=True)
            os.makedirs(self.final_destination_dir, exist_ok=True)
            # cleanup any leftover debug thumbnail files from older runs
            try:
                for d in (self.temp_dl_loc, self.final_destination_dir):
                    if d and os.path.isdir(d):
                        for fn in os.listdir(d):
                            if fn.startswith("preferred_thumb_") and fn.lower().endswith(('.jpg', '.jpeg', '.png')):
                                try:
                                    os.remove(os.path.join(d, fn))
                                    logger.debug("Removed leftover debug thumbnail: %s", fn)
                                except Exception:
                                    pass
            except Exception:
                pass

            ydl_opts = {
                "quiet": True,
                "no_warnings": True,
                "progress_hooks": [self._progress_hook],
                "paths": {"home": self.final_destination_dir, "temp": self.temp_dl_loc},
                # Use the requested filename template (title truncated to 90 chars,
                # uploader truncated to 30 chars, and include upload date and id).
                "outtmpl": "%(title).90s [%(uploader).30s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s",
                "windowsfilenames": True,
            }

            # Always request the thumbnail be written so it can be embedded or kept
            ydl_opts["writethumbnail"] = True
            # Convert thumbnails to a common format to avoid embedding unsupported types
            ydl_opts["convert_thumbnails"] = "jpg"

            if not self.use_part_files:
                ydl_opts["nopart"] = True

            if self.restrict_filenames:
                ydl_opts["restrictfilenames"] = True

            if self.cookies:
                ydl_opts["cookiefile"] = self.cookies
                logger.debug("Using cookies file: %s", self.cookies)

            if self.sponsorblock:
                ydl_opts["sponsorblock_remove"] = ["sponsor", "intro", "outro", "selfpromo", "interaction", "preview", "music_offtopic"]
                logger.debug("SponsorBlock enabled")

            # Rate limit (pass-through; assume user supplies a correct yt-dlp acceptable string)
            if self.rate_limit and self.rate_limit not in ("0", "", "no limit", "No limit"):
                ydl_opts["ratelimit"] = self.rate_limit
                logger.debug("Setting ratelimit: %s", self.rate_limit)

            # Format selection
            if self.download_mp3:
                # prefer best audio and postprocess to audio_ext
                ydl_opts["format"] = f"bestaudio[ext={self.audio_ext}]/bestaudio/best"
                # Extract audio and write metadata. We avoid yt-dlp's EmbedThumbnail
                # so we can control which thumbnail is embedded (preferred highest-res).
                ydl_opts["postprocessors"] = [
                    {"key": "FFmpegExtractAudio", "preferredcodec": (self.audio_ext or "mp3"), "preferredquality": str(self.audio_quality).replace("k","")},
                    {"key": "FFmpegMetadata"},
                ]
                logger.debug("Configured audio-only format: %s", ydl_opts["format"])
            else:
                quality = self.video_quality if self.video_quality != "best" else "bestvideo"
                vext_clause = f"[ext={self.video_ext}]" if self.video_ext else ""
                vcodec_clause = f"[vcodec~={self.video_codec}]" if self.video_codec else ""
                acodec_clause = f"[acodec~={self.audio_codec}]" if self.audio_codec else ""
                ydl_opts["format"] = f"{quality}{vext_clause}{vcodec_clause}+bestaudio{acodec_clause}/best"
                logger.debug("Configured video format: %s", ydl_opts["format"])
                # For video downloads, do not use yt-dlp's EmbedThumbnail; we'll embed
                # our preferred thumbnail after download instead.
                ydl_opts.setdefault("postprocessors", [])
                ydl_opts["postprocessors"].append({"key": "FFmpegMetadata"})

            # Extract metadata first
            try:
                self.status_updated.emit("Extracting metadata...")
                logger.debug("Extracting metadata for %s", self.raw_url)
                with yt_dlp.YoutubeDL({**ydl_opts, "skip_download": True}) as ydl:
                    info = ydl.extract_info(self.raw_url, download=False)
                    logger.debug("Metadata thumbnail field: %s", info.get("thumbnail"))
                    if info.get("thumbnails"):
                        logger.debug("Metadata thumbnails list: %s", info.get("thumbnails"))
                    # Choose the best thumbnail URL from metadata (prefer highest-res / maxres)
                    self._preferred_thumbnail = None
                    try:
                        thumb_url = None
                        thumbs = info.get("thumbnails") or []
                        if thumbs:
                            # prefer entry with largest (width*height) if available
                            best = None
                            best_area = 0
                            for t in thumbs:
                                url = t.get("url") if isinstance(t, dict) else None
                                if not url:
                                    continue
                                w = t.get("width") or 0
                                h = t.get("height") or 0
                                area = (w or 0) * (h or 0)
                                if area > best_area:
                                    best_area = area
                                    best = url
                            # fallback: try to find maxresdefault variant in any url
                            if not best:
                                for t in thumbs:
                                    url = t.get("url") if isinstance(t, dict) else None
                                    if url and "maxresdefault" in url:
                                        best = url
                                        break
                            thumb_url = best
                        if not thumb_url:
                            thumb_url = info.get("thumbnail")

                        if thumb_url:
                            # Try variants (YouTube often has maxresdefault). Build candidate URLs with labels.
                            candidates = [(thumb_url, "original")]
                            try:
                                # replace common youtube suffixes to try for higher res
                                if "hqdefault" in thumb_url:
                                    candidates.append((thumb_url.replace("hqdefault", "maxresdefault"), "maxresdefault"))
                                if "default" in thumb_url:
                                    candidates.append((thumb_url.replace("default", "maxresdefault"), "maxresdefault"))
                                if "sddefault" in thumb_url:
                                    candidates.append((thumb_url.replace("sddefault", "maxresdefault"), "maxresdefault"))
                            except Exception:
                                pass

                            headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
                            logger.debug("Thumbnail candidates to try: %s", [c for c, _ in candidates])
                            self._preferred_thumbnail = None
                            self._preferred_is_temp = False
                            for cand, label in candidates:
                                try:
                                    logger.debug("Attempting to fetch thumbnail candidate (%s): %s", label, cand)
                                    head = requests.head(cand, headers=headers, timeout=8)
                                    ok = head.status_code == 200
                                    if not ok:
                                        r = requests.get(cand, stream=True, headers=headers, timeout=12)
                                        ok = r.status_code == 200
                                    else:
                                        r = requests.get(cand, stream=True, headers=headers, timeout=12)
                                    if ok and r is not None and r.status_code == 200:
                                        # write to a temporary file to avoid leaving debug artifacts
                                        with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg', dir=self.temp_dl_loc) as tf:
                                            for chunk in r.iter_content(1024 * 8):
                                                if chunk:
                                                    tf.write(chunk)
                                            out_path = tf.name
                                        self._preferred_thumbnail = out_path
                                        self._preferred_thumbnail_method = label
                                        self._preferred_is_temp = True
                                        logger.debug("Downloaded preferred thumbnail to %s (method=%s)", out_path, label)
                                        break
                                    else:
                                        logger.debug("Thumbnail candidate failed (%s): %s status=%s", label, cand, getattr(r, 'status_code', getattr(head, 'status_code', None)))
                                except Exception:
                                    logger.exception("Thumbnail candidate request failed: %s (%s)", cand, label)
                            if not getattr(self, '_preferred_thumbnail', None):
                                logger.debug("No thumbnail variant could be downloaded for %s", info.get('id'))
                                logger.debug("Metadata thumbnail URL was: %s", thumb_url)
                    except Exception:
                        logger.exception("Error selecting preferred thumbnail from metadata")
            except Exception as ex:
                logger.exception("Metadata extraction failed: %s", ex)
                self.error_occurred.emit(f"Metadata extraction failed: {ex}")
                self.finished.emit(False)
                return

            # Title
            if info.get("_type") == "playlist":
                title = info.get("title", "Playlist")
            else:
                title = info.get("title", info.get("id", "Item"))
            self.video_title = title
            self.title_updated.emit(title)
            logger.debug("Emitted title: %s", title)

            # Try to predict filename and prompt overwrite if needed
            try:
                with yt_dlp.YoutubeDL({**ydl_opts, "skip_download": True}) as ydl:
                    prepared = ydl.prepare_filename(info)
                expected = os.path.join(self.final_destination_dir, os.path.basename(prepared))
                logger.debug("Expected destination: %s", expected)
                if os.path.exists(expected):
                    logger.debug("File exists at destination: %s", expected)
                    allow = self._ask_overwrite_blocking(expected)
                    if not allow:
                        self.status_updated.emit("Skipped (file exists)")
                        self.finished.emit(False)
                        return
                    else:
                        try:
                            os.remove(expected)
                        except Exception:
                            logger.exception("Unable to remove existing file: %s", expected)
                            self.error_occurred.emit(f"Unable to remove existing file: {expected}")
                            self.finished.emit(False)
                            return
            except Exception:
                logger.debug("Unable to compute expected filename - continuing")

            # Start actual download (this will obey noplaylist semantics as set in ydl_opts)
            try:
                self.status_updated.emit("Starting download...")
                logger.debug("Starting yt-dlp download for %s", self.raw_url)
                with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                    info_dict = ydl.extract_info(self.raw_url, download=True)
                logger.debug("yt-dlp completed for %s", self.raw_url)
                logger.debug("Post-download info thumbnail: %s", info_dict.get("thumbnail"))
                if info_dict.get("thumbnails"):
                    logger.debug("Post-download thumbnails list: %s", info_dict.get("thumbnails"))
                # (no debug file backups in final build)
                # Try fallback embed using preferred or local thumbnails
                try:
                    self._embed_best_local_thumbnail(info_dict)
                except Exception:
                    logger.exception("Error running fallback thumbnail embed")
                self._success = True
                self.video_title = info_dict.get("title", self.video_title)
                self.title_updated.emit(self.video_title)
                self.finished.emit(True)
                self.status_updated.emit("Completed")
                return
            except Exception as e:
                logger.exception("Download error: %s", e)
                if isinstance(e, RuntimeError) and "cancel" in str(e).lower():
                    self.status_updated.emit("Cancelled")
                    self.finished.emit(False)
                    return
                tb = traceback.format_exc()
                self.error_occurred.emit(f"{e}\n{tb}")
                self.finished.emit(False)
                return

        except Exception as e:
            logger.exception("Unexpected error in worker: %s", e)
            self.error_occurred.emit(str(e))
            self.finished.emit(False)

    def cancel(self):
        logger.debug("Cancel requested for %s", self.raw_url)
        self._cancelled = True
        self.status_updated.emit("Cancel requested")

    def _embed_best_local_thumbnail(self, info_dict: dict):
        """Find the largest local thumbnail file written by yt-dlp and embed it into
        the downloaded media file using ffmpeg as a fallback if EmbedThumbnail failed.
        """
        vid_id = info_dict.get("id")
        # collect candidate image files from temp and final dirs
        candidates = []
        for d in (self.temp_dl_loc, self.final_destination_dir):
            try:
                for fn in os.listdir(d):
                    if fn.lower().endswith((".jpg", ".jpeg", ".png", ".webp")):
                        # prefer files with the id in the name, but collect all
                        path = os.path.join(d, fn)
                        candidates.append(path)
            except Exception:
                continue

        if not candidates:
            logger.debug("No local thumbnail files found for embedding")
            return

        # If a preferred thumbnail was downloaded from metadata, use it first
        temp_files_to_cleanup = []
        preferred = getattr(self, "_preferred_thumbnail", None)
        preferred_is_temp = getattr(self, "_preferred_is_temp", False)
        if preferred and os.path.exists(preferred):
            thumb = preferred
            logger.debug("Using preferred thumbnail for embedding: %s", thumb)
            if preferred_is_temp and preferred.startswith(os.path.normpath(self.temp_dl_loc)):
                temp_files_to_cleanup.append(preferred)
        else:
            # choose largest file
            candidates = sorted(candidates, key=lambda p: os.path.getsize(p), reverse=True)
            thumb = candidates[0]
            logger.debug("Selected thumbnail for embedding: %s", thumb)

        # ensure thumbnail is a jpg (convert with ffmpeg if needed)
        thumb_jpg = thumb
        conv = None
        if not thumb.lower().endswith((".jpg", ".jpeg")):
            conv = os.path.join(self.temp_dl_loc, f"embed_thumb_{vid_id}.jpg")
            try:
                cmd = [
                    "ffmpeg",
                    "-y",
                    "-i",
                    thumb,
                    conv,
                ]
                logger.debug("Converting thumbnail to jpg: %s", cmd)
                subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                thumb_jpg = conv
                temp_files_to_cleanup.append(conv)
            except Exception:
                logger.exception("Thumbnail conversion failed; proceeding with original file")
                thumb_jpg = thumb

        # find downloaded media file(s) that match the id
        media_candidates = []
        try:
            for fn in os.listdir(self.final_destination_dir):
                if vid_id and vid_id in fn:
                    media_candidates.append(os.path.join(self.final_destination_dir, fn))
        except Exception:
            logger.exception("Unable to list final destination directory")

        if not media_candidates:
            logger.debug("No media files found to embed thumbnail into")
            return

        # pick most recent candidate
        media_candidates = sorted(media_candidates, key=lambda p: os.path.getmtime(p), reverse=True)
        target = media_candidates[0]
        logger.debug("Embedding thumbnail into target file: %s", target)

        # build ffmpeg command to attach the image as cover art
        # create an output temp filename that preserves the original extension
        root, ext = os.path.splitext(target)
        if not ext:
            ext = ""
        out_tmp = f"{root}.embedtmp{ext}"
        cmd = [
            "ffmpeg",
            "-y",
            "-i",
            target,
            "-i",
            thumb_jpg,
            "-map",
            "0",
            "-map",
            "1",
            "-c",
            "copy",
            "-metadata:s:v:1",
            "title=Album cover",
            "-metadata:s:v:1",
            "comment=Cover (front)",
            "-disposition:v:1",
            "attached_pic",
            out_tmp,
        ]

        try:
            logger.debug("Running ffmpeg to embed thumbnail: %s", cmd)
            subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            # replace original
            try:
                os.replace(out_tmp, target)
                logger.info("Embedded thumbnail into %s", target)
            except Exception:
                logger.exception("Failed to replace original file with embedded output")
        except Exception:
            logger.exception("ffmpeg embedding failed for %s", target)
        finally:
            # cleanup any temp thumbnail files we created
            for p in temp_files_to_cleanup:
                try:
                    if os.path.exists(p):
                        os.remove(p)
                except Exception:
                    pass
            # also clear stored preferred thumbnail flag
            try:
                if hasattr(self, '_preferred_thumbnail'):
                    delattr(self, '_preferred_thumbnail')
                if hasattr(self, '_preferred_is_temp'):
                    delattr(self, '_preferred_is_temp')
            except Exception:
                pass
