/*   Kate search plugin
 *
 * Copyright (C) 2011-2013 by Kåre Särs <kare.sars@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file called COPYING; if not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "search_open_files.h"

#include <QElapsedTimer>

SearchOpenFiles::SearchOpenFiles(QObject *parent)
    : QObject(parent)
{
    m_nextRunTimer.setInterval(0);
    m_nextRunTimer.setSingleShot(true);
    connect(&m_nextRunTimer, &QTimer::timeout, this, [this]() { doSearchNextFile(m_nextLine); });
}

bool SearchOpenFiles::searching()
{
    return !m_cancelSearch;
}

void SearchOpenFiles::startSearch(const QList<KTextEditor::Document *> &list, const QRegularExpression &regexp)
{
    if (m_nextFileIndex != -1)
        return;

    m_docList = list;
    m_nextFileIndex = 0;
    m_regExp = regexp;
    m_cancelSearch = false;
    m_terminateSearch = false;
    m_statusTime.restart();
    m_nextLine = 0;
    m_nextRunTimer.start(0);
}

void SearchOpenFiles::terminateSearch()
{
    m_cancelSearch = true;
    m_terminateSearch = true;
    m_nextFileIndex = -1;
    m_nextLine = -1;
    m_nextRunTimer.stop();
}

void SearchOpenFiles::cancelSearch()
{
    m_cancelSearch = true;
}

void SearchOpenFiles::doSearchNextFile(int startLine)
{
    if (m_cancelSearch || m_nextFileIndex >= m_docList.size()) {
        m_nextFileIndex = -1;
        m_cancelSearch = true;
        m_nextLine = -1;
        return;
    }

    // NOTE The document managers signal documentWillBeDeleted() must be connected to
    // cancelSearch(). A closed file could lead to a crash if it is not handled.
    int line = searchOpenFile(m_docList[m_nextFileIndex], m_regExp, startLine);
    if (line == 0) {
        // file searched go to next
        m_nextFileIndex++;
        if (m_nextFileIndex == m_docList.size()) {
            m_nextFileIndex = -1;
            m_cancelSearch = true;
            emit searchDone();
        } else {
            m_nextLine = 0;
        }
    } else {
        m_nextLine = line;
    }
    m_nextRunTimer.start();
}

int SearchOpenFiles::searchOpenFile(KTextEditor::Document *doc, const QRegularExpression &regExp, int startLine)
{
    if (m_statusTime.elapsed() > 100) {
        m_statusTime.restart();
        emit searching(doc->url().toString());
    }

    if (regExp.pattern().contains(QLatin1String("\\n"))) {
        return searchMultiLineRegExp(doc, regExp, startLine);
    }

    return searchSingleLineRegExp(doc, regExp, startLine);
}

int SearchOpenFiles::searchSingleLineRegExp(KTextEditor::Document *doc, const QRegularExpression &regExp, int startLine)
{
    int column;
    QElapsedTimer time;

    time.start();
    for (int line = startLine; line < doc->lines(); line++) {
        if (time.elapsed() > 100) {
            // qDebug() << "Search time exceeded" << time.elapsed() << line;
            return line;
        }
        QRegularExpressionMatch match;
        match = regExp.match(doc->line(line));
        column = match.capturedStart();
        while (column != -1 && !match.captured().isEmpty()) {
            emit matchFound(doc->url().toString(), doc->documentName(), doc->line(line), match.capturedLength(), line, column, line, column + match.capturedLength());
            match = regExp.match(doc->line(line), column + match.capturedLength());
            column = match.capturedStart();
        }
    }
    return 0;
}

int SearchOpenFiles::searchMultiLineRegExp(KTextEditor::Document *doc, const QRegularExpression &regExp, int inStartLine)
{
    int column = 0;
    int startLine = 0;
    QElapsedTimer time;
    time.start();
    QRegularExpression tmpRegExp = regExp;

    if (inStartLine == 0) {
        // Copy the whole file to a temporary buffer to be able to search newlines
        m_fullDoc.clear();
        m_lineStart.clear();
        m_lineStart << 0;
        for (int i = 0; i < doc->lines(); i++) {
            m_fullDoc += doc->line(i) + QLatin1Char('\n');
            m_lineStart << m_fullDoc.size();
        }
        if (!regExp.pattern().endsWith(QLatin1Char('$'))) {
            // if regExp ends with '$' leave the extra newline at the end as
            // '$' will be replaced with (?=\\n), which needs the extra newline
            m_fullDoc.remove(m_fullDoc.size() - 1, 1);
        }
    } else {
        if (inStartLine > 0 && inStartLine < m_lineStart.size()) {
            column = m_lineStart[inStartLine];
            startLine = inStartLine;
        } else {
            return 0;
        }
    }

    if (regExp.pattern().endsWith(QLatin1Char('$'))) {
        QString newPatern = tmpRegExp.pattern();
        newPatern.replace(QStringLiteral("$"), QStringLiteral("(?=\\n)"));
        tmpRegExp.setPattern(newPatern);
    }

    QRegularExpressionMatch match;
    match = tmpRegExp.match(m_fullDoc, column);
    column = match.capturedStart();
    while (column != -1 && !match.captured().isEmpty()) {
        // search for the line number of the match
        int i;
        startLine = -1;
        for (i = 1; i < m_lineStart.size(); i++) {
            if (m_lineStart[i] > column) {
                startLine = i - 1;
                break;
            }
        }
        if (startLine == -1) {
            break;
        }

        int startColumn = (column - m_lineStart[startLine]);
        int endLine = startLine + match.captured().count(QLatin1Char('\n'));
        int lastNL = match.captured().lastIndexOf(QLatin1Char('\n'));
        int endColumn = lastNL == -1 ? startColumn + match.captured().length() : match.captured().length() - lastNL - 1;

        emit matchFound(doc->url().toString(), doc->documentName(), doc->line(startLine).left(column - m_lineStart[startLine]) + match.captured(), match.capturedLength(), startLine, startColumn, endLine, endColumn);

        match = tmpRegExp.match(m_fullDoc, column + match.capturedLength());
        column = match.capturedStart();

        if (time.elapsed() > 100) {
            // qDebug() << "Search time exceeded" << time.elapsed() << line;
            return startLine;
        }
    }
    return 0;
}
