import os
import json
import threading
import logging
import time
import re
import subprocess
import sys

log = logging.getLogger(__name__)

# Use a location inside the core directory or a dedicated data directory
INDEX_FILENAME = os.path.join(os.path.dirname(__file__), '..', 'build', 'extractor_index.json')


def _safe_makedirs(path):
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
    except Exception:
        pass


def build_index(timeout=30):
    """Attempt to build an index of supported extractors.
    
    Since we are using a bundled yt-dlp binary and might not have the python module installed,
    we can try to parse `yt-dlp --list-extractors`.
    """
    from core.binary_manager import get_binary_path
    
    yt_dlp_path = get_binary_path("yt-dlp")
    if not yt_dlp_path:
        log.warning("yt-dlp binary not found; skipping extractor index build")
        return False

    creation_flags = 0
    if sys.platform == "win32" and getattr(sys, "frozen", False):
        creation_flags = subprocess.CREATE_NO_WINDOW

    try:
        # Run yt-dlp --list-extractors
        cmd = [yt_dlp_path, "--list-extractors"]
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            creationflags=creation_flags,
            encoding='utf-8',
            errors='replace'
        )
        
        if result.returncode != 0:
            log.warning(f"Failed to list extractors: {result.stderr}")
            return False
            
        extractors = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        
        # Build a simple index mapping extractor names to themselves (as we don't get domains easily from CLI)
        # We can try to infer domains from extractor names, but it's heuristic.
        # Many extractors are named like "Youtube", "Vimeo", "Instagram", etc.
        
        entries = []
        for ext in extractors:
            entries.append({
                'name': ext,
                'description': f"Extractor for {ext}",
                # Heuristic: use the extractor name as a potential host keyword
                'hosts': [ext.lower()] 
            })

        _safe_makedirs(INDEX_FILENAME)
        with open(INDEX_FILENAME, 'w', encoding='utf-8') as f:
            json.dump({'generated': time.time(), 'entries': entries}, f, indent=2)
            
        log.info(f"Built extractor index with {len(entries)} entries at {INDEX_FILENAME}")
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
    
    # Remove www. prefix for matching
    if h.startswith("www."):
        h = h[4:]
        
    data = load_index()
    entries = data.get('entries', [])
    
    if not entries:
        # If index is empty, we can't say for sure, so maybe return False or True?
        # Returning False forces Tier 2 validation (simulate), which is safer but slower.
        return False

    for ent in entries:
        # Check against 'hosts' (which are just extractor names in our CLI-based index)
        hosts = ent.get('hosts', [])
        for dh in hosts:
            # Simple substring match: if extractor name is in hostname
            # e.g. "instagram" in "instagram.com" -> True
            if dh in h:
                return True

    return False
