/* This file is part of the KDE libraries
   Copyright (C) 2003 Anders Lund <anders@alweb.dk>
   Copyright (C) 2001-2004 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Charles Samuels <charles@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "katecmds.h"

#include "katedocument.h"
#include "kateview.h"
#include "kateconfig.h"
#include "kateautoindent.h"

#include <kdebug.h>
#include <klocale.h>

#include <qregexp.h>

// syncs a config flag in the document with a boolean value
static void setDocFlag( KateDocumentConfig::ConfigFlags flag, bool enable,
                  KateDocument *doc )
{
  doc->config()->setConfigFlags( flag, enable );
}

// this returns wheather the string s could be converted to
// a bool value, one of on|off|1|0|true|false. the argument val is
// set to the extracted value in case of success
static bool getBoolArg( QString s, bool *val  )
{
  bool res( false );
  s = s.lower();
  res = (s == "on" || s == "1" || s == "true");
  if ( res )
  {
    *val = true;
    return true;
  }
  res = (s == "off" || s == "0" || s == "false");
  if ( res )
  {
    *val = false;
    return true;
  }
  return false;
}

QStringList KateCommands::CoreCommands::cmds()
{
  QStringList l;
  l << "indent" << "unindent" << "cleanindent"
    << "comment" << "uncomment" << "goto"
    << "set-tab-width" << "set-replace-tabs" << "set-show-tabs"
    << "set-remove-trailing-space"
    << "set-indent-spaces" << "set-indent-width" << "set-indent-mode" << "set-auto-indent"
    << "set-line-numbers" << "set-folding-markers" << "set-icon-border"
    << "set-word-wrap" << "set-word-wrap-column"
    << "set-replace-tabs-save" << "set-remove-trailing-space-save";
  return l;
}

bool KateCommands::CoreCommands::exec(Kate::View *view,
                            const QString &_cmd,
                            QString &errorMsg)
{
#define KCC_ERR(s) { errorMsg=s; return false; }
  // cast it hardcore, we know that it is really a kateview :)
  KateView *v = (KateView*) view;

  if ( ! v )
    KCC_ERR( i18n("Could not access view") );

  //create a list of args
  QStringList args( QStringList::split( QRegExp("\\s+"), _cmd ) );
  QString cmd ( args.first() );
  args.remove( args.first() );

  // ALL commands that takes no arguments.
  if ( cmd == "indent" )
  {
    v->indent();
    return true;
  }
  else if ( cmd == "unindent" )
  {
    v->unIndent();
    return true;
  }
  else if ( cmd == "cleanindent" )
  {
    v->cleanIndent();
    return true;
  }
  else if ( cmd == "comment" )
  {
    v->comment();
    return true;
  }
  else if ( cmd == "uncomment" )
  {
    v->uncomment();
    return true;
  }
  else if ( cmd == "set-indent-mode" )
  {
    bool ok(false);
    int val ( args.first().toInt( &ok ) );
    if ( ok )
    {
      if ( val < 0 )
        KCC_ERR( i18n("Mode must be at least 0.") );
      v->doc()->config()->setIndentationMode( val );
    }
    else
      v->doc()->config()->setIndentationMode( KateAutoIndent::modeNumber( args.first() ) );
    return true;
  }

  // ALL commands that takes exactly one integer argument.
  else if ( cmd == "set-tab-width" ||
            cmd == "set-indent-width" ||
            cmd == "set-word-wrap-column" ||
            cmd == "goto" )
  {
    // find a integer value > 0
    if ( ! args.count() )
      KCC_ERR( i18n("Missing argument. Usage: %1 <value>").arg( cmd ) );
    bool ok;
    int val ( args.first().toInt( &ok ) );
    if ( !ok )
      KCC_ERR( i18n("Failed to convert argument '%1' to integer.")
                .arg( args.first() ) );

    if ( cmd == "set-tab-width" )
    {
      if ( val < 1 )
        KCC_ERR( i18n("Width must be at least 1.") );
      v->setTabWidth( val );
    }
    else if ( cmd == "set-indent-width" )
    {
      if ( val < 1 )
        KCC_ERR( i18n("Width must be at least 1.") );
      v->doc()->config()->setIndentationWidth( val );
    }
    else if ( cmd == "set-word-wrap-column" )
    {
      if ( val < 2 )
        KCC_ERR( i18n("Column must be at least 1.") );
      v->doc()->setWordWrapAt( val );
    }
    else if ( cmd == "goto" )
    {
      if ( val < 1 )
        KCC_ERR( i18n("Line must be at least 1") );
      if ( (uint)val > v->doc()->numLines() )
        KCC_ERR( i18n("There is not that many lines in this document") );
      v->gotoLineNumber( val - 1 );
    }
    return true;
  }

  // ALL commands that takes 1 boolean argument.
  else if ( cmd == "set-icon-border" ||
            cmd == "set-folding-markers" ||
            cmd == "set-line-numbers" ||
            cmd == "set-replace-tabs" ||
            cmd == "set-remove-trailing-space" ||
            cmd == "set-show-tabs" ||
            cmd == "set-indent-spaces" ||
            cmd == "set-auto-indent" ||
            cmd == "set-word-wrap" ||
            cmd == "set-replace-tabs-save" ||
            cmd == "set-remove-trailing-space-save" )
  {
    if ( ! args.count() )
      KCC_ERR( i18n("Usage: %1 on|off|1|0|true|false").arg( cmd ) );
    bool enable;
    if ( getBoolArg( args.first(), &enable ) )
    {
      if ( cmd == "set-icon-border" )
        v->setIconBorder( enable );
      else if (cmd == "set-folding-markers")
        v->setFoldingMarkersOn( enable );
      else if ( cmd == "set-line-numbers" )
        v->setLineNumbersOn( enable );
      else if ( cmd == "set-replace-tabs" )
        setDocFlag( KateDocumentConfig::cfReplaceTabsDyn, enable, v->doc() );
      else if ( cmd == "set-remove-trailing-space" )
        setDocFlag( KateDocumentConfig::cfRemoveTrailingDyn, enable, v->doc() );
      else if ( cmd == "set-show-tabs" )
        setDocFlag( KateDocumentConfig::cfShowTabs, enable, v->doc() );
      else if ( cmd == "set-indent-spaces" )
        setDocFlag( KateDocumentConfig::cfSpaceIndent, enable, v->doc() );
      else if ( cmd == "set-auto-indent" )
        setDocFlag( KateDocumentConfig::cfAutoIndent, enable, v->doc() );
      else if ( cmd == "set-word-wrap" )
        v->doc()->setWordWrap( enable );
      else if ( cmd == "set-replace-tabs-save" )
        setDocFlag( KateDocumentConfig::cfReplaceTabs, enable, v->doc() );
      else if ( cmd == "set-remove-trailing-space-save" )
        setDocFlag( KateDocumentConfig::cfRemoveSpaces, enable, v->doc() );

      return true;
    }
    else
      KCC_ERR( i18n("Bad argument '%1'. Usage: %2 on|off|1|0|true|false")
               .arg( args.first() ).arg( cmd ) );
  }

  // unlikely..
  KCC_ERR( i18n("Unknown command '%1'").arg(cmd) );
}

static void replace(QString &s, const QString &needle, const QString &with)
{
  int pos=0;
  while (1)
  {
    pos=s.find(needle, pos);
    if (pos==-1) break;
    s.replace(pos, needle.length(), with);
    pos+=with.length();
  }

}

static int backslashString(const QString &haystack, const QString &needle, int index)
{
  int len=haystack.length();
  int searchlen=needle.length();
  bool evenCount=true;
  while (index<len)
  {
    if (haystack[index]=='\\')
    {
      evenCount=!evenCount;
    }
    else
    {  // isn't a slash
      if (!evenCount)
      {
        if (haystack.mid(index, searchlen)==needle)
          return index-1;
      }
      evenCount=true;
    }
    index++;

  }

  return -1;
}

// exchange "\t" for the actual tab character, for example
static void exchangeAbbrevs(QString &str)
{
  // the format is (findreplace)*[nullzero]
  const char *magic="a\x07t\t";

  while (*magic)
  {
    int index=0;
    char replace=magic[1];
    while ((index=backslashString(str, QChar(*magic), index))!=-1)
    {
      str.replace(index, 2, QChar(replace));
      index++;
    }
    magic++;
    magic++;
  }
}

int KateCommands::SedReplace::sedMagic(QString &textLine, const QString &find, const QString &repOld, const QString &delim, bool noCase, bool repeat)
{

  QRegExp matcher(find, noCase);

  int start=0;
  int matches = 0;

  while (start!=-1)
  {
    start=matcher.search(textLine, start);

    if (start==-1) break;

    matches++;

    int length=matcher.matchedLength();


    QString rep=repOld;

    // now set the backreferences in the replacement
    QStringList backrefs=matcher.capturedTexts();
    int refnum=1;

    QStringList::Iterator i = backrefs.begin();
    ++i;

    for (; i!=backrefs.end(); ++i)
    {
      // I need to match "\\" or "", but not "\"
      QString number=QString::number(refnum);

      int index=0;
      while (index!=-1)
      {
        index=backslashString(rep, number, index);
        if (index>=0)
        {
          rep.replace(index, 2, *i);
          index+=(*i).length();
        }
      }

      refnum++;
    }

    replace(rep, "\\\\", "\\");
    replace(rep, "\\" + delim, delim);

    textLine.replace(start, length, rep);
    if (!repeat) break;
    start+=rep.length();
  }

  return matches;
}

static void setLineText(Kate::View *view, int line, const QString &text)
{
  int l = view->getDoc()->lineLength( (uint)line );
  view->getDoc()->removeText( line, 0, line, l );
  view->getDoc()->insertText( line, 0, text );
}

bool KateCommands::SedReplace::exec (Kate::View *view, const QString &cmd, QString &msg)
{
  kdDebug(13030)<<"SedReplace::execCmd()"<<endl;

  QRegExp delim("^[$%]?s ([^\\w\\s])");
  if ( delim.search( cmd ) < 0 ) return false;

  bool fullFile=cmd[0]=='%';
  bool noCase=cmd[cmd.length()-1]=='i' || cmd[cmd.length()-2]=='i';
  bool repeat=cmd[cmd.length()-1]=='g' || cmd[cmd.length()-2]=='g';
  bool onlySelect=cmd[0]=='$';

  QString d = delim.cap(1);
  kdDebug(13030)<<"got delim '"<<d<<"'"<<endl;

  QRegExp splitter( QString("^[$%]?s ")  + d + "((?:[^\\\\\\" + d + "]|\\\\.)*)\\" + d +"((?:[^\\\\\\" + d + "]|\\\\.)*)\\" + d + "[ig]{0,2}$" );
  if (splitter.search(cmd)<0) return false;

  QString find=splitter.cap(1);
  kdDebug(13030)<< "SedReplace: find=" << find.latin1() <<endl;

  QString replace=splitter.cap(2);
  exchangeAbbrevs(replace);
  kdDebug(13030)<< "SedReplace: replace=" << replace.latin1() <<endl;

  KateDocument *doc = ((KateView*)view)->doc();
  if ( ! doc ) return false;

  doc->editStart();

  int res = 0;

  if (fullFile)
  {
    int numLines=doc->numLines();
    for (int line=0; line < numLines; line++)
    {
      int n;
      QString text=doc->textLine(line);
      if ( ( n = sedMagic(text, find, replace, d, !noCase, repeat) ) )
      {
        setLineText(view, line, text);
        res += n;
      }
    }
  }
  else if (onlySelect)
  {
    // Not implemented
  }
  else
  { // just this line
    QString textLine=view->currentTextLine();
    int line=view->cursorLine();
    if ( ( res += sedMagic(textLine, find, replace, d, !noCase, repeat) ) )
    {
      setLineText(view, line, textLine);
    }
  }

  msg = i18n("%1 replacements done").arg( res );

  doc->editEnd();

  return true;
}

bool KateCommands::Character::exec (Kate::View *view, const QString &_cmd, QString &)
{
  QString cmd = _cmd;

  // hex, octal, base 9+1
  QRegExp num("^char *(0?x[0-9A-Fa-f]{1,4}|0[0-7]{1,6}|[0-9]{1,3})$");
  if (num.search(cmd)==-1) return false;

  cmd=num.cap(1);

  // identify the base

  unsigned short int number=0;
  int base=10;
  if (cmd[0]=='x' || cmd.left(2)=="0x")
  {
    cmd.replace(QRegExp("^0?x"), "");
    base=16;
  }
  else if (cmd[0]=='0')
    base=8;
  bool ok;
  number=cmd.toUShort(&ok, base);
  if (!ok || number==0) return false;
  if (number<=255)
  {
    char buf[2];
    buf[0]=(char)number;
    buf[1]=0;
    view->insertText(QString(buf));
  }
  else
  { // do the unicode thing
    QChar c(number);
    view->insertText(QString(&c, 1));
  }

  return true;
}

bool KateCommands::Date::exec (Kate::View *view, const QString &cmd, QString &)
{
  if (cmd.left(4) != "date")
    return false;

  if (QDateTime::currentDateTime().toString(cmd.mid(5, cmd.length()-5)).length() > 0)
    view->insertText(QDateTime::currentDateTime().toString(cmd.mid(5, cmd.length()-5)));
  else
    view->insertText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

  return true;
}

// kate: space-indent on; indent-width 2; replace-tabs on;
