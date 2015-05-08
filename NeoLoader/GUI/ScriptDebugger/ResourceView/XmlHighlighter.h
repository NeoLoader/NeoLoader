/* $URL$
   $Rev$
   $Author$
   $Date$
   $Id$
 */

/*
** Copyright 2009-10 Martin Holmes, Meagan Timney and the
** University of Victoria Humanities Computing and Media
** Centre.

** This file is part of the projXMLEditor project which in
** turn belongs to the Image Markup Tool version 2.0
** project. The purpose of svgIconsTest is to provide a
** platform to test and learn various features of Qt, and
** to provide a semi-useful tool to aid in the rapid
** creation and editing of resource files containing SVG
** icons for Qt application development.

** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.

** You may also use this code under the Mozilla Public Licence
** version 1.1. MPL 1.1 can be found at http://www.mozilla.org/MPL/MPL-1.1.html.

** "svgIconsTest" is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU Lesser General Public License for more details.
*/

#ifndef XMLHIGHLIGHTER_H
#define XMLHIGHLIGHTER_H

#include <QSyntaxHighlighter>

#include <QHash>
#include <QTextCharFormat>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

class XmlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    XmlHighlighter(QTextDocument *parent = 0);

protected:
    void highlightBlock(const QString &text);
    void highlightSubBlock(const QString &text, const int startIndex, const int currState);

private:
    struct HighlightingRule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> hlRules;

    QRegExp xmlProcInstStartExpression;
    QRegExp xmlProcInstEndExpression;
    QRegExp xmlCommentStartExpression;
    QRegExp xmlCommentEndExpression;
    QRegExp xmlDoctypeStartExpression;
    QRegExp xmlDoctypeEndExpression;

    QRegExp xmlOpenTagStartExpression;
    QRegExp xmlOpenTagEndExpression;
    QRegExp xmlCloseTagStartExpression;
    QRegExp xmlCloseTagEndExpression;
    QRegExp xmlAttributeStartExpression;
    QRegExp xmlAttributeEndExpression;
    QRegExp xmlAttValStartExpression;
    QRegExp xmlAttValEndExpression;

    QRegExp xmlAttValExpression;


    QTextCharFormat xmlProcInstFormat;
    QTextCharFormat xmlDoctypeFormat;
    QTextCharFormat xmlCommentFormat;
    QTextCharFormat xmlTagFormat;
    QTextCharFormat xmlEntityFormat;
    QTextCharFormat xmlAttributeFormat;
    QTextCharFormat xmlAttValFormat;

//Enumeration for types of element, used for tracking what
//we're inside while highlighting over multiline blocks.
    enum xmlState {
                      inNothing,     //Don't know if we'll need this or not.
                      inProcInst,   //starting with <? and ending with ?>
                      inDoctypeDecl, //starting with <!DOCTYPE and ending with >
                      inOpenTag,     //starting with < + xmlName and ending with /?>
                      inOpenTagName, //after < and before whitespace. Implies inOpenTag.
                      inAttribute,   //if inOpenTag, starting with /s*xmlName/s*=/s*" and ending with ".
                      inAttName,     //after < + xmlName + whitespace, and before =". Implies inOpenTag.
                      inAttVal,      //after =" and before ". May also use single quotes. Implies inOpenTag.
                      inCloseTag,    //starting with </ and ending with >.
                      inCloseTagName,//after </ and before >. Implies inCloseTag.
                      inComment      //starting with <!-- and ending with -->. Overrides all others.
                     };
};


#endif
