import os
import json
import threading
import logging
import time
import re

log = logging.getLogger(__name__)

INDEX_FILENAME = os.path.join(os.path.dirname(__file__), '..', 'build', 'extractor_index.json')


def _safe_makedirs(path):
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
    except Exception:
        pass


def build_index(timeout=30):
    """Attempt to import yt_dlp and enumerate extractor entities, writing a simple
    JSON index with available extractor names, descriptions and any visible URL patterns.

    This is best-effort: if `yt_dlp` isn't importable, the function returns False.
    """
    try:
        import yt_dlp
    except Exception:
        log.info("yt_dlp python module not available; skipping extractor index build")
        return False

    try:
        # Try to obtain the list_entities helper (present in newer yt-dlp)
        extractor_module = getattr(yt_dlp, 'extractor', None)
        list_entities = getattr(extractor_module, 'list_entities', None)
        entities = None
        if callable(list_entities):
            entities = list_entities()
        else:
            # Fall back to scanning extractor classes if available
            entities = []
            for name in dir(extractor_module):
                ent = getattr(extractor_module, name)
                if hasattr(ent, '_extractor'):
                    entities.append(ent)

        out = []
        for e in entities:
            try:
                ent = e
                # Some list_entities return tuples/classes
                if isinstance(e, tuple) and len(e) >= 1:
                    ent = e[0]
                info = {
                    'name': getattr(ent, 'IE_NAME', getattr(ent, '__name__', None)),
                    'description': getattr(ent, 'DESCRIPTION', None) or getattr(ent, '__doc__', None),
                    'valid_url': getattr(ent, '_VALID_URL', None),
                }
                # Attempt to extract obvious hostnames from _VALID_URL via regex
                hosts = set()
                if info['valid_url']:
                    # find domain-like tokens (e.g., youtube\.com or youtube\.be)
                    for m in re.finditer(r"([a-z0-9\-]+\.(?:com|net|org|tv|io|fm|co|uk|me|online|cc))", info['valid_url'], flags=re.I):
                        hosts.add(m.group(1).lower())
                if hosts:
                    info['hosts'] = sorted(list(hosts))
                out.append(info)
            except Exception:
                continue

        _safe_makedirs(INDEX_FILENAME)
        with open(INDEX_FILENAME, 'w', encoding='utf-8') as f:
            json.dump({'generated': time.time(), 'entries': out}, f, indent=2)
        log.info(f"Built extractor index with {len(out)} entries at {INDEX_FILENAME}")
        return True
    except Exception as e:
        log.exception(f"Failed to build extractor index: {e}")
        return False


def build_index_async(timeout=30):
    t = threading.Thread(target=build_index, args=(timeout,), daemon=True)
    t.start()
    return t


def load_index():
    try:
        if not os.path.exists(INDEX_FILENAME):
            return {}
        with open(INDEX_FILENAME, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return {}


def host_supported(hostname):
    """Check whether a hostname roughly matches any extractor hosts from the index."""
    if not hostname:
        return False
    h = hostname.lower()
    data = load_index()
    entries = data.get('entries') or []
    for ent in entries:
        hosts = ent.get('hosts') or []
        for dh in hosts:
            if h == dh or h.endswith('.' + dh):
                return True
        # Also check name/description/valid_url heuristically
        for key in ('name', 'description', 'valid_url'):
            v = ent.get(key) or ''
            if v and dh and dh in h:
                return True
            if v and h in str(v).lower():
                return True
    return False
