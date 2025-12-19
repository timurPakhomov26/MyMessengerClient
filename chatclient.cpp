#include "chatclient.h"
#include <QVBoxLayout>
#include <QLabel>

ChatClient::ChatClient(QWidget *parent) : QWidget(parent) {
    m_socket = new QTcpSocket(this);

    // --- СОЗДАНИЕ ВИДЖЕТОВ ---
    m_chatArea = new QTextEdit(this);
    m_chatArea->setReadOnly(true);

    m_userListWidget = new QListWidget(this); // Наш список контактов
    m_userListWidget->setMaximumWidth(150);   // Делаем его не слишком широким

    m_myUserName = new QLineEdit(this);
    m_targetName = new QLineEdit(this);
    m_messageEdit = new QLineEdit(this);

    QPushButton *connBtn = new QPushButton("Войти", this);
    QPushButton *sendBtn = new QPushButton("Отправить", this);

    // --- РАСПОЛОЖЕНИЕ (LAYOUT) ---

    // 1. Левая часть: ник, чат, ввод сообщения
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel("Ваш ник:"));
    leftLayout->addWidget(m_myUserName);
    leftLayout->addWidget(connBtn);
    leftLayout->addWidget(m_chatArea);
    leftLayout->addWidget(new QLabel("Кому (кликните в списке справа):"));
    leftLayout->addWidget(m_targetName);
    leftLayout->addWidget(m_messageEdit);
    leftLayout->addWidget(sendBtn);

    // 2. Основной слой: Горизонтальный (Левая часть + Список контактов)
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(leftLayout);
    mainLayout->addWidget(m_userListWidget);

    setLayout(mainLayout); // Устанавливаем итоговый слой на окно

    // --- СИГНАЛЫ И СЛОТЫ ---
    connect(connBtn, &QPushButton::clicked, this, &ChatClient::connectToServer);
    connect(sendBtn, &QPushButton::clicked, this, &ChatClient::sendMessage);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_messageEdit, &QLineEdit::returnPressed, this, &ChatClient::sendMessage);

    // Магия: кликаем на ник в списке — он летит в поле "Кому"
    connect(m_userListWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item){
        m_targetName->setText(item->text());
    });
}

void ChatClient::connectToServer() {
    m_socket->connectToHost("83.136.235.45", 1234);
}

void ChatClient::onConnected() {
    m_chatArea->append("✅ Подключено!");
    m_socket->write(m_myUserName->text().toUtf8());
}

void ChatClient::sendMessage() {
    QString data = m_targetName->text() + ":" + m_messageEdit->text();
    m_socket->write(data.toUtf8());
    m_chatArea->append("Вы: " + m_messageEdit->text());
    m_messageEdit->clear();
}

void ChatClient::onReadyRead() {
    QByteArray data = m_socket->readAll();
    QString message = QString::fromUtf8(data).trimmed();

    // ПРОВЕРКА: Если сообщение начинается с нашего спец-кода "USERS_LIST:"
    if (message.startsWith("USERS_LIST:")) {
        // Отрезаем приставку "USERS_LIST:" (это 11 символов)
        QString list = message.mid(11);


        QStringList users = list.split(",");

        // Обновляем виджет справа
        m_userListWidget->clear();        // Стираем старое
        m_userListWidget->addItems(users); // Закидываем новые ники

        m_chatArea->moveCursor(QTextCursor::End);

        return; // Выходим из функции, чтобы эта системная строка не попала в чат
    }

    // Если это не список пользователей, значит это просто сообщение
    m_chatArea->append(message);
}
