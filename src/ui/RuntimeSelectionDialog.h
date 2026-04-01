#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QStringList>

class QListWidget;
class QCheckBox;

class RuntimeSelectionDialog : public QDialog {
    Q_OBJECT
public:
    RuntimeSelectionDialog(const QVariantMap &info, bool selectVideo, bool selectAudio, bool selectSubs, QWidget *parent = nullptr);

    QString getSelectedVideoFormat() const;
    QString getSelectedAudioFormat() const;
    QStringList getSelectedSubtitles() const;

private:
    void setupUi();
    void populateData();

    QVariantMap m_info;
    bool m_selectVideo;
    bool m_selectAudio;
    bool m_selectSubs;

    QListWidget *m_videoList;
    QListWidget *m_audioList;
    QListWidget *m_subsList;
};