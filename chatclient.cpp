#include "chatclient.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFileDialog>
#include <QSoundEffect>
#include <QTextBrowser>
#include <QCoreApplication>

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
    m_targetName = new QLineEdit(this);
    m_messageEdit = new QLineEdit(this);
    m_attachButton = new QPushButton("📎", this);

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
    mainLayout->addWidget(m_attachButton);

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

    connect(m_micButton, &QPushButton::pressed, this, &ChatClient::startRecording);
    connect(m_micButton, &QPushButton::released, this, &ChatClient::stopRecording);

    m_chatArea->setOpenExternalLinks(false);
    m_chatArea->setOpenLinks(false);
    m_chatArea->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_chatArea->setReadOnly(true);

    connect(m_chatArea, &QTextBrowser::anchorClicked, [this](const QUrl &url){
        QString link = url.toString();

        if (link.startsWith("play:")) {
            // Вырезаем имя файла (все что после "play:")
            QString fileName = link.mid(5);

            // Формируем полный путь к папке, где запущен мессенджер
            QString fullPath = QCoreApplication::applicationDirPath() + "/" + fileName;

            qDebug() << "Попытка воспроизведения:" << fullPath;

            if (QFile::exists(fullPath)) {
                QSoundEffect *player = new QSoundEffect(this);
                player->setSource(QUrl::fromLocalFile(fullPath));
                player->setVolume(1.0f);

                // Удаляем объект плеера после завершения звука (чтобы не жрать память)
                connect(player, &QSoundEffect::playingChanged, [player](){
                    if (!player->isPlaying()) player->deleteLater();
                });

                player->play();
            } else {
                qDebug() << "ОШИБКА: Файл не найден по пути:" << fullPath;
            }
        }
    });


    connect(m_userListWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item)
            {
                QString sender = item->text().section(" [", 0, 0); // Достаем чистое имя
                m_unreadCounts[sender] = 0;

                item->setBackground(Qt::transparent);
                item->setForeground(Qt::black);
                item->setText(sender);

                m_targetName->setText(sender);
                m_chatArea->clear();
                m_socket->write(QString("/get_history %1\n").arg(sender).toUtf8());
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

        m_socket->write(text.toUtf8());

        // m_chatArea->append("<i style='color:gray;'>Отправка команды: " + text + "</i>");
    }
    else {
        QString target = m_targetName->text().trimmed();

        if (target.isEmpty()) {
            m_chatArea->append("<b style='color:red;'>Система: Выберите получателя в списке справа!</b>");
            return;
        }

        QString data = target + ":" + text;
        m_socket->write(data.toUtf8());

        QString time = QDateTime::currentDateTime().toString("hh:mm");
        //  m_chatArea->append(QString("<span style='color:gray;'>[%1]</span> <b style='color:green;'>Вы:</b> %2")
        // .arg(time, text));
    }

    m_messageEdit->clear();
    m_chatArea->moveCursor(QTextCursor::End);
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

void ChatClient::onReadyRead() {
    static QByteArray buffer;
    buffer.append(m_socket->readAll());

    while (buffer.size() > 0) {
        QString myNick = m_myUserName->text().trimmed();
        QString currentTarget = m_targetName->text().trimmed();

        // --- 1. ОБРАБОТКА ФАЙЛОВ И ГОЛОСОВЫХ ---
        if (buffer.startsWith("FILE_REC:")) {
            int first = buffer.indexOf(':'), second = buffer.indexOf(':', first + 1);
            int third = buffer.indexOf(':', second + 1), fourth = buffer.indexOf(':', third + 1);
            if (fourth == -1) return;

            int fileSize = buffer.mid(third + 1, fourth - third - 1).toInt();
            int headerSize = fourth + 1;
            if (buffer.size() < (headerSize + fileSize)) return;

            QString sender = QString::fromUtf8(buffer.mid(first + 1, second - first - 1));
            QString fileName = QString::fromUtf8(buffer.mid(second + 1, third - second - 1));
            QByteArray fileBytes = buffer.mid(headerSize, fileSize);
            buffer.remove(0, headerSize + fileSize);

            // СЧЕТЧИК ДЛЯ МЕДИА [!]
            if (sender != currentTarget && sender != myNick) {
                m_unreadCounts[sender]++;
                for(int i = 0; i < m_userListWidget->count(); ++i) {
                    QListWidgetItem* item = m_userListWidget->item(i);
                    if(item->text().section(" [", 0, 0) == sender) {
                        item->setBackground(QColor(255, 107, 107));
                        item->setForeground(Qt::white);
                        item->setText(QString("%1 [%2]").arg(sender).arg(m_unreadCounts[sender]));
                        break;
                    }
                }
                continue;
            }

            // Отрисовка медиа
            QString align = (sender == myNick) ? "right" : "left";
            QString bgColor = (sender == myNick) ? "#6c5ce7" : "#dfe6e9";
            QImage img;
            if (img.loadFromData(fileBytes)) {
                QImage scaledImg = img.width() > 500 ? img.scaledToWidth(500, Qt::SmoothTransformation) : img;
                QString resName = QString("img_%1").arg(QDateTime::currentMSecsSinceEpoch());
                m_chatArea->document()->addResource(QTextDocument::ImageResource, QUrl(resName), scaledImg);
                m_chatArea->insertHtml(QString("<table width='100%'><tr><td align='%1'><div style='background-color: %2; padding: 8px; border-radius: 12px;'><img src='%3' width='350'></div></td></tr></table><br>").arg(align, bgColor, resName));
            } else if (fileName.endsWith(".wav") || fileName.endsWith(".raw")) {
                QString voiceFileName = QString("voice_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());
                QFile tempFile(voiceFileName);
                if (tempFile.open(QIODevice::WriteOnly)) { tempFile.write(addWavHeader(fileBytes)); tempFile.close(); }
                m_chatArea->insertHtml(QString("<div align='%1'><div style='background-color: %2; padding: 10px; border-radius: 12px; color: white;'><b>🎤 Голосовое:</b> <a href='play:%3' style='color: yellow;'>[ ПРОСЛУШАТЬ ]</a></div></div><br>").arg(align, bgColor, voiceFileName));
            }
        }
        // --- 2. ОБРАБОТКА ТЕКСТА ---
        else {
            int lineEnd = buffer.indexOf('\n');
            if (lineEnd == -1) return;

            QByteArray lineData = buffer.left(lineEnd);
            buffer.remove(0, lineEnd + 1);
            QString message = QString::fromUtf8(lineData).trimmed();
            if (message.isEmpty()) continue;

            if (message.startsWith("USERS_LIST:")) {
                QString list = message.mid(11);
                QStringList users = list.split(',', Qt::SkipEmptyParts);
                QMap<QString, int> oldCounts = m_unreadCounts;
                m_userListWidget->clear();
                m_userListWidget->addItems(users);
                for(int i = 0; i < m_userListWidget->count(); ++i) {
                    QString name = m_userListWidget->item(i)->text();
                    if(oldCounts.value(name) > 0) {
                        QListWidgetItem* item = m_userListWidget->item(i);
                        item->setBackground(QColor(255, 107, 107));
                        item->setForeground(Qt::white);
                        item->setText(QString("%1 [%2]").arg(name).arg(oldCounts[name]));
                    }
                }
                continue;
            }

            if (message.contains(": ")) {
                QString timeStr = message.left(5);
                QString rest = message.mid(6);
                QString sender = rest.section(": ", 0, 0).trimmed();
                QString text = rest.section(": ", 1).trimmed();

                // СЧЕТЧИК ДЛЯ ТЕКСТА [!]
                if (sender != currentTarget && sender != myNick) {
                    m_unreadCounts[sender]++;
                    for(int i = 0; i < m_userListWidget->count(); ++i) {
                        QListWidgetItem* item = m_userListWidget->item(i);
                        if(item->text().section(" [", 0, 0) == sender) {
                            item->setBackground(QColor(255, 107, 107));
                            item->setForeground(Qt::white);
                            item->setText(QString("%1 [%2]").arg(sender).arg(m_unreadCounts[sender]));
                            break;
                        }
                    }
                    continue;
                }

                QString bgColor = (sender == myNick) ? "#6c5ce7" : "#dfe6e9";
                QString align = (sender == myNick) ? "right" : "left";
                m_chatArea->insertHtml(QString("<table width='100%'><tr><td align='%1'><div style='background-color: %2; color: white; padding: 6px 12px; border-radius: 12px;'><b>%3:</b> %4 <span style='font-size: 8px;'>%5</span></div></td></tr></table>").arg(align, bgColor, (sender == myNick ? "Вы" : sender), text, timeStr));
            }
        }
    }
    m_chatArea->moveCursor(QTextCursor::End);
}
