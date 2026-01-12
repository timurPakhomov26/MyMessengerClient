#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QImage>
#include <QDateTime>
#include <QUrl>
#include <QTextDocument>
#include <QAudioSource>
#include <QAudioFormat>
#include <QFile>
#include <QSoundEffect>
#include <QTextBrowser>
#include <QAudioSink>
#include <QUdpSocket>

class ChatClient : public QWidget {
    Q_OBJECT
public:
    explicit ChatClient(QWidget *parent = nullptr);

private slots:
    void connectToServer();
    void sendMessage();
    void onReadyRead();
    void onConnected();
    void onAttachFile();

private:
    QTcpSocket *m_socket;
    QTextBrowser *m_chatArea;
    QLineEdit *m_myUserName;
    QLineEdit *m_targetName;
    QLineEdit *m_messageEdit;
    QListWidget *m_userListWidget;
    QMap<QString,int> m_unreadCounts;
    QPushButton *m_attachButton;
    void sendFile(const QString &filePath);

    QAudioSource *m_audioSource = nullptr;
    QAudioSink   *m_audioSink = nullptr;
    QFile m_audioFile;

    QSoundEffect *m_currentPlayer = nullptr;
    QString m_lastPlayedFile;
    QPushButton *m_micButton;
    void startRecording();
    void stopRecording();
    QByteArray addWavHeader(QByteArray data);

    QLineEdit *m_passwordEdit;
    QPushButton *connBtn;
    QPushButton *sendBtn;
    void renderTextMessage(const QString &sender, const QString &text, const QString &time);

    QIODevice *m_outputDevice = nullptr;
    QIODevice *m_inputDevice;
    bool m_isMuted = false;
    bool m_isDeaf = false;
    QPushButton *m_voiceButton;
    QPushButton *m_voiceChatButton;
    QPushButton *m_headsetButton;
    int m_voiceThreshold = 1500;

    QUdpSocket *m_udpSocket;
};

#endif
