import logging
import os
import subprocess
import tempfile


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
    """Set track metadata for downloaded media files using ffmpeg copy remux."""
    try:
        index = int(str(playlist_index).strip())
    except (TypeError, ValueError):
        return 0

    if index <= 0:
        return 0
    if not ffmpeg_path or not os.path.exists(ffmpeg_path):
        return 0

    tagged_count = 0
    for file_path in created_files or []:
        try:
            src = os.path.normpath(str(file_path).strip().strip('"\''))
            if not os.path.isfile(src):
                continue
            if os.path.splitext(src)[1].lower() in _SKIP_EXTENSIONS:
                continue

            fd, tmp_out = tempfile.mkstemp(
                prefix="md_tracktag_",
                suffix=os.path.splitext(src)[1],
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
                    f"track={index}",
                    "-metadata",
                    f"tracknumber={index}",
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
