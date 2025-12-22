#include "chatclient.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>

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
    QString text = m_messageEdit->text().trimmed();

    if (text.isEmpty()) return;

    // 2. ПРОВЕРКА: Это команда (начинается с /) или обычное сообщение?
    if (text.startsWith("/")) {

        m_socket->write(text.toUtf8());

        m_chatArea->append("<i style='color:gray;'>Отправка команды: " + text + "</i>");
    }
    else {
        QString target = m_targetName->text().trimmed();

        if (target.isEmpty()) {
            m_chatArea->append("<b style='color:red;'>Система: Выберите получателя в списке справа!</b>");
            return;
        }

        QString data = target + ":" + text;
        m_socket->write(data.toUtf8());

        // Добавляем в свое окно чата
        QString time = QDateTime::currentDateTime().toString("hh:mm");
        m_chatArea->append(QString("<span style='color:gray;'>[%1]</span> <b style='color:green;'>Вы:</b> %2")
                               .arg(time, text));
    }

    // 3. Очищаем поле ввода и возвращаем фокус на него
    m_messageEdit->clear();
    m_chatArea->moveCursor(QTextCursor::End);
}

void ChatClient::onReadyRead()
{
    QByteArray allData = m_socket->readAll();//байты которые идут с сервака

    // . Превращаем байты в текст и рубим на строки по символу '\n'
    QStringList lines = QString::fromUtf8(allData).split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines)
    {
        QString message = line.trimmed();
        if (message.isEmpty()) continue;

        // --- ЛОГИКА ОБРАБОТКИ СПИСКА ПОЛЬЗОВАТЕЛЕЙ ---
        if (message.startsWith("USERS_LIST:")) {
            // Отрезаем префикс "USERS_LIST:"
            QString list = message.mid(11);
            // Разбиваем "user1,user2" на список имен
            QStringList users = list.split(',', Qt::SkipEmptyParts);

            // Обновляем виджет списка на панели справа
            m_userListWidget->clear();
            m_userListWidget->addItems(users);
        }
        else
        {
            m_chatArea->append(message);
        }
    }

    m_chatArea->moveCursor(QTextCursor::End);
}
