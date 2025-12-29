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
    QFile m_audioFile;
    QPushButton *m_micButton;
    void startRecording();
    void stopRecording();
    QByteArray addWavHeader(QByteArray data);
};

#endif
