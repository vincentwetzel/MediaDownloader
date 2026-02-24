import logging
import os
import subprocess
import tempfile
from mutagen.oggopus import OggOpus


log = logging.getLogger(__name__)


_SKIP_EXTENSIONS = {
    ".json",
    ".jpg",
    ".jpeg",
    ".png",
    ".webp",
    ".srt",
    ".vtt",
    ".ass",
    ".lrc",
    ".txt",
    ".part",
    ".ytdl",
}


def apply_playlist_track_number(created_files, playlist_index, ffmpeg_path, creation_flags=0):
    """Set track metadata for downloaded media files.

    Uses mutagen for .opus to avoid remuxing (which can strip embedded artwork),
    and ffmpeg copy remux for other containers.
    """
    try:
        index = int(str(playlist_index).strip())
    except (TypeError, ValueError):
        return 0

    if index <= 0:
        return 0
    if not ffmpeg_path or not os.path.exists(ffmpeg_path):
        return 0

    padded_index = f"{index:02d}"

    tagged_count = 0
    for file_path in created_files or []:
        try:
            src = os.path.normpath(str(file_path).strip().strip('"\''))
            if not os.path.isfile(src):
                continue
            if os.path.splitext(src)[1].lower() in _SKIP_EXTENSIONS:
                continue

            file_ext = os.path.splitext(src)[1].lower()
            if file_ext == ".opus":
                try:
                    opus_file = OggOpus(src)
                    opus_file["tracknumber"] = [padded_index]
                    opus_file["track"] = [padded_index]
                    opus_file.save()
                    tagged_count += 1
                except Exception:
                    log.debug("Mutagen track tag write failed for %s", src, exc_info=True)
                continue

            fd, tmp_out = tempfile.mkstemp(
                prefix="md_tracktag_",
                suffix=file_ext,
                dir=os.path.dirname(src) or None,
            )
            os.close(fd)
            try:
                cmd = [
                    ffmpeg_path,
                    "-y",
                    "-i",
                    src,
                    "-map",
                    "0",
                    "-c",
                    "copy",
                    "-metadata",
                    f"track={padded_index}",
                    "-metadata",
                    f"tracknumber={padded_index}",
                    tmp_out,
                ]
                proc = subprocess.run(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    shell=False,
                    creationflags=creation_flags,
                )

                if proc.returncode != 0:
                    log.debug("Failed to write playlist track metadata to %s: %s", src, (proc.stderr or "").strip())
                    continue

                os.replace(tmp_out, src)
                tagged_count += 1
            finally:
                if os.path.exists(tmp_out):
                    try:
                        os.remove(tmp_out)
                    except Exception:
                        pass
        except Exception:
            log.debug("Track metadata tagging failed for %s", file_path, exc_info=True)

    return tagged_count
