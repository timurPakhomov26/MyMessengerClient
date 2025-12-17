#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>

class ChatClient : public QWidget {
    Q_OBJECT
public:
    explicit ChatClient(QWidget *parent = nullptr);

private slots:
    void connectToServer();
    void sendMessage();
    void onReadyRead();
    void onConnected();

private:
    QTcpSocket *m_socket;
    QTextEdit *m_chatArea;
    QLineEdit *m_myUserName;
    QLineEdit *m_targetName;
    QLineEdit *m_messageEdit;
    QListWidget *m_userListWidget;
};

#endif
