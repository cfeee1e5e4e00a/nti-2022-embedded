def send_notification(email, txt, theme):
    import smtplib
    """Send message from sender, get 'email' - receiver, 'txt' - message text, 'theme' - theme of mail"""
    sender = 'uchansansan@cfeee1e5e4e00a.ru'
    sender_password = 'lnhvwqldkhovdzxt'
    mail_lib = smtplib.SMTP_SSL('smtp.yandex.ru', 465)
    mail_lib.login(sender, sender_password)
    msg = 'From: %s\r\nTo: %s\r\nContent-Type: text/plain; charset="utf-8"\r\nSubject: %s\r\n\r\n' % (
    sender, email, theme)
    msg += txt
    mail_lib.sendmail(sender, email, msg.encode('utf8'))
    mail_lib.quit()

send_notification('uchansansan@gmail.com', 'Hello', 'Умная больница')