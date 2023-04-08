#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <QQueue>
#include <QObject>
#include <QJsonObject>
#include <QSharedPointer>

#include <mutex>
#include <condition_variable>

class QSslSocket;
class QTcpSocket;
class QTextStream;

/*
 *init object structure
 *-------------------------
 *smtp_host=smtp.mail.ru
 *smtp_port=465
 *to_mail=example@mail.ru
 *from_mail=example@mail.ru
 *from_password=*******
 *-------------------------
 */

class SmtpClient:public QObject
{
    Q_OBJECT
private:
    const int min_smtp_err_code_ {400};
    enum State{
        INIT,
        TLS,
        HANDSHAKE,
        AUTH,
        USER,
        PASS,
        RCPT,
        MAIL,
        DATA,
        BODY,
        QUIT,
        CLOSE
    };

    QString smtp_host_ {};
    int smtp_port_ {};

    QString to_mail_ {};
    QString to_user_ {};
    QString from_user_ {};
    QString from_mail_ {};
    QString from_password_ {};

    QString subject_ {};
    QString message_ {};

    QString file_name_ {};
    QByteArray file_content_ {};
    QStringList additional_to_list_ {};

    State state_ {INIT};
    QQueue<QString> smtp_queue_ {};
    QJsonObject init_object_ {};
    QSharedPointer<QSslSocket> ssl_socket_ptr_ {nullptr};

    QString create_mail_message();

private slots:
    void slot_connected();
    void slot_diconnected();
    void slot_ready_read();

public:
    SmtpClient(const QJsonObject& init_object, const QStringList& additional_to_list, QObject* parent=nullptr);
    virtual ~SmtpClient()=default;
    void send_email(const QString& subject, const QString& message, const QString& file_name, const QByteArray& file_content);

signals:
    void signal_completed(bool ok, const QString& message);
};

#endif // SMTPCLIENT_H
