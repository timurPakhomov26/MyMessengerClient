#include "chatclient.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFileDialog>
#include <QSoundEffect>
#include <QTextBrowser>
#include <QCoreApplication>
#include <QNetworkDatagram>

ChatClient::ChatClient(QWidget *parent) : QWidget(parent) {
    m_socket = new QTcpSocket(this);

    // --- СОЗДАНИЕ ВИДЖЕТОВ ---
    m_chatArea = new QTextBrowser(this);
    m_chatArea->setOpenExternalLinks(false);
    m_chatArea->setOpenLinks(false);
    m_chatArea->setReadOnly(true);
    // 2. Включаем поддержку ссылок
    m_chatArea->setTextInteractionFlags(Qt::TextBrowserInteraction);

    m_userListWidget = new QListWidget(this);
    m_userListWidget->setMaximumWidth(150);

    m_myUserName = new QLineEdit(this);
    m_myUserName->setPlaceholderText("Login...");

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText("Password...");
    m_passwordEdit->setEchoMode(QLineEdit::Password); // Чтобы вместо букв были точки!

    m_targetName = new QLineEdit(this);
    m_messageEdit = new QLineEdit(this);
    m_attachButton = new QPushButton("📎", this);

    connBtn = new QPushButton("Войти", this);
    sendBtn = new QPushButton("Отправить", this);

    // --- РАСПОЛОЖЕНИЕ (LAYOUT) ---

    // 1. Левая часть: ник, чат, ввод сообщения
    QVBoxLayout *leftLayout = new QVBoxLayout();

    leftLayout->addWidget(new QLabel("Ваш ник:"));
    leftLayout->addWidget(m_myUserName);
    leftLayout->addWidget(m_passwordEdit);
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
    // mainLayout->addWidget(m_attachButton);

    //mainLayout->addWidget(m_myUserName);
    // mainLayout->addWidget(m_passwordEdit);
    setLayout(mainLayout); // Устанавливаем итоговый слой на окно

    // --- СИГНАЛЫ И СЛОТЫ ---
    connect(connBtn, &QPushButton::clicked, this, &ChatClient::connectToServer);
    connect(sendBtn, &QPushButton::clicked, this, &ChatClient::sendMessage);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_messageEdit, &QLineEdit::returnPressed, this, &ChatClient::sendMessage);

    connect(m_attachButton, &QPushButton::clicked, this, &ChatClient::onAttachFile);

    m_micButton = new QPushButton("🎤", this);
    m_micButton->setMinimumSize(20, 20); // Сделаем её квадратной и заметной
    m_micButton->setStyleSheet("QPushButton { background-color: #dfe6e9; border-radius: 20px; font-size: 18px; }"
                               "QPushButton:pressed { background-color: #ff7675; }"); // Краснеет при нажатии


    leftLayout->addWidget(m_micButton);
    leftLayout->addWidget(m_attachButton);

    connect(m_micButton, &QPushButton::pressed, this, &ChatClient::startRecording);
    connect(m_micButton, &QPushButton::released, this, &ChatClient::stopRecording);

    m_chatArea->setOpenExternalLinks(false);
    m_chatArea->setOpenLinks(false);
    m_chatArea->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_chatArea->setReadOnly(true);

    // --- НАСТРОЙКА АУДИО-ДВИЖКА 2026 ---
    QAudioFormat format;
    format.setSampleRate(8000); // Низкая частота для рации, чтобы не лагало
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16); // 16 бит - стандарт

    // Создаем сами объекты (микрофон и динамик)
    m_audioSource = new QAudioSource(format, this);
    m_audioSink = new QAudioSink(format, this);

    // Сразу готовим динамик к приему байтов
    m_outputDevice = m_audioSink->start();

    connect(m_chatArea, &QTextBrowser::anchorClicked, [this](const QUrl &url){
        QString link = url.toString();
        if (link.startsWith("play:")) {
            QString fileName = link.mid(5);
            QString fullPath = QCoreApplication::applicationDirPath() + "/" + fileName;

            if (m_currentPlayer && m_currentPlayer->isPlaying()) {
                m_currentPlayer->stop();

                // Если нажали на ту же самую запись, что играет сейчас — просто стопаем и выходим (как пауза)
                if (m_lastPlayedFile == fileName) {
                    m_lastPlayedFile = "";
                    return;
                }
            }

            // 2. Запускаем новую запись
            if (QFile::exists(fullPath)) {
                if (!m_currentPlayer) m_currentPlayer = new QSoundEffect(this);

                m_currentPlayer->setSource(QUrl::fromLocalFile(fullPath));
                m_currentPlayer->setVolume(1.0f);
                m_currentPlayer->play();

                m_lastPlayedFile = fileName; // Запоминаем, что играет
                qDebug() << "Играю:" << fileName;
            }
        }
    });


    connect(m_userListWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item)
            {
        QString fullText = item->text();
        m_chatArea->clear();

        if (fullText.contains("ОБЩИЙ")) {
            m_targetName->setText("ОБЩИЙ ЧАТ");
            // СБРАСЫВАЕМ СЧЕТЧИК И ВИЗУАЛ СРАЗУ
            m_unreadCounts["GROUP_CHAT"] = 0;
            item->setBackground(Qt::transparent);
            item->setForeground(QColor("#f1c40f"));
            item->setText("📢 [ ОБЩИЙ ЧАТ ]"); // Убираем [1] из текста!

            m_socket->write("/get_history GROUP_CHAT\n");
        } else {
            QString name = fullText.mid(2).section(" [", 0, 0).trimmed();
            m_targetName->setText(name);
            m_unreadCounts[name] = 0;
            item->setBackground(Qt::transparent);
            m_socket->write(QString("/get_history %1\n").arg(name).toUtf8());
        }
            });

    connect(connBtn, &QPushButton::clicked, [this](){
        QString user = m_myUserName->text().trimmed();
        QString pass = m_passwordEdit->text().trimmed();


        if (user.isEmpty() || pass.isEmpty()) {
            m_chatArea->append("<b style='color:red;'>Введите и логин, и пароль!</b>");
            return;
        }

        // Шлем на сервер спец-пакет
        QString authData = QString("AUTH:%1:%2\n").arg(user).arg(pass);
        qDebug() << "SENDING AUTH:" << authData.toUtf8();
        m_socket->write(authData.toUtf8());
        //m_chatArea->append("<i>Попытка входа...</i>");
    });



    // m_inputDevice = m_audioSource->start();
    m_inputDevice = m_audioSource->start();
    if (!m_inputDevice) {
        qDebug() << "КРИТИЧЕСКАЯ ОШИБКА: Микрофон не запустился!";
        return;
    }
    // connect(m_inputDevice, &QIODevice::readyRead, this,[this](){
    //     QByteArray data = m_inputDevice->readAll();
    //     if (m_isMuted || data.isEmpty()) return;

    //     // СЧИТАЕМ ГРОМКОСТЬ (RMS - среднеквадратичное)
    //     const int16_t *samples = reinterpret_cast<const int16_t*>(data.data());
    //     int sampleCount = data.size() / sizeof(int16_t);
    //     long long sum = 0;

    //     for (int i = 0; i < sampleCount; ++i) {
    //         sum += qAbs(samples[i]); // Складываем амплитуду всех звуковых волн
    //     }

    //     int averageVolume = (sampleCount > 0) ? (sum / sampleCount) : 0;

    //     // ОТСЕЧКА: Шлем только если громче порога
    //     if (averageVolume > m_voiceThreshold) {
    //         m_socket->write("VOICE_DATA:" + data);
    //         // qDebug() << "Голос активен, громкость:" << averageVolume;
    //     } else {
    //         // qDebug() << "Тишина... пропускаем";
    //     }
    // },Qt::UniqueConnection);

    m_voiceButton = new QPushButton("🎙️ Микро: Вкл", this);
    m_headsetButton = new QPushButton("🎧 Уши: Вкл", this);
    leftLayout->addWidget(m_voiceButton);
    // Добавляем их в интерфейс (например, в voiceLayout)
    // voiceLayout->addWidget(m_micButton);
    // voiceLayout->addWidget(m_headsetButton);

    // --- ЛОГИКА КНОПКИ МИКРОФОНА ---

    m_voiceChatButton = new QPushButton("🎤 Войти в Голос", this);
    m_voiceChatButton->setCheckable(true); // Чтобы она фиксировалась в нажатом состоянии
    m_voiceChatButton->setStyleSheet("background-color: #2d3436; color: white; font-weight: bold; padding: 10px;");
     leftLayout->insertWidget(0, m_voiceChatButton);

    connect(m_voiceChatButton, &QPushButton::clicked, [this](bool checked){
        if (checked) {
            // Запускаем микрофон, только если он остановлен
            if (m_audioSource->state() == QAudio::StoppedState) {
                m_inputDevice = m_audioSource->start();
                if (m_inputDevice) {
                    connect(m_inputDevice, &QIODevice::readyRead, this, [this](){
                        if (!m_isMuted) {
                            QByteArray data = m_inputDevice->readAll();
                            if (m_isMuted || data.isEmpty()) return;

                            // СЧИТАЕМ ГРОМКОСТЬ
                            const int16_t *samples = reinterpret_cast<const int16_t*>(data.data());
                            int sampleCount = data.size() / sizeof(int16_t);
                            long long sum = 0;

                            for (int i = 0; i < sampleCount; ++i) {
                                sum += qAbs(samples[i]);
                            }

                            int averageVolume = (sampleCount > 0) ? (sum / sampleCount) : 0;

                            // ВОТ ТУТ ИСПОЛЬЗУЕТСЯ ТВОЙ ПОРОГ
                            if (averageVolume > m_voiceThreshold) {
                                m_udpSocket->writeDatagram(data, QHostAddress("83.136.235.45"), 1235);
                                // qDebug() << "Голос прошел! Громкость:" << averageVolume;
                            }
                        }
                    });
                }
            }

            // Запускаем динамик
            if (m_audioSink->state() == QAudio::StoppedState) {
                m_outputDevice = m_audioSink->start();
            }

            m_socket->write("/voice_enter\n");
        } else {
            m_audioSource->stop();
            m_audioSink->stop();
            m_socket->write("/voice_leave\n");
        }
    });
    // --- ЛОГИКА КНОПКИ НАУШНИКОВ ---
    connect(m_headsetButton, &QPushButton::clicked, [this](){
        m_isDeaf = !m_isDeaf; // Переключаем флаг
        if (m_isDeaf) {
            m_headsetButton->setText("🎧 Уши: Выкл");
            m_audioSink->setVolume(0.0); // Глушим звук на 100%
        } else {
            m_headsetButton->setText("🎧 Уши: Вкл");
            m_audioSink->setVolume(1.0); // Возвращаем громкость на максимум
        }
    });





    // 3. ЛОГИКА ВХОДА И ВЫХОДА
    // connect(m_voiceChatButton, &QPushButton::clicked, [this](bool checked){
    //     if (checked) {
    //         // --- ВХОД В ГОЛОС ---
    //         m_voiceChatButton->setText("🛑 Выйти из Голоса");
    //         m_voiceChatButton->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");

    //         m_socket->write("/voice_enter\n");

    //         // Запускаем микрофон и динамик (тот код, что мы писали раньше)
    //         if (m_audioSource) m_inputDevice = m_audioSource->start();
    //         if (m_audioSink)   m_outputDevice = m_audioSink->start();

    //         m_chatArea->append("<b style='color:#2ecc71;'>Система: Голосовой канал активирован!</b>");
    //     } else {
    //         // --- ВЫХОД ИЗ ГОЛОСА ---
    //         m_voiceChatButton->setText("🎤 Войти в Голос");
    //         m_voiceChatButton->setStyleSheet("background-color: #2d3436; color: white; font-weight: bold;");

    //         m_socket->write("/voice_leave\n");

    //         // Стопаем железки
    //         if (m_audioSource) m_audioSource->stop();
    //         if (m_audioSink)   m_audioSink->stop();

    //         m_chatArea->append("<b style='color:#e74c3c;'>Система: Вы вышли из голосового канала.</b>");
    //     }
    // });

    m_udpSocket = new QUdpSocket(this);
    // Биндим на тот же порт 1235, чтобы ловить ответ от сервера
    m_udpSocket->bind(QHostAddress::AnyIPv4, 1235, QUdpSocket::ShareAddress);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, [this](){
        while (m_udpSocket->hasPendingDatagrams()) {
            QByteArray data = m_udpSocket->receiveDatagram().data();
            if (!m_isDeaf && m_outputDevice) {
                m_outputDevice->write(data); // СРАЗУ В ДИНАМИК
            }
        }
    });

}
void ChatClient::connectToServer()
{
    m_socket->connectToHost("83.136.235.45", 1234);
}

void ChatClient::onConnected() {
    m_chatArea->append("✅ Подключено!");
    m_socket->write(m_myUserName->text().toUtf8());
}

void ChatClient::onAttachFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Выберите картинку", "", "Images (*.png *.jpg *.jpeg)");

    if (!filePath.isEmpty()) {
        sendFile(filePath); // Если файл выбран — отправляем
    }
}

void ChatClient::sendFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray fileData = file.readAll();

    // ВНИМАНИЕ: Не давай грузить файлы по 100Мб, а то сервак ляжет
    if (fileData.size() > 5 * 1024 * 1024) { // Лимит 5 Мб
        m_chatArea->append("<b style='color:red;'>Ошибка: Файл слишком большой! (Max 5MB)</b>");
        return;
    }

    QString fileName = QFileInfo(filePath).fileName();
    QString target = m_targetName->text();
    if (target.isEmpty()) return;

    // Заголовок: FILE:Кому:Имя:Размер:
    QByteArray header = QString("FILE:%1:%2:%3:").arg(target, fileName).arg(fileData.size()).toUtf8();

    m_socket->write(header + fileData);
    m_socket->write("\n");

    m_chatArea->append("<b style='color:blue;'>Вы отправили файл: " + fileName + "</b>");
}

void ChatClient::startRecording()
{
    QAudioFormat format;
    format.setSampleRate(16000); // 16кГц для голоса за глаза
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    m_audioFile.setFileName("temp_voice.raw");
    if (!m_audioFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    m_audioSource = new QAudioSource(format, this);
    m_audioSource->start(&m_audioFile);
    qDebug() << "Запись пошла...";
}

void ChatClient::stopRecording()
{
    m_audioSource->stop();
    m_audioFile.close();
    delete m_audioSource;
    m_audioSource = nullptr;

    // А теперь отправляем этот файл через наш уже готовый sendFile!
    sendFile("temp_voice.raw");
}

void ChatClient::sendMessage() {
    QString text = m_messageEdit->text().trimmed();
    if (text.isEmpty()) return;

    if (text.startsWith("/")) {
        m_socket->write((text + "\n").toUtf8()); // Добавляем \n для надежности
    }
    else {
        QString target = m_targetName->text().trimmed();

        if (target.isEmpty()) {
            m_chatArea->append("<b style='color:red;'>Система: Выберите получателя!</b>");
            return;
        }

        // --- КРИТИЧЕСКИЙ ФИКС ДЛЯ ОБЩАКА ---
        // Если мы выбрали золотой пункт "ОБЩИЙ ЧАТ", маскируем его под GROUP_CHAT
        if (target == "ОБЩИЙ ЧАТ" || target.contains("ОБЩИЙ ЧАТ")) {
            target = "GROUP_CHAT";
        }

        // Формируем данные: "Кому:Текст\n"
        QString data = target + ":" + text + "\n";
        m_socket->write(data.toUtf8());

        // Очищаем поле ввода сразу после отправки
        m_messageEdit->clear();
    }
}

QByteArray ChatClient::addWavHeader(QByteArray data) {
    QByteArray header;
    qint32 fileSize = 36 + data.size();
    qint32 sampleRate = 16000;
    qint16 channels = 1;
    qint16 bytesPerSample = 2;
    qint32 byteRate = sampleRate * channels * bytesPerSample;
    qint16 blockAlign = channels * bytesPerSample;
    qint16 bitsPerSample = 16;
    qint32 fmtChunkSize = 16;
    qint16 audioFormat = 1; // PCM

    header.append("RIFF", 4);
    header.append(reinterpret_cast<const char*>(&fileSize), 4);
    header.append("WAVE", 4);
    header.append("fmt ", 4);
    header.append(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    header.append(reinterpret_cast<const char*>(&audioFormat), 2);
    header.append(reinterpret_cast<const char*>(&channels), 2);
    header.append(reinterpret_cast<const char*>(&sampleRate), 4);
    header.append(reinterpret_cast<const char*>(&byteRate), 4);
    header.append(reinterpret_cast<const char*>(&blockAlign), 2);
    header.append(reinterpret_cast<const char*>(&bitsPerSample), 2);
    header.append("data", 4);
    qint32 dataSize = data.size();
    header.append(reinterpret_cast<const char*>(&dataSize), 4);

    return header + data;
}

void ChatClient::renderTextMessage(const QString &sender, const QString &text, const QString &time)
{
    QString myNick = m_myUserName->text().trimmed();
    QString align = (sender == myNick) ? "right" : "left";
    QString bgColor = (sender == myNick) ? "#6c5ce7" : "#2d3436";

    m_chatArea->insertHtml(QString(
                               "<table width=\"100%\"><tr><td align=\"%1\">"
                               "<div style=\"background-color: %2; color: white; padding: 8px 15px; border-radius: 15px;\">"
                               "<b style=\"color: #f1c40f;\">%3:</b> %4 <small style=\"color: #bdc3c7;\">%5</small>"
                               "</div></td></tr></table><br>"
                               ).arg(align, bgColor, sender, text, time));

    m_chatArea->moveCursor(QTextCursor::End);
}

void ChatClient::onReadyRead() {
    // 1. Считываем всё один раз!
    QByteArray rawData = m_socket->readAll();
    //if (rawData.isEmpty()) return;

    static QByteArray buffer;

    // 2. ХИРУРГИЯ: Отделяем голос от общего потока байтов
    if (rawData.contains("VOICE_DATA:")) {
        int idx = rawData.indexOf("VOICE_DATA:");
        if (!m_isDeaf && m_outputDevice) {
            m_outputDevice->write(rawData.mid(idx + 11)); // Шлем в уши
        }
        return;
    }
    buffer.append(rawData);

    // 3. ТВОЙ ЦИКЛ ОБРАБОТКИ (Файлы и Текст)
    while (buffer.size() > 0) {
        QString myNick = m_myUserName->text().trimmed();
        QString currentTarget = m_targetName->text().trimmed();

        // --- ОБРАБОТКА ФАЙЛОВ И МЕДИА ---
        if (buffer.startsWith("FILE_REC:")) {
            int first = buffer.indexOf(':'), second = buffer.indexOf(':', first + 1);
            int third = buffer.indexOf(':', second + 1), fourth = buffer.indexOf(':', third + 1);
            if (fourth == -1) break;

            int fileSize = buffer.mid(third + 1, fourth - third - 1).toInt();
            int headerSize = fourth + 1;
            if (buffer.size() < (headerSize + fileSize)) break;

            QString sender = QString::fromUtf8(buffer.mid(first + 1, second - first - 1));
            QString fileName = QString::fromUtf8(buffer.mid(second + 1, third - second - 1));
            QByteArray fileBytes = buffer.mid(headerSize, fileSize);
            buffer.remove(0, headerSize + fileSize);

            if (sender != currentTarget && sender != myNick) {
                m_unreadCounts[sender]++;
                for(int i = 0; i < m_userListWidget->count(); ++i) {
                    QListWidgetItem* item = m_userListWidget->item(i);
                    // Извлекаем имя пользователя из текста элемента списка
                    QString nameInList = item->text().mid(2).section(" [", 0, 0).trimmed();

                    if(nameInList == sender) {
                        bool isOnline = item->text().startsWith("●");
                        QString statusPrefix = isOnline ? "● " : "○ ";
                        item->setBackground(QColor(255, 107, 107)); // Красный фон
                        item->setForeground(Qt::white);            // Белый текст
                        item->setText(QString("%1%2 [%3]").arg(statusPrefix, sender).arg(m_unreadCounts[sender]));
                        break;
                    }
                }
                continue;
            }

            // Твоя отрисовка картинок и WAV
            QString align = (sender == myNick) ? "right" : "left";
            QString bgColor = (sender == myNick) ? "#6c5ce7" : "#dfe6e9";
            QImage img;
            if (img.loadFromData(fileBytes)) {
                QImage scaledImg = img.width() > 500 ? img.scaledToWidth(500) : img;
                QString resName = QString("img_%1").arg(QDateTime::currentMSecsSinceEpoch());
                m_chatArea->document()->addResource(QTextDocument::ImageResource, QUrl(resName), scaledImg);
                m_chatArea->insertHtml(QString("<table width='100%'><tr><td align='%1'><div style='background-color: %2; padding: 8px; border-radius: 12px;'><img src='%3' width='350'></div></td></tr></table><br>").arg(align, bgColor, resName));
            } else if (fileName.endsWith(".wav")) {
                QString voiceFileName = QString("voice_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());
                QFile tempFile(voiceFileName);
                if (tempFile.open(QIODevice::WriteOnly)) { tempFile.write(addWavHeader(fileBytes)); tempFile.close(); }
                m_chatArea->insertHtml(QString("<div align='%1'><div style='background-color: %2; padding: 10px; border-radius: 12px; color: white;'><b>🎤 Голосовое:</b> <a href='play:%3' style='color: yellow;'>[ ПРОСЛУШАТЬ ]</a></div></div><br>").arg(align, bgColor, voiceFileName));
            }
        }
        // --- ОБРАБОТКА ТЕКСТА ---
        else {
            int lineEnd = buffer.indexOf('\n');
            if (lineEnd == -1) break;

            QByteArray lineData = buffer.left(lineEnd);
            buffer.remove(0, lineEnd + 1);
            QString message = QString::fromUtf8(lineData).trimmed();

            qDebug() << "SERVER_SAYS:" << message;

            if (message.isEmpty()) continue;

            // ОБЩИЙ ЧАТ
            if (message.startsWith("GROUP_MSG:")) {
                QString timeStr = message.section(':', 1, 2);
                QString sender  = message.section(':', 3, 3);
                QString text    = message.section(':', 4);
                m_chatArea->insertHtml(QString("<br><b style='color:#f1c40f'>%1:</b> %2 <small>(%3)</small><br>").arg(sender, text, timeStr));
                continue;
            }

            // АВТОРИЗАЦИЯ (Твой код)
            if (message == "AUTH_OK" || message.startsWith("AUTH_OK:")) {
                m_chatArea->append("<b style='color:#2ecc71;'>Система: Вход выполнен!</b>");
                m_myUserName->setEnabled(false); m_passwordEdit->setVisible(false);
                continue;
            }

            // СПИСОК ЮЗЕРОВ (Твой код)
            if (message.startsWith("USERS_LIST:")) {
                QStringList pairs = message.mid(11).split(',', Qt::SkipEmptyParts);
                m_userListWidget->clear();
                QListWidgetItem *groupItem = new QListWidgetItem("📢 [ ОБЩИЙ ЧАТ ]");
                groupItem->setForeground(QColor("#f1c40f"));
                m_userListWidget->addItem(groupItem);
                for (const QString &pair : pairs) {
                    QString name = pair.section(':', 0, 0);
                    bool isOnline = pair.section(':', 1, 1) == "1";
                    QListWidgetItem *item = new QListWidgetItem((isOnline ? "● " : "○ ") + name);
                    item->setForeground(isOnline ? QColor("#2ecc71") : QColor("#95a5a6"));
                    m_userListWidget->addItem(item);
                }
                continue;
            }

            // ЛИЧНЫЕ СООБЩЕНИЯ
            if (message.contains(": ")) {
                QString timeStr = message.left(5);
                QString rest = message.mid(6);
                QString sender = rest.section(": ", 0, 0).trimmed();
                QString text = rest.section(": ", 1).trimmed();

                if (sender == currentTarget || sender == myNick) {
                    QString align = (sender == myNick) ? "right" : "left";
                    m_chatArea->insertHtml(QString("<div align='%1'><b>%2:</b> %3 <small>%4</small></div><br>").arg(align, sender, text, timeStr));
                }
            }
        }
    }
    m_chatArea->moveCursor(QTextCursor::End);
}
