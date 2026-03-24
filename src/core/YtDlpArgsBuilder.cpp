#include "YtDlpArgsBuilder.h"
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>

YtDlpArgsBuilder::YtDlpArgsBuilder() {
}

QString YtDlpArgsBuilder::getCodecMapping(const QString& codecName) {
    if (codecName == "H.264 (AVC)") return "h264";
    if (codecName == "H.265 (HEVC)") return "h265";
    if (codecName == "avc1 (h264)") return "avc1";
    if (codecName == "VP9") return "vp9";
    if (codecName == "AV1") return "av1";
    if (codecName == "ProRes (Archive)") return "prores";
    if (codecName == "Theora") return "theora";

    // Audio codecs
    if (codecName == "AAC") return "aac";
    if (codecName == "Opus") return "opus";
    if (codecName == "Vorbis") return "vorbis";
    if (codecName == "MP3") return "mp3";
    if (codecName == "FLAC") return "flac";
    if (codecName == "PCM") return "pcm";
    if (codecName == "WAV") return "wav";
    if (codecName == "ALAC") return "alac";
    if (codecName == "AC3") return "ac3";
    if (codecName == "EAC3") return "eac3";
    if (codecName == "DTS") return "dts";

    // Fallback for any unmapped or simple names
    return codecName.toLower();
}

QStringList YtDlpArgsBuilder::build(ConfigManager *configManager, const QString &url, const QVariantMap &options) {
    if (!configManager) {
        qCritical() << "YtDlpArgsBuilder::build called without a ConfigManager";
        return {};
    }
    QStringList rawArgs;

    // --- Basic arguments ---
    rawArgs << url;
    rawArgs << "--verbose";
    rawArgs << "--write-info-json";
    rawArgs << "--encoding" << "utf-8";
    rawArgs << "--no-restrict-filenames";
    rawArgs << "--newline";
    rawArgs << "--ignore-errors"; // Continue on non-fatal errors (like subtitle failures)
    rawArgs << "--progress-template" << "download:[download] %(progress.percent_str)s of %(progress.total_bytes_str)s at %(progress.speed_str)s ETA %(progress.eta_str)s";

    QString downloadType = options.value("type").toString();
    QString finalOutputExtension;

    // --- Format Selection ---
    if (downloadType == "video") {
        QString videoQuality = configManager->get("Video", "video_quality", "1080p (HD)").toString();
        QString videoCodecSetting = configManager->get("Video", "video_codec", "Default").toString();
        QString audioCodecSetting = configManager->get("Video", "video_audio_codec", "Default").toString();
        QString requestedExtension = configManager->get("Video", "video_extension", "mp4").toString();
        finalOutputExtension = requestedExtension;

        QString vcodec = getCodecMapping(videoCodecSetting);
        QString acodec = getCodecMapping(audioCodecSetting);

        QString videoFormatSelector = "bestvideo";
        if (videoQuality.toLower() == "best" || videoQuality.toLower() == "worst") {
            videoFormatSelector = videoQuality.toLower() + "video";
        } else {
            videoFormatSelector += QString("[height<=?%1]").arg(videoQuality.split(' ').first().remove('p'));
        }
        if (videoCodecSetting != "Default") videoFormatSelector += QString("[vcodec~='(?i)%1']").arg(vcodec);

        QString audioFormatSelector = "bestaudio";
        if (audioCodecSetting != "Default") audioFormatSelector += QString("[acodec~='(?i)%1']").arg(acodec);

        rawArgs << "-f" << QString("%1+%2/bestvideo+bestaudio/best").arg(videoFormatSelector, audioFormatSelector);
        if (videoCodecSetting != "Default") rawArgs << "--merge-output-format" << requestedExtension;

    } else if (downloadType == "audio") {
        QString audioQuality = configManager->get("Audio", "audio_quality", "Best").toString();
        QString audioCodecSetting = configManager->get("Audio", "audio_codec", "Default").toString();
        finalOutputExtension = configManager->get("Audio", "audio_extension", "mp3").toString();
        QString acodec = getCodecMapping(audioCodecSetting);

        QString formatSelector = "bestaudio";
        if (audioQuality.toLower() == "best" || audioQuality.toLower() == "worst") {
            formatSelector = audioQuality.toLower() + "audio";
        } else {
            // Strip any non-digit characters so "320K" or "128 kbps" safely becomes "320" / "128"
            formatSelector += QString("[abr<=?%1]").arg(QString(audioQuality).remove(QRegularExpression("[a-zA-Z\\s]")));
        }
        if (audioCodecSetting != "Default") formatSelector += QString("[acodec~='(?i)%1']").arg(acodec);

        rawArgs << "-f" << formatSelector + "/bestaudio";
        rawArgs << "-x";
        if (audioCodecSetting != "Default") rawArgs << "--audio-format" << finalOutputExtension;
        rawArgs << "--audio-quality" << "0";
    }

    // --- Playlist Logic ---
    QString playlistLogic = options.value("playlist_logic", "Ask").toString();
    if (playlistLogic == "Download All (no prompt)") rawArgs << "--yes-playlist";
    else if (playlistLogic == "Download Single (ignore playlist)") rawArgs << "--no-playlist";

    // --- Duplicate Check Override ---
    if (options.value("override_archive", false).toBool()) rawArgs << "--force-download";

    // --- General Options ---
    if (configManager->get("General", "sponsorblock", false).toBool()) rawArgs << "--sponsorblock-remove" << "all";
    if (configManager->get("Metadata", "use_aria2c", false).toBool()) {
        rawArgs << "--external-downloader" << "aria2c";
        rawArgs << "--external-downloader-args" << "aria2c:--summary-interval=1";
    }
    if (configManager->get("Metadata", "embed_chapters", true).toBool()) rawArgs << "--embed-chapters";
    if (configManager->get("Metadata", "embed_metadata", true).toBool()) rawArgs << "--embed-metadata";

    const QStringList supportedThumbnailExts = {"mp3", "mkv", "mka", "ogg", "opus", "flac", "m4a", "mp4", "m4v", "mov"};
    if (configManager->get("Metadata", "embed_thumbnail", true).toBool() && supportedThumbnailExts.contains(finalOutputExtension, Qt::CaseInsensitive)) {
        rawArgs << "--embed-thumbnail";
        if (configManager->get("Metadata", "high_quality_thumbnail", false).toBool()) {
            rawArgs << "--ppa" << "ThumbnailsConvertor+ffmpeg_o:-q:v 0";
        }
        QString convertThumb = configManager->get("Metadata", "convert_thumbnail_to", "jpg").toString();
        if (convertThumb != "None") rawArgs << "--convert-thumbnails" << convertThumb;
    }

    // --- Subtitles ---
    bool embedSubs = configManager->get("Subtitles", "embed_subtitles", false).toBool();
    bool writeSubs = configManager->get("Subtitles", "write_subtitles", false).toBool();
    if (embedSubs || writeSubs) {
        QString subLangs = configManager->get("Subtitles", "languages", "en").toString();
        rawArgs << (subLangs == "all" ? "--all-subs" : "--sub-langs") << subLangs;
        if (configManager->get("Subtitles", "write_auto_subtitles", false).toBool()) rawArgs << "--write-auto-subs";
        if (embedSubs) rawArgs << "--embed-subs";
        if (writeSubs) {
            rawArgs << "--write-subs";
            rawArgs << "--sub-format" << configManager->get("Subtitles", "format", "srt").toString().remove('*');
        }
    }

    // --- JS Runtime ---
    const QString denoPath = QDir(QCoreApplication::applicationDirPath()).filePath("deno.exe");
    if (QFile::exists(denoPath)) {
        rawArgs << "--js-runtimes" << "deno:" + denoPath;
    }

    // --- Filename restrictions ---
    rawArgs << "--windows-filenames";

    // --- Cookies ---
    QString cookiesBrowser = configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") rawArgs << "--cookies-from-browser" << cookiesBrowser.toLower();

    // --- Rate Limit ---
    QString rateLimit = options.value("rate_limit", "Unlimited").toString();
    if (rateLimit != "Unlimited") {
        rawArgs << "--limit-rate" << QString(rateLimit).replace(" MB/s", "M").replace(" KB/s", "K").replace(" ", "");
    }

    // --- Output paths ---
    QString tempPath = configManager->get("Paths", "temporary_downloads_directory").toString();
    if (tempPath.isEmpty()) tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/MediaDownloader";
    QDir().mkpath(tempPath);
    QString outputTemplate = configManager->get("General", "output_template").toString();
    if (outputTemplate.isEmpty()) outputTemplate = "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s";
    rawArgs << "-o" << QDir(tempPath).filePath(outputTemplate);

    // --- Print final filepath ---
    rawArgs << "--print" << "after_move:filepath";

    return rawArgs;
}
