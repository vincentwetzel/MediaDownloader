#include "YtDlpArgsBuilder.h"
#include <QDir>
#include "core/ProcessUtils.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
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
    bool isLivestream = options.value("is_live", false).toBool() || options.value("wait_for_video", false).toBool();

    if (isLivestream) {
        QString quality = configManager->get("Livestream", "quality", "best").toString();
        
        if (quality.toLower() == "best" || quality.toLower() == "worst") {
            rawArgs << "-f" << quality.toLower();
        } else {
            QString res = quality.split(' ').first().remove('p');
            rawArgs << "-f" << QString("bestvideo[height<=?%1]+bestaudio/best").arg(res);
        }

        QString downloadAs = configManager->get("Livestream", "download_as", "MPEG-TS").toString();
        if (downloadAs == "MPEG-TS") {
            rawArgs << "--hls-use-mpegts";
            finalOutputExtension = "ts"; // With --hls-use-mpegts, the output is .ts for HLS streams.
        } else {
            rawArgs << "--merge-output-format" << "mkv";
            finalOutputExtension = "mkv";
        }

        QString convertTo = configManager->get("Livestream", "convert_to", "None").toString();
        if (convertTo != "None" && !convertTo.isEmpty()) {
            rawArgs << "--remux-video" << convertTo.toLower();
            finalOutputExtension = convertTo.toLower();
        }
        
        if (configManager->get("Livestream", "live_from_start", false).toBool()) rawArgs << "--live-from-start";
        else rawArgs << "--no-live-from-start";

        if (configManager->get("Livestream", "wait_for_video", true).toBool() || options.value("wait_for_video", false).toBool()) {
            int minWait = options.value("livestream_wait_min", configManager->get("Livestream", "wait_for_video_min")).toInt();
            int maxWait = options.value("livestream_wait_max", configManager->get("Livestream", "wait_for_video_max")).toInt();

            rawArgs << QString("--wait-for-video=%1-%2")
                       .arg(minWait)
                       .arg(maxWait);
        } else {
            rawArgs << "--no-wait-for-video";
        }

        if (configManager->get("Livestream", "use_part", true).toBool()) rawArgs << "--part";
        else rawArgs << "--no-part";

    } else if (downloadType == "video") {
        QString videoQuality = options.contains("video_quality") ? options.value("video_quality").toString() : configManager->get("Video", "video_quality", "1080p (HD)").toString();
        QString videoCodecSetting = options.contains("video_codec") ? options.value("video_codec").toString() : configManager->get("Video", "video_codec", "Default").toString();
        QString audioCodecSetting = options.contains("video_audio_codec") ? options.value("video_audio_codec").toString() : configManager->get("Video", "video_audio_codec", "Default").toString();
        QString requestedExtension = configManager->get("Video", "video_extension", "mp4").toString();
        finalOutputExtension = requestedExtension;

        if (videoQuality == "Select at Runtime") videoQuality = "best";
        if (videoCodecSetting == "Select at Runtime") videoCodecSetting = "Default";
        if (audioCodecSetting == "Select at Runtime") audioCodecSetting = "Default";

        if (options.contains("runtime_video_format") && !options.value("runtime_video_format").toString().isEmpty()) {
            rawArgs << "-f" << options.value("runtime_video_format").toString();
            rawArgs << "--merge-output-format" << requestedExtension;
        } else {
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

            rawArgs << "-f" << QString("%1+%2/%1/bestvideo+bestaudio/best").arg(videoFormatSelector, audioFormatSelector);
            rawArgs << "--merge-output-format" << requestedExtension;
        }

    } else if (downloadType == "audio") {
        QString audioQuality = options.contains("audio_quality") ? options.value("audio_quality").toString() : configManager->get("Audio", "audio_quality", "Best").toString();
        QString audioCodecSetting = options.contains("audio_codec") ? options.value("audio_codec").toString() : configManager->get("Audio", "audio_codec", "Default").toString();
        finalOutputExtension = configManager->get("Audio", "audio_extension", "mp3").toString();

        if (audioQuality == "Select at Runtime") audioQuality = "best";
        if (audioCodecSetting == "Select at Runtime") audioCodecSetting = "Default";

        if (options.contains("runtime_audio_format") && !options.value("runtime_audio_format").toString().isEmpty()) {
            rawArgs << "-f" << options.value("runtime_audio_format").toString();
        } else {
            QString acodec = getCodecMapping(audioCodecSetting);
            QString formatSelector = "bestaudio";

            if (audioQuality.toLower() == "best" || audioQuality.toLower() == "worst") {
                formatSelector = audioQuality.toLower() + "audio";
            } else {
                // Strip any non-digit characters so "320K" or "128 kbps" safely becomes "320" / "128"
                formatSelector += QString("[abr<=?%1]").arg(QString(audioQuality).remove(QRegularExpression("[a-zA-Z\\s]")));
            }
            if (audioCodecSetting != "Default") formatSelector += QString("[acodec~='(?i)%1']").arg(acodec);

            rawArgs << "-f" << formatSelector + "/bestaudio/best";
        }
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
    const ProcessUtils::FoundBinary aria2Binary = ProcessUtils::findBinary("aria2c", configManager);
    if (configManager->get("Metadata", "use_aria2c", false).toBool() && aria2Binary.source != "Not Found") {
        QString aria2cPath = aria2Binary.path;
        rawArgs << "--external-downloader" << aria2cPath;
        rawArgs << "--external-downloader-args" << "aria2c:--summary-interval=1";
    }
    
    QString geoProxy = configManager->get("DownloadOptions", "geo_verification_proxy", "").toString();
    if (!geoProxy.isEmpty()) {
        rawArgs << "--geo-verification-proxy" << geoProxy;
    }

    if (configManager->get("Metadata", "embed_chapters", true).toBool()) rawArgs << "--embed-chapters";
    if (configManager->get("DownloadOptions", "split_chapters", false).toBool()) rawArgs << "--split-chapters";
    if (configManager->get("Metadata", "embed_metadata", true).toBool()) rawArgs << "--embed-metadata";

    const QStringList supportedThumbnailExts = {"mp3", "mkv", "mka", "ogg", "opus", "flac", "m4a", "mp4", "m4v", "mov"};
    
    bool embedThumb = configManager->get("Metadata", "embed_thumbnail", true).toBool();
    bool genFolderJpg = (downloadType == "audio" && configManager->get("Metadata", "generate_folder_jpg", false).toBool() && options.value("playlist_index", -1).toInt() > 0);

    bool canEmbed = embedThumb && supportedThumbnailExts.contains(finalOutputExtension, Qt::CaseInsensitive);
    // We want to write a thumbnail for the UI even if we can't embed it.
    bool shouldWrite = (downloadType == "video" || isLivestream || genFolderJpg);

    if (canEmbed) {
        rawArgs << "--embed-thumbnail";
    } else if (shouldWrite) {
        rawArgs << "--write-thumbnail";
    }

    if (canEmbed || shouldWrite) {
        QStringList ppaArgs;
        if (configManager->get("Metadata", "high_quality_thumbnail", false).toBool()) {
            ppaArgs << "-q:v 0";
        }
        
        // Crop to square if downloading audio
        if (downloadType == "audio" && configManager->get("Metadata", "crop_artwork_to_square", true).toBool()) {
            ppaArgs << "-vf crop=ih";
        }

        if (!ppaArgs.isEmpty()) {
            rawArgs << "--ppa" << QString("ThumbnailsConvertor+ffmpeg_o:%1").arg(ppaArgs.join(" "));
        }

        QString convertThumb = configManager->get("Metadata", "convert_thumbnail_to", "jpg").toString();
        if (convertThumb != "None") {
            rawArgs << "--convert-thumbnails" << convertThumb;
        } else if (genFolderJpg && !canEmbed) {
            // If we are only writing for folder.jpg, we must convert to jpg.
            rawArgs << "--convert-thumbnails" << "jpg";
        }
    }

    QString tempPath = configManager->get("Paths", "temporary_downloads_directory").toString();
    if (tempPath.isEmpty()) tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/LzyDownloader";

    if (genFolderJpg) {
        rawArgs << "--write-thumbnail";
        rawArgs << "-o" << QString("thumbnail:%1").arg(QDir(tempPath).filePath(options.value("id").toString() + "_folder.%(ext)s"));
    }

    // --- Subtitles ---
    bool embedSubs = configManager->get("Subtitles", "embed_subtitles", false).toBool();
    bool writeSubs = configManager->get("Subtitles", "write_subtitles", false).toBool();
    if (embedSubs || writeSubs) {
        QString subLangsRaw = configManager->get("Subtitles", "languages", "en").toString();
        QStringList subLangsList = subLangsRaw.split(',', Qt::SkipEmptyParts);
        subLangsList.removeAll("runtime"); // Exclude 'runtime' from being passed to yt-dlp

        if (options.contains("runtime_subtitles")) {
            subLangsList.append(options.value("runtime_subtitles").toString().split(',', Qt::SkipEmptyParts));
            subLangsList.removeDuplicates();
        }

        if (!subLangsList.isEmpty()) {
            if (subLangsList.contains("all")) {
                rawArgs << "--all-subs";
            } else {
                rawArgs << "--sub-langs" << subLangsList.join(',');
            }
            if (configManager->get("Subtitles", "write_auto_subtitles", false).toBool()) rawArgs << "--write-auto-subs";
            if (embedSubs) rawArgs << "--embed-subs";
            if (writeSubs) {
                rawArgs << "--write-subs";
                rawArgs << "--sub-format" << configManager->get("Subtitles", "format", "srt").toString();
            }
        }
    }

    // --- JS Runtime ---
    ProcessUtils::FoundBinary denoBinary = ProcessUtils::findBinary("deno", configManager);
    if (denoBinary.source != "Not Found") {
        rawArgs << "--js-runtimes" << "deno:" + denoBinary.path;
    }

    // --- Filename restrictions ---
    rawArgs << "--windows-filenames";

    // --- Cookies ---
    QString cookiesBrowser = configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") rawArgs << "--cookies-from-browser" << cookiesBrowser.toLower();

    // --- Custom ffmpeg path ---
    // yt-dlp needs the directory containing ffmpeg and ffprobe
    QString ffmpegPath = ProcessUtils::findBinary("ffmpeg", configManager).path;
    if (ffmpegPath != "ffmpeg") { // Only add if we found a specific path
        rawArgs << "--ffmpeg-location" << QFileInfo(ffmpegPath).path();
    }

    // --- Download Sections ---
    QString downloadSections = options.value("download_sections").toString();
    if (!downloadSections.isEmpty()) {
        rawArgs << "--download-sections" << downloadSections;
    }

    // --- Rate Limit ---
    QString rateLimit = options.value("rate_limit", "Unlimited").toString();
    if (rateLimit != "Unlimited") {
        rawArgs << "--limit-rate" << QString(rateLimit).replace(" MB/s", "M").replace(" KB/s", "K").replace(" ", "");
    }

    // --- Output paths ---
    QDir().mkpath(tempPath);
    
    QString outputTemplate;
    if (downloadType == "audio") {
        outputTemplate = configManager->get("General", "output_template_audio").toString();
    } else {
        outputTemplate = configManager->get("General", "output_template_video").toString();
    }
    
    // Fallback to legacy combined setting if the specific ones aren't set yet
    if (outputTemplate.isEmpty()) {
        outputTemplate = configManager->get("General", "output_template").toString();
    }

    if (outputTemplate.isEmpty()) outputTemplate = "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s";
    rawArgs << "-o" << QDir(tempPath).filePath(outputTemplate);

    // --- Print final filepath ---
    rawArgs << "--print" << "after_move:LZY_FINAL_PATH:%(filepath)s";

    return rawArgs;
}
