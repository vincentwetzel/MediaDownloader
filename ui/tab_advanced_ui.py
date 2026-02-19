from PyQt6.QtWidgets import (
    QGroupBox,
    QVBoxLayout,
    QCheckBox,
    QHBoxLayout,
    QLabel,
    QComboBox,
    QPushButton,
)
from PyQt6.QtCore import Qt

# Subtitle language: backend code -> display name (GUI shows display name, config stores code)
SUBTITLE_LANGUAGES = {
    "af": "Afrikaans",
    "am": "Amharic",
    "ar": "Arabic",
    "as": "Assamese",
    "az": "Azerbaijani",
    "be": "Belarusian",
    "bg": "Bulgarian",
    "bn": "Bengali",
    "bs": "Bosnian",
    "ca": "Catalan",
    "cs": "Czech",
    "cy": "Welsh",
    "da": "Danish",
    "de": "German",
    "el": "Greek",
    "en": "English",
    "en-GB": "English (United Kingdom)",
    "en-US": "English (United States)",
    "es": "Spanish",
    "es-419": "Spanish (Latin America)",
    "es-ES": "Spanish (Spain)",
    "et": "Estonian",
    "eu": "Basque",
    "fa": "Persian",
    "fi": "Finnish",
    "fil": "Filipino",
    "fr": "French",
    "fr-CA": "French (Canada)",
    "gl": "Galician",
    "gu": "Gujarati",
    "he": "Hebrew",
    "hi": "Hindi",
    "hr": "Croatian",
    "hu": "Hungarian",
    "hy": "Armenian",
    "id": "Indonesian",
    "is": "Icelandic",
    "it": "Italian",
    "ja": "Japanese",
    "ka": "Georgian",
    "kk": "Kazakh",
    "km": "Khmer",
    "kn": "Kannada",
    "ko": "Korean",
    "ky": "Kyrgyz",
    "lo": "Lao",
    "lt": "Lithuanian",
    "lv": "Latvian",
    "mk": "Macedonian",
    "ml": "Malayalam",
    "mn": "Mongolian",
    "mr": "Marathi",
    "ms": "Malay",
    "my": "Burmese",
    "ne": "Nepali",
    "nl": "Dutch",
    "no": "Norwegian",
    "or": "Odia",
    "pa": "Punjabi",
    "pl": "Polish",
    "pt": "Portuguese",
    "pt-BR": "Portuguese (Brazil)",
    "ro": "Romanian",
    "ru": "Russian",
    "si": "Sinhala",
    "sk": "Slovak",
    "sl": "Slovenian",
    "sq": "Albanian",
    "sr": "Serbian",
    "sv": "Swedish",
    "sw": "Swahili",
    "ta": "Tamil",
    "te": "Telugu",
    "th": "Thai",
    "tr": "Turkish",
    "uk": "Ukrainian",
    "ur": "Urdu",
    "uz": "Uzbek",
    "vi": "Vietnamese",
    "zh-Hans": "Chinese (Simplified)",
    "zh-Hant": "Chinese (Traditional)",
    "zu": "Zulu",
}

# Output template insertables shown in Advanced Settings.
# Format: (visible label, inserted template token)
OUTPUT_TEMPLATE_TOKENS = [
    ("Insert...", None),
    ("Title %(title)s", "%(title)s"),
    ("Uploader %(uploader)s", "%(uploader)s"),
    ("Release Date %(release_date)s", "%(release_date)s"),
    ("Release Year %(release_date>%Y)s", "%(release_date>%Y)s"),
    ("Release Month %(release_date>%m)s", "%(release_date>%m)s"),
    ("Release Day %(release_date>%d)s", "%(release_date>%d)s"),
    ("Video ID %(id)s", "%(id)s"),
    ("Extension %(ext)s", "%(ext)s"),
    ("Playlist %(playlist_title)s", "%(playlist_title)s"),
    ("Playlist Index %(playlist_index)s", "%(playlist_index)s"),
    ("Uploader ID %(uploader_id)s", "%(uploader_id)s"),
    ("Duration %(duration)s", "%(duration)s"),
]

def build_config_group(tab):
    config_group = QGroupBox("Configuration")
    layout = QVBoxLayout()
    config_group.setLayout(layout)

    # Output folder row
    out_row = QHBoxLayout()
    out_lbl = QLabel("Output folder:")
    out_lbl.setToolTip("Where completed downloads will be saved.")
    out_path = tab.config.get("Paths", "completed_downloads_directory", fallback="")
    tab.out_display = QLabel(out_path)
    tab.out_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
    btn_out = QPushButton("\U0001F4C1")
    btn_out.setFixedWidth(40)
    btn_out.clicked.connect(tab.browse_out)
    btn_out.setToolTip("Browse and select the output folder for completed downloads.")
    out_row.addWidget(out_lbl)
    out_row.addWidget(tab.out_display, stretch=1)
    out_row.addWidget(btn_out)
    layout.addLayout(out_row)

    # Temporary folder row
    temp_row = QHBoxLayout()
    temp_lbl = QLabel("Temporary folder:")
    temp_lbl.setToolTip("Where downloads are stored while in progress before moving to output folder.")
    temp_path = tab.config.get("Paths", "temporary_downloads_directory", fallback="")
    tab.temp_display = QLabel(temp_path)
    tab.temp_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
    btn_temp = QPushButton("\U0001F4C1")
    btn_temp.setFixedWidth(40)
    btn_temp.clicked.connect(tab.browse_temp)
    btn_temp.setToolTip("Browse and select the temporary folder for in-progress downloads.")
    temp_row.addWidget(temp_lbl)
    temp_row.addWidget(tab.temp_display, stretch=1)
    temp_row.addWidget(btn_temp)
    layout.addLayout(temp_row)

    # Theme dropdown
    theme_row = QHBoxLayout()
    theme_lbl = QLabel("Theme:")
    theme_lbl.setToolTip("Choose the application theme.")
    tab.theme_combo = QComboBox()
    tab.theme_combo.setToolTip("Select the application theme.")
    tab.theme_combo.addItem("System", "auto")
    tab.theme_combo.addItem("Light", "light")
    tab.theme_combo.addItem("Dark", "dark")

    saved_theme = tab.config.get("General", "theme", fallback="auto")
    idx = tab.theme_combo.findData(saved_theme)
    if idx >= 0:
        tab.theme_combo.setCurrentIndex(idx)
    else:
        if saved_theme == "system":
            idx = tab.theme_combo.findData("auto")
            if idx >= 0:
                tab.theme_combo.setCurrentIndex(idx)

    tab.theme_combo.currentIndexChanged.connect(tab._on_theme_changed)

    theme_row.addWidget(theme_lbl)
    theme_row.addWidget(tab.theme_combo, stretch=1)
    layout.addLayout(theme_row)

    return config_group

def build_metadata_group(tab):
    """Builds the Metadata & Thumbnails settings group."""
    meta_group = QGroupBox("Metadata & Thumbnails")
    layout = QVBoxLayout()
    meta_group.setLayout(layout)

    # --- Metadata & Thumbnails ---
    tab.embed_metadata_cb = QCheckBox("Embed metadata")
    tab.embed_metadata_cb.setToolTip("Embed metadata (title, artist, etc.) into the media file.")
    embed_metadata_val = tab.config.get("General", "embed_metadata", fallback="True")
    tab.embed_metadata_cb.setChecked(str(embed_metadata_val) == "True")
    tab.embed_metadata_cb.stateChanged.connect(
        lambda s: tab._save_general("embed_metadata", str(bool(s)))
    )
    layout.addWidget(tab.embed_metadata_cb)

    tab.embed_thumbnail_cb = QCheckBox("Embed thumbnail")
    tab.embed_thumbnail_cb.setToolTip("Embed the thumbnail into the media file as album art.")
    embed_thumbnail_val = tab.config.get("General", "embed_thumbnail", fallback="True")
    tab.embed_thumbnail_cb.setChecked(str(embed_thumbnail_val) == "True")
    tab.embed_thumbnail_cb.stateChanged.connect(
        lambda s: tab._save_general("embed_thumbnail", str(bool(s)))
    )
    layout.addWidget(tab.embed_thumbnail_cb)

    tab.high_quality_thumbnail_cb = QCheckBox("Use high-quality thumbnail converter")
    tab.high_quality_thumbnail_cb.setToolTip(
        "Use a higher quality converter for thumbnails (MJPEG format).\n"
        "This can prevent black bars (letterboxing) on non-square thumbnails\n"
        "and can result in better quality album art for audio files."
    )
    high_quality_thumbnail_val = tab.config.get("General", "high_quality_thumbnail", fallback="True")
    tab.high_quality_thumbnail_cb.setChecked(str(high_quality_thumbnail_val) == "True")
    tab.high_quality_thumbnail_cb.stateChanged.connect(
        lambda s: tab._save_general("high_quality_thumbnail", str(bool(s)))
    )
    layout.addWidget(tab.high_quality_thumbnail_cb)

    # Convert Thumbnails Dropdown
    thumb_conv_row = QHBoxLayout()
    thumb_conv_lbl = QLabel("Convert thumbnails to:")
    thumb_conv_lbl.setToolTip("Convert downloaded thumbnails to a specific format.")
    tab.thumb_conv_combo = QComboBox()
    tab.thumb_conv_combo.setToolTip("Select format to convert thumbnails to.")
    
    # Add options: jpg (default), png, webp, None (keep original)
    tab.thumb_conv_combo.addItem("jpg (Default)", "jpg")
    tab.thumb_conv_combo.addItem("png", "png")
    tab.thumb_conv_combo.addItem("webp", "webp")
    tab.thumb_conv_combo.addItem("None (Keep Original)", "None")

    saved_thumb_conv = tab.config.get("General", "convert_thumbnails", fallback="jpg")
    idx = tab.thumb_conv_combo.findData(saved_thumb_conv)
    if idx >= 0:
        tab.thumb_conv_combo.setCurrentIndex(idx)
    else:
        # Fallback to jpg if invalid
        idx_jpg = tab.thumb_conv_combo.findData("jpg")
        if idx_jpg >= 0:
            tab.thumb_conv_combo.setCurrentIndex(idx_jpg)
            tab._save_general("convert_thumbnails", "jpg")

    tab.thumb_conv_combo.currentIndexChanged.connect(tab._on_thumb_conv_changed)

    thumb_conv_row.addWidget(thumb_conv_lbl)
    thumb_conv_row.addWidget(tab.thumb_conv_combo, stretch=1)
    layout.addLayout(thumb_conv_row)
    
    return meta_group

def build_subtitles_group(tab):
    """Builds the Subtitles settings group."""
    subs_group = QGroupBox("Subtitles")
    layout = QVBoxLayout()
    subs_group.setLayout(layout)

    # --- Subtitles ---
    subs_lang_row = QHBoxLayout()
    subs_lang_lbl = QLabel("Subtitle language:")
    subs_lang_lbl.setToolTip("Language for subtitles.")
    tab.subs_lang_combo = QComboBox()
    tab.subs_lang_combo.setToolTip("Select a language for subtitles.")

    for code, name in SUBTITLE_LANGUAGES.items():
        tab.subs_lang_combo.addItem(name, code)

    saved_lang = tab.config.get("General", "subtitles_langs", fallback="en")
    idx = tab.subs_lang_combo.findData(saved_lang)
    if idx >= 0:
        tab.subs_lang_combo.setCurrentIndex(idx)
    else:
        # Saved value not in list (e.g. from old text entry); fall back to English
        idx_en = tab.subs_lang_combo.findData("en")
        if idx_en >= 0:
            tab.subs_lang_combo.setCurrentIndex(idx_en)
            tab._save_general("subtitles_langs", "en")

    tab.subs_lang_combo.currentIndexChanged.connect(tab._on_subs_lang_changed)

    subs_lang_row.addWidget(subs_lang_lbl)
    subs_lang_row.addWidget(tab.subs_lang_combo, stretch=1)
    layout.addLayout(subs_lang_row)

    tab.embed_subs_cb = QCheckBox("Embed subtitles into video")
    tab.embed_subs_cb.setToolTip("Embed subtitles in the video file.")
    embed_subs_val = tab.config.get("General", "subtitles_embed", fallback="False")
    tab.embed_subs_cb.setChecked(str(embed_subs_val) == "True")
    tab.embed_subs_cb.stateChanged.connect(
        lambda s: tab._save_general("subtitles_embed", str(bool(s)))
    )
    layout.addWidget(tab.embed_subs_cb)

    tab.write_subs_cb = QCheckBox("Write subtitles (separate file)")
    tab.write_subs_cb.setToolTip("Download subtitles to a separate file.")
    write_subs_val = tab.config.get("General", "subtitles_write", fallback="False")
    tab.write_subs_cb.setChecked(str(write_subs_val) == "True")
    tab.write_subs_cb.stateChanged.connect(
        lambda s: tab._save_general("subtitles_write", str(bool(s)))
    )
    layout.addWidget(tab.write_subs_cb)

    tab.write_auto_subs_cb = QCheckBox("Include automatically-generated subtitles")
    tab.write_auto_subs_cb.setToolTip(
        "If normal subtitles are not available, attempt to download autogenerated subtitles."
    )
    write_auto_subs_val = tab.config.get("General", "subtitles_write_auto", fallback="False")
    tab.write_auto_subs_cb.setChecked(str(write_auto_subs_val) == "True")
    tab.write_auto_subs_cb.stateChanged.connect(
        lambda s: tab._save_general("subtitles_write_auto", str(bool(s)))
    )
    layout.addWidget(tab.write_auto_subs_cb)

    subs_format_row = QHBoxLayout()
    subs_format_lbl = QLabel("Subtitle file format:")
    subs_format_lbl.setToolTip("File format to convert subtitles to. Select 'None' to keep the original format.")
    tab.subs_format_combo = QComboBox()
    tab.subs_format_combo.setToolTip("Select a subtitle format. SRT is recommended.")
    
    tab.subs_format_combo.addItem("srt *", "srt")
    tab.subs_format_combo.addItem("vtt", "vtt")
    tab.subs_format_combo.addItem("ass", "ass")
    tab.subs_format_combo.addItem("lrc", "lrc")
    tab.subs_format_combo.addItem("best", "best")
    tab.subs_format_combo.addItem("None", "None")

    saved_subs_format = tab.config.get("General", "subtitles_format", fallback="srt")
    idx = tab.subs_format_combo.findData(saved_subs_format)
    if idx >= 0:
        tab.subs_format_combo.setCurrentIndex(idx)
    else:
        tab.subs_format_combo.setCurrentIndex(0)
        tab._save_general("subtitles_format", "srt")

    tab.subs_format_combo.currentIndexChanged.connect(tab._on_subs_format_changed)
    
    subs_format_row.addWidget(subs_format_lbl)
    subs_format_row.addWidget(tab.subs_format_combo, stretch=1)
    layout.addLayout(subs_format_row)

    return subs_group
