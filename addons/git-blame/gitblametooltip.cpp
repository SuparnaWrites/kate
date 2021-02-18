/*  SPDX-License-Identifier: MIT

    SPDX-FileCopyrightText: 2021 Waqar Ahmed <waqar.17a@gmail.com>
    SPDX-FileCopyrightText: 2021 Kåre Särs <kare.sars@iki.fi>

    SPDX-License-Identifier: MIT
*/
#include "gitblametooltip.h"

#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QString>
#include <QTextBrowser>
#include <QTimer>
#include <QScrollBar>

#include <KTextEditor/ConfigInterface>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>

using KSyntaxHighlighting::AbstractHighlighter;
using KSyntaxHighlighting::Format;

static QString toHtmlRgbaString(const QColor &color)
{
    if (color.alpha() == 0xFF)
        return color.name();

    QString rgba = QStringLiteral("rgba(");
    rgba.append(QString::number(color.red()));
    rgba.append(QLatin1Char(','));
    rgba.append(QString::number(color.green()));
    rgba.append(QLatin1Char(','));
    rgba.append(QString::number(color.blue()));
    rgba.append(QLatin1Char(','));
    // this must be alphaF
    rgba.append(QString::number(color.alphaF()));
    rgba.append(QLatin1Char(')'));
    return rgba;
}

class HtmlHl : public AbstractHighlighter
{
public:
    HtmlHl()
        : out(&outputString)
    {
    }

    void setText(const QString &txt)
    {
        text = txt;
        QTextStream in(&text);

        out.reset();
        outputString.clear();

        bool inDiff = false;

        KSyntaxHighlighting::State state;
        out << "<pre>";
        while (!in.atEnd()) {
            currentLine = in.readLine();

            // allow empty lines in code blocks, no ruler here
            if (!inDiff && currentLine.isEmpty()) {
                out << "<hr>";
                continue;
            }

            // diff block
            if (!inDiff && currentLine.startsWith(QLatin1String("diff"))) {
                inDiff = true;
            }

            state = highlightLine(currentLine, state);
            out << "\n";
        }
        out << "</pre>";
    }

    QString html() const
    {
        //        while (!out.atEnd())
        //            qWarning() << out.readLine();
        return outputString;
    }

protected:
    void applyFormat(int offset, int length, const Format &format) override
    {
        if (!length)
            return;

        QString formatOutput;

        if (format.hasTextColor(theme())) {
            formatOutput = toHtmlRgbaString(format.textColor(theme()));
        }

        if (!formatOutput.isEmpty()) {
            out << "<span style=\"color:" << formatOutput << "\">";
        }

        out << currentLine.mid(offset, length).toHtmlEscaped();

        if (!formatOutput.isEmpty()) {
            out << "</span>";
        }
    }

private:
    QString text;
    QString currentLine;
    QString outputString;
    QTextStream out;
};

class Tooltip : public QTextBrowser
{
    Q_OBJECT

public:
    static Tooltip *self()
    {
        static Tooltip instance;
        return &instance;
    }

    void setTooltipText(const QString &text)
    {
        if (text.isEmpty())
            return;

        m_htmlHl.setText(text);
        setHtml(m_htmlHl.html());
    }

    void setView(KTextEditor::View *view)
    {
        // view changed?
        // => update definition
        // => update font
        if (view != m_view) {
            if (m_view && m_view->focusProxy()) {
                m_view->focusProxy()->removeEventFilter(this);
            }

            m_view = view;

            m_htmlHl.setDefinition(m_syntaxHlRepo.definitionForName(QStringLiteral("Diff")));
            updateFont();

            if (m_view && m_view->focusProxy()) {
                m_view->focusProxy()->installEventFilter(this);
            }
        }
    }

    Tooltip(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::BypassGraphicsProxyWidget | Qt::ToolTip);
        document()->setDocumentMargin(5);
        setFrameStyle(QFrame::Box | QFrame::Raised);
        connect(&m_hideTimer, &QTimer::timeout, this, &Tooltip::hideTooltip);

        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto updateColors = [this](KTextEditor::Editor *e) {
            auto theme = e->theme();
            m_htmlHl.setTheme(theme);

            auto pal = palette();
            const QColor bg = theme.editorColor(KSyntaxHighlighting::Theme::BackgroundColor);
            pal.setColor(QPalette::Base, bg);
            const QColor normal = theme.textColor(KSyntaxHighlighting::Theme::Normal);
            pal.setColor(QPalette::Text, normal);
            setPalette(pal);

            updateFont();
        };
        updateColors(KTextEditor::Editor::instance());
        connect(KTextEditor::Editor::instance(), &KTextEditor::Editor::configChanged, this, updateColors);
    }

    bool eventFilter(QObject *, QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::WindowActivate:
        case QEvent::WindowDeactivate:
            hideTooltip();
            break;
        default:
            break;
        }
        return false;
    }

    void updateFont()
    {
        if (!m_view)
            return;
        auto ciface = qobject_cast<KTextEditor::ConfigInterface *>(m_view);
        auto font = ciface->configValue(QStringLiteral("font")).value<QFont>();
        setFont(font);
    }

    Q_SLOT void hideTooltip()
    {
        close();
        setText(QString());
    }

    void fixGeometry()
    {
        static QScrollBar scrollBar(Qt::Horizontal);
        QFontMetrics fm(font());
        QSize size = fm.size(Qt::TextSingleLine, QStringLiteral("m"));
        int fontHeight = size.height();
        size.setHeight(m_view->height() - fontHeight * 2 - scrollBar.sizeHint().height());
        size.setWidth(qRound(m_view->width() * 0.7));
        resize(size);

        QPoint p = m_view->mapToGlobal(m_view->pos());
        p.setY(p.y() + fontHeight);
        p.setX(p.x() + m_view->textAreaRect().left() + m_view->textAreaRect().width() - size.width() - fontHeight);
        this->move(p);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        m_hideTimer.start(3000);
        return QTextBrowser::showEvent(event);
    }

    void enterEvent(QEvent *event) override
    {
        inContextMenu = false;
        m_hideTimer.stop();
        return QTextBrowser::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (!m_hideTimer.isActive() && !inContextMenu) {
            hideTooltip();
        }
        return QTextBrowser::leaveEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        auto pos = event->pos();
        if (rect().contains(pos)) {
            return QTextBrowser::mouseMoveEvent(event);
        }
        hideTooltip();
    }

    void contextMenuEvent(QContextMenuEvent *e) override
    {
        inContextMenu = true;
        return QTextBrowser::contextMenuEvent(e);
    }

private:
    bool inContextMenu = false;
    KTextEditor::View *m_view;
    QTimer m_hideTimer;
    HtmlHl m_htmlHl;
    KSyntaxHighlighting::Repository m_syntaxHlRepo;
};

void GitBlameTooltip::show(const QString &text, KTextEditor::View *v)
{
    if (text.isEmpty() || !v || !v->document()) {
        return;
    }

    Tooltip::self()->setView(v);
    Tooltip::self()->setTooltipText(text);
    Tooltip::self()->fixGeometry();
    Tooltip::self()->raise();
    Tooltip::self()->show();
}

#include "gitblametooltip.moc"
