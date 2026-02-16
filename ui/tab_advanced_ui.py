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
from core.version import __version__ as APP_VERSION

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
    ("Channel %(channel)s", "%(channel)s"),
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

def build_media_group(tab):
    media_group = QGroupBox("Media & Subtitles")
    layout = QVBoxLayout()
    media_group.setLayout(layout)

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

    tab.write_auto_subs_cb = QCheckBox("Write automatic subtitles")
    tab.write_auto_subs_cb.setToolTip(
        "If normal subtitles are not available, attempt to download autogenerated subtitles."
    )
    write_auto_subs_val = tab.config.get("General", "subtitles_write_auto", fallback="False")
    tab.write_auto_subs_cb.setChecked(str(write_auto_subs_val) == "True")
    tab.write_auto_subs_cb.stateChanged.connect(
        lambda s: tab._save_general("subtitles_write_auto", str(bool(s)))
    )
    layout.addWidget(tab.write_auto_subs_cb)

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

    subs_format_row = QHBoxLayout()
    subs_format_lbl = QLabel("Convert subtitle file to format:")
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

    # Chapter embedding
    tab.embed_chapters_cb = QCheckBox("Embed chapters")
    tab.embed_chapters_cb.setToolTip("Embed chapter markers into the media file when available.")
    embed_chapters_val = tab.config.get("General", "embed_chapters", fallback="True")
    tab.embed_chapters_cb.setChecked(str(embed_chapters_val) == "True")
    tab.embed_chapters_cb.stateChanged.connect(
        lambda s: tab._save_general("embed_chapters", str(bool(s)))
    )
    layout.addWidget(tab.embed_chapters_cb)

    return media_group
