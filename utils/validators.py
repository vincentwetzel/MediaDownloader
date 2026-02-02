from urllib.parse import urlparse, parse_qs

def is_search_url(url: str) -> bool:
    """
    Checks if a given URL is a search results page from YouTube or YouTube Music.
    """
    try:
        parsed_url = urlparse(url)
        path = parsed_url.path
        query = parse_qs(parsed_url.query)

        # YouTube search URLs
        if "youtube.com" in parsed_url.netloc and path == "/results" and "search_query" in query:
            return True

        # YouTube Music search URLs
        if "music.youtube.com" in parsed_url.netloc and path == "/search" and "q" in query:
            return True

    except Exception:
        # Ignore parsing errors, treat as not a search URL
        return False

    return False
