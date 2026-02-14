import os
import shutil
import logging
import traceback

log = logging.getLogger("core.file_ops_monitor")

# Keep originals
_original_remove = os.remove
_original_unlink = os.unlink
_original_replace = os.replace
_original_rmdir = os.rmdir if hasattr(os, 'rmdir') else None
_original_shutil_move = shutil.move


def _log_and_call(func_name, orig_func, *args, **kwargs):
    try:
        log.debug(f"File op intercepted: {func_name} args={args} kwargs={kwargs}")
        # Stack trace disabled to reduce terminal clutter
        # stack = ''.join(traceback.format_stack(limit=8)[:-1])
        # log.debug(f"Call stack for {func_name}:\n{stack}")
    except Exception:
        pass
    return orig_func(*args, **kwargs)


def remove(path):
    return _log_and_call('os.remove', _original_remove, path)


def unlink(path):
    return _log_and_call('os.unlink', _original_unlink, path)


def replace(src, dst):
    return _log_and_call('os.replace', _original_replace, src, dst)


def rmdir(path):
    if _original_rmdir:
        return _log_and_call('os.rmdir', _original_rmdir, path)
    raise NotImplementedError


def shutil_move(src, dst):
    return _log_and_call('shutil.move', _original_shutil_move, src, dst)


# Monkey-patch common file ops at import time
try:
    os.remove = remove
    os.unlink = unlink
    os.replace = replace
    if _original_rmdir:
        os.rmdir = rmdir
    shutil.move = shutil_move
    log.info("file_ops_monitor active: patched os.remove/os.unlink/os.replace/shutil.move")
except Exception as e:
    log.exception(f"Failed to patch file ops: {e}")
