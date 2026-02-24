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

            fd, tmp_out = tempfile.mkstemp(
                prefix="md_tracktag_",
                suffix=file_ext,
                dir=os.path.dirname(src) or None,
            )
            os.close(fd)
            try:
                output_format_args = []
                # .opus extension makes ffmpeg pick the strict `opus` muxer, which
                # rejects attached cover-art streams. Force Ogg muxing first.
                if file_ext == ".opus":
                    output_format_args = ["-f", "ogg"]

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
                    *output_format_args,
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
                if proc.returncode != 0 and file_ext == ".opus":
                    stderr_text = (proc.stderr or "").lower()
                    # Some .opus files can contain attached cover-art streams that
                    # ffmpeg cannot remux when writing updated metadata. Retry with
                    # audio-only mapping so track tags are still applied.
                    if "unsupported codec id in stream" in stderr_text:
                        retry_cmd = [
                            ffmpeg_path,
                            "-y",
                            "-i",
                            src,
                            "-map",
                            "0:a",
                            "-c",
                            "copy",
                            "-metadata",
                            f"track={padded_index}",
                            "-metadata",
                            f"tracknumber={padded_index}",
                            "-f",
                            "ogg",
                            tmp_out,
                        ]
                        proc = subprocess.run(
                            retry_cmd,
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
