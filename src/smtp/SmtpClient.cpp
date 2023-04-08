#include "SmtpClient.h"

#include <map>
#include <iostream>
#include <QDebug>
#include <QSslSocket>
#include <QTextStream>
#include <QFileInfo>

QString SmtpClient::create_mail_message()
{
    //get mime type by file suffix
    const auto& mime_type_get{[](const QString& file_name){
            const QFileInfo file_info(file_name);
            const QString& suffix {file_info.suffix()};

            const std::map<QString,QString>& types_map{
                {"aac", "audio/aac"},
                {"abw", "application/x-abiword"},
                {"arc", "application/x-freearc"},
                {"avif","image/avif"},
                {"avi", "video/x-msvideo"},
                {"azw",  "application/vnd.amazon.ebook"},
                {"bin",  "application/octet-stream"},
                {"bmp",  "image/bmp"},
                {"bz",   "application/x-bzip"},
                {"bz2",  "application/x-bzip2"},
                {"cda",  "application/x-cdf"},
                {"csh",  "application/x-csh"},
                {"css",  "text/css"},
                {"csv",  "text/csv"},
                {"doc",  "application/msword"},
                {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
                {"eot",  "application/vnd.ms-fontobject"},
                {"epub", "application/epub+zip"},
                {"gz",   "application/gzip"},
                {"gif",  "image/gif"},
                {"htm",  "text/html"},
                {"html", "text/html"},
                {"ico",  "image/vnd.microsoft.icon"},
                {"ics",  "text/calendar"},
                {"jar",  "application/java-archive"},
                {"jpg",  "image/jpeg"},
                {"jpeg", "image/jpeg"},
                {"js",   "text/javascript"},
                {"json",  "application/json"},
                {"jsonld","application/ld+json"},
                {"mid", "audio/midi"},
                {"midi", "audio/midi"},
                {"mid", "audio/x-midi"},
                {"midi", "audio/x-midi"},
                {"mjs", "text/javascript"},
                {"mp3", "audio/mpeg"},
                {"mp4", "video/mp4"},
                {"mpeg", "video/mpeg"},
                {"mpkg", "application/vnd.apple.installer+xml"},
                {"odp", "application/vnd.oasis.opendocument.presentation"},
                {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
                {"odt", "application/vnd.oasis.opendocument.text"},
                {"oga", "audio/ogg"},
                {"ogv", "video/ogg"},
                {"ogx", "application/ogg"},
                {"opus", "audio/opus"},
                {"otf", "font/otf"},
                {"png", "image/png"},
                {"pdf", "application/pdf"},
                {"php", "application/x-httpd-php"},
                {"ppt", "application/vnd.ms-powerpoint"},
                {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
                {"rar", "application/vnd.rar"},
                {"rtf", "application/rtf"},
                {"sh", "application/x-sh"},
                {"svg", "image/svg+xml"},
                {"tar", "application/x-tar"},
                {"tif", "image/tiff"},
                {"tiff", "image/tiff"},
                {"ts", "video/mp2t"},
                {"ttf", "font/ttf"},
                {"txt", "text/plain"},
                {"vsd", "application/vnd.visio"},
                {"wav", "audio/wav"},
                {"weba", "audio/webm"},
                {"webm", "video/webm"},
                {"woff", "font/woff"},
                {"woff2", "font/woff2"},
                {"xhtml", "application/xhtml+xml"},
                {"xls", "application/vnd.ms-excel"},
                {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
                {"xml", "application/xml"},
                {"xul", "application/vnd.mozilla.xul+xml"},
                {"zip", "application/zip"},
                {"3gp", "video/3gpp"},
                {"3gp2", "video/3gpp2"},
                {"7z", "application/x-7z-compressed"}
            };
            const auto& found {types_map.find(suffix)};
            if(found!=types_map.end()){
                return found->second;
            }
            return QString("application/octet-stream");
        }
    };

    QString message {};
    message.append(QString("From: %1 <%2>\n").arg(from_user_, from_mail_));
    message.append(QString("To: %1 <%2>").arg(to_user_, to_mail_));

    if(!additional_to_list_.empty()){
        for(const QString& add_to_mail: additional_to_list_){
            QString add_to_user {add_to_mail.split("@").first()};
            message.append(QString(", %1 <%2>").arg(add_to_user,add_to_mail));
        }
    }
    message.append("\n");

    message.append(QString("Subject: %1\n").arg(subject_));

    message.append("MIME-Version: 1.0\n");
    message.append("Content-Type: multipart/mixed; boundary=frontier\n\n");

    message.append( "--frontier\n" );
    message.append("Content-Type: text/plain\n\n");
    message.append(message_);
    message.append("\n\n");

    message.append("--frontier\n" );
    message.append(QString("Content-Type: %1\n").arg(mime_type_get(file_name_)));
    message.append(QString("Content-Disposition: attachment; filename=%1;\n").arg(file_name_));
    message.append("Content-Transfer-Encoding: base64\n\n");
    message.append(file_content_.toBase64() + "\n");

    message.append( "--frontier--\n" );
    message.append("\r\n.");
    //std::cout<<message.toStdString();
    return message;
}

void SmtpClient::slot_connected()
{
    qDebug()<<"socket connected";
}

void SmtpClient::slot_diconnected()
{
    qDebug()<<"socket disconnected";
}

void SmtpClient::slot_ready_read()
{
    QString response {};
    do{
        response=ssl_socket_ptr_->readAll();
    }
    while(ssl_socket_ptr_->canReadLine());
    const QStringList& smtp_commands {response.split("\r\n")};

    for(const QString& command: smtp_commands){
        if(!command.isEmpty()){
            smtp_queue_.enqueue(command);
        }
    }

    while(!smtp_queue_.empty()){
        const QString line {smtp_queue_.dequeue()};

        bool ok {false};
        const QString& code_str {line.mid(0,3)};
        const int& code {code_str.toInt(&ok)};

        if(ok){
            if(state_==INIT && code==220){
                ssl_socket_ptr_->write("EHLO localhost\r\n");
                ssl_socket_ptr_->flush();
                state_=HANDSHAKE;
                continue;
            }
            else if(state_==HANDSHAKE && code==250){
                ssl_socket_ptr_->write("EHLO localhost\r\n");
                ssl_socket_ptr_->flush();
                state_=AUTH;
                continue;
            }
            else if(state_==AUTH && code==250){
                ssl_socket_ptr_->write("AUTH LOGIN\r\n");
                ssl_socket_ptr_->flush();
                state_=USER;
                continue;
            }
            else if(state_==USER && code==334){
                ssl_socket_ptr_->write(from_mail_.toUtf8().toBase64());
                ssl_socket_ptr_->write("\r\n");
                ssl_socket_ptr_->flush();
                state_=PASS;
                continue;
            }
            else if(state_==PASS && code==334){
                ssl_socket_ptr_->write(from_password_.toUtf8().toBase64());
                ssl_socket_ptr_->write("\r\n");
                ssl_socket_ptr_->flush();
                state_=MAIL;
                continue;
            }
            else if(state_==MAIL && code==235){
                ssl_socket_ptr_->write(QString("MAIL FROM: <%1>").arg(from_mail_).toUtf8());
                ssl_socket_ptr_->write("\r\n");
                ssl_socket_ptr_->flush();
                state_=RCPT;
                continue;
            }
            else if(state_==RCPT && code==250){
                ssl_socket_ptr_->write(QString("RCPT TO: <%1>").arg(to_mail_).toUtf8());
                ssl_socket_ptr_->write("\r\n");
                ssl_socket_ptr_->flush();
                state_=DATA;
                continue;
            }
            else if(state_==DATA && code==250){
                ssl_socket_ptr_->write("DATA\r\n");
                ssl_socket_ptr_->flush();
                state_=BODY;
                continue;
            }
            else if(state_==BODY && code==354){
                const QString& mail_message {create_mail_message()};
                ssl_socket_ptr_->write(mail_message.toUtf8());
                ssl_socket_ptr_->write("\r\n");
                ssl_socket_ptr_->flush();
                state_=QUIT;
                continue;
            }
            else if(state_==QUIT && code==250){
                ssl_socket_ptr_->write("QUIT\r\n");
                ssl_socket_ptr_->flush();
                state_=CLOSE;
                continue;
            }
            else if(state_==CLOSE && code==221){
                ssl_socket_ptr_->close();
                emit signal_completed(true, "");
                return;
            }
            else if(code > min_smtp_err_code_){
                ssl_socket_ptr_->close();
                emit signal_completed(false, line);
                return;
            }
        }
    }
}

SmtpClient::SmtpClient(const QJsonObject &init_object, const QStringList &additional_to_list, QObject *parent):
    QObject(parent),additional_to_list_{additional_to_list}
{

    ssl_socket_ptr_.reset(new QSslSocket);
    QObject::connect(ssl_socket_ptr_.get(),&QSslSocket::connected,
                     this, &SmtpClient::slot_connected);

    QObject::connect(ssl_socket_ptr_.get(), &QSslSocket::readyRead,
                     this, &SmtpClient::slot_ready_read);

    QObject::connect(ssl_socket_ptr_.get(),&QSslSocket::disconnected,
                     this, &SmtpClient::slot_diconnected);

    smtp_host_=init_object.value("smtp_host").toString();
    smtp_port_=init_object.value("smtp_port").toInt();

    to_mail_=init_object.value("to_mail").toString();
    from_mail_=init_object.value("from_mail").toString();
    from_password_=init_object.value("from_password").toString();

    to_user_ =to_mail_.split("@").first();
    from_user_ =from_mail_.split("@").first();
}

void SmtpClient::send_email(const QString &subject, const QString &message, const QString& file_name, const QByteArray& file_content)
{
    subject_=subject;
    message_=message;
    file_name_=file_name;
    file_content_=file_content;

    ssl_socket_ptr_->connectToHostEncrypted(smtp_host_,smtp_port_);
    const bool& success {ssl_socket_ptr_->waitForConnected(1000 * 10)};
    if(!success){
        ssl_socket_ptr_->close();

        const QString& msg {QString("Fail to connect to smtp server, server: %1, port: %2").arg(smtp_host_).arg(smtp_port_)};
        emit signal_completed(false, msg);
    }
}
