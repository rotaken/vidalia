/*
**  This file is part of Vidalia, and is subject to the license terms in the
**  LICENSE file, found in the top level directory of this distribution. If you
**  did not receive the LICENSE file with this file, you may obtain it from the
**  Vidalia source package distributed by the Vidalia Project at
**  http://www.vidalia-project.net/. No part of Vidalia, including this file,
**  may be copied, modified, propagated, or distributed except according to the
**  terms described in the LICENSE file.
*/

#include <QHash>
#include <QFile>
#include <QDomDocument>
#include <QTextStream>


#define TS_DOCTYPE                    "TS"
#define TS_ELEMENT_ROOT               "TS"
#define TS_ELEMENT_CONTEXT            "context"
#define TS_ELEMENT_NAME               "name"
#define TS_ELEMENT_MESSAGE            "message"
#define TS_ELEMENT_SOURCE             "source"
#define TS_ELEMENT_TRANSLATION        "translation"
#define TS_ATTR_TRANSLATION_TYPE      "type"
#define TS_ATTR_VERSION               "version"


/** Create a new context element with the name <b>contextName</b>. */
QDomElement
new_context_element(QDomDocument *ts, const QString &contextName)
{
  QDomElement context, name;
 
  /* Create a <name> element */
  name = ts->createElement(TS_ELEMENT_NAME);
  name.appendChild(ts->createTextNode(contextName));

  /* Create a <context> element and add the <name> element as a child */
  context = ts->createElement(TS_ELEMENT_CONTEXT);
  context.appendChild(name);
  return context;
}

/** Create a new message element using the source string <b>msgid</b> and the
 * translation <b>msgstr</b>. */
QDomElement
new_message_element(QDomDocument *ts,
                    const QString &msgid, const QString &msgstr)
{
  QDomElement message, source, translation;

  /* Create and set the <source> element */
  source = ts->createElement(TS_ELEMENT_SOURCE);
  source.appendChild(ts->createTextNode(msgid));

  /* Create and set the <translation> element */
  translation = ts->createElement(TS_ELEMENT_TRANSLATION);
  if (!msgstr.isEmpty())
    translation.appendChild(ts->createTextNode(msgstr));
  else
    translation.setAttribute(TS_ATTR_TRANSLATION_TYPE, "unfinished");

  /* Create a <message> element and add the <source> and <translation>
   * elements as children */
  message = ts->createElement(TS_ELEMENT_MESSAGE);
  message.appendChild(source);
  message.appendChild(translation);

  return message;
}

/** Create a new TS document of the appropriate doctype and with a TS root
 * element. */
QDomDocument
new_ts_document()
{
  QDomDocument ts(TS_DOCTYPE);
  
  QDomElement root = ts.createElement(TS_ELEMENT_ROOT);
  root.setAttribute(TS_ATTR_VERSION, "1.1");
  ts.appendChild(root);

  return ts;
}

/** Parse the context name from <b>str</b>, where the context name is of the
 * form ContextName#Number. This is the format used by translate-toolkit. */
QString
parse_context_name(const QString &str)
{
  if (str.contains("#"))
    return str.section("#", 0, 0);
  return QString();
}

/** Parse the PO-formatted message string from <b>msg</b>. If <b>msg</b> is a
 * multiline string, the extra double quotes will be replaced with newlines
 * appropriately. */
QString
parse_message_string(const QString &msg)
{
  QString out = msg.trimmed(); 
  
  out.replace("\"\n\"", "\n");
  if (out.startsWith("\""))
    out = out.remove(0, 1);
  if (out.endsWith("\""))
    out.chop(1);
  return out;
}

/** Read and return the next non-empty line from <b>stream</b>. */
QString
read_next_line(QTextStream *stream)
{
  stream->skipWhiteSpace();
  return stream->readLine().append("\n");
}

/** Convert <b>po</b> from the PO format to a TS-formatted XML document.
 * <b>ts</b> will be set to the resulting TS document. Return the number of
 * converted strings on success, or -1 on error and <b>errorMessage</b> will
 * be set. */
int
po2ts(QTextStream *po, QDomDocument *ts, QString *errorMessage)
{
  QString line, context, msgid, msgstr;
  QHash<QString,QDomElement> contextElements;
  QDomElement contextElement, msgElement, transElement;
  int n_strings = 0;

  Q_ASSERT(po);
  Q_ASSERT(ts);
  Q_ASSERT(errorMessage);

  *ts = new_ts_document();

  while (!po->atEnd()) {
    /* Parse the context name and ignore other "#" lines. */
    if (!line.startsWith("#: ")) {
      line = read_next_line(po);
      continue;
    }
    context = parse_context_name(line.section(" ", 1));
    while (line.startsWith("#"))
      line = read_next_line(po);

    /* Parse the (possibly multiline) message source string */
    if (!line.startsWith("msgid ")) {
      *errorMessage = "expected 'msgid' line";
      return -1;
    }
    msgid = line.section(" ", 1);
    
    line = read_next_line(po);
    while (line.startsWith("\"")) {
      msgid.append(line);
      line = read_next_line(po);
    }
    msgid = parse_message_string(msgid);

    /* Parse the (possibly multiline) translated string */
    if (!line.startsWith("msgstr ")) {
      *errorMessage = "expected 'msgstr' line";
      return -1;
    }
    msgstr = line.section(" ", 1);
    
    line = read_next_line(po);
    while (line.startsWith("\"")) {
      msgstr.append(line);
      line = read_next_line(po);
    }
    msgstr = parse_message_string(msgstr);

    /* Add the message and translation to the .ts document */
    if (contextElements.contains(context)) {
      contextElement = contextElements.value(context);
    } else {
      contextElement = new_context_element(ts, context);
      ts->documentElement().appendChild(contextElement);
      contextElements.insert(context, contextElement);
    }
    contextElement.appendChild(new_message_element(ts, msgid, msgstr)); 
    
    n_strings++;
  }
  return n_strings;
}

/** Display application usage and exit. */
void
print_usage_and_exit()
{
  QTextStream error(stderr);
  error << "usage: po2ts [-q] -i <infile.po> -o <outfile.ts>\n";
  error << "  -q (optional)   Quiet mode (errors are still displayed)\n";
  error << "  -i <infile.po>  Input .po file\n";
  error << "  -o <outfile.ts> Output .ts file\n";
  error.flush();
  exit(1);
}

int
main(int argc, char *argv[])
{
  QTextStream error(stderr);
  QString errorMessage;
  char *infile, *outfile;
  bool quiet = false;

  /* Check for the correct number of input parameters. */
  if (argc < 5 || argc > 6)
    print_usage_and_exit();
  for (int i = 1; i < argc; i++) {
    QString arg(argv[i]);
    if (!arg.compare("-q", Qt::CaseInsensitive))
      quiet = true;
    else if (!arg.compare("-i", Qt::CaseInsensitive) && ++i < argc)
      infile = argv[i];
    else if (!arg.compare("-o", Qt::CaseInsensitive) && ++i < argc)
      outfile = argv[i];
    else
      print_usage_and_exit(); 
  }

  /* Open the input PO file for reading. */
  QFile poFile(infile);
  if (!poFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    error << QString("Unable to open '%1' for reading: %2\n").arg(infile)
                                                .arg(poFile.errorString());
    return 2;
  }

  QDomDocument ts;
  QTextStream po(&poFile);
  int n_strings = po2ts(&po, &ts, &errorMessage);
  if (n_strings < 0) {
    error << QString("Unable to convert '%1': %2\n").arg(infile)
                                                    .arg(errorMessage);
    return 3;
  }

  /* Open the TS file for writing. */
  QFile tsFile(outfile);
  if (!tsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    error << QString("Unable to open '%1' for writing: %2\n").arg(outfile)
                                                .arg(tsFile.errorString());
    return 4;
  }

  /* Write the .ts output. */
  QTextStream out(&tsFile);
  out.setCodec("UTF-8");
  out << ts.toString(4);

  if (!quiet) {
    QTextStream(stdout) << QString("Converted %1 strings from %2 to %3.\n")
                                                            .arg(n_strings)
                                                            .arg(infile)
                                                            .arg(outfile);
  }
  return 0;
}

