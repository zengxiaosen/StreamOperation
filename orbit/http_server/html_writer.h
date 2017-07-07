// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)

#ifndef HTTP_SERVER_HTML_WRITER_H__
#define HTTP_SERVER_HTML_WRITER_H__

#include <string>
#include <set>
#include <map>

#include <fstream>
#include <string>
#include <vector>

#include "stream_service/orbit/base/base.h"
#include "stream_service/orbit/base/strutil.h"
#include "glog/logging.h"

namespace orbit {
using std::string;
using std::set;
using std::map;
using std::vector;

class HTMLTableWriter {
 public:
  typedef vector<string> Columns;
  typedef vector<string> Row;

  HTMLTableWriter() {
    has_border_ = false;
    has_columns_ = false;
    virtual_columns_ = -1;
  }

  void AddColumn(const string& column);
  void AddRow(const Row& row);
  int GetRowsLength();
  void SetCell(int row_number, int cell_number, const string& cell_value);

  string Finalize();

  // Getter/Setter for various table's properties.
  string get_border() {
    return border_;
  }
  void set_border(string border) {
    border_ = border;
  }
  string get_width() {
    return width_;
  }
  void set_width(string width) {
    width_ = width;
  }
  string get_height() {
    return height_;
  }
  void set_height(string height) {
    height_ = height;
  }
  bool has_border() {
    return has_border_;
  }
  void set_has_border() {
    has_border_ = true;
  }
  void set_class(const string& pclass) {
    class_ = pclass;
  }
  string get_class() {
    return class_;
  }
  void set_cellspacing(const string& cellspacing) {
    cellspacing_ = cellspacing;
  }
  string get_cellspacing() {
    return cellspacing_;
  }
  string get_title() {
    return title_;
  }
  void set_title(const string& title) {
    title_ = title;
  }
  string get_statement() {
    return statement_;
  }
  void set_statement(const string& statement) {
    statement_ = statement;
  }

 private:
  bool has_columns_;
  int virtual_columns_;
  Columns columns;
  vector<Row> rows;
  string border_;
  bool has_border_;
  string width_;
  string height_;
  string class_;
  string cellspacing_;
  string title_;
  string statement_;
  DISALLOW_COPY_AND_ASSIGN(HTMLTableWriter);
};

/*
class SimpleHTMLWriter is utility clas to output html page based on each
section given by the client. The function AddHTMLSection() is used to
feed a html fragment to the writer, Finalize() is used to get the output
html page.
 */
class SimpleHTMLWriter {
 public:
  SimpleHTMLWriter() {
  }
  ~SimpleHTMLWriter() {
  }
  void AddHTMLSection(const string& html);
  string Finalize();
 private:
  string html_body_;
  DISALLOW_COPY_AND_ASSIGN(SimpleHTMLWriter);
};

/*
class TemplateHTMLWriter is utility class for a simple C++ html template engine,
used to output html page based on HTML templates.
HTML template is defined as a complete html fragment with some special tags
embeded inside html template. During the runtime, server code usually retrives
data from backend system and then fill in the tag section in template and
output the full html in the end.
A sample fragment of HTML template is:
......
<td class="tdback">Refresh Policy:</td>
  <td>
    <select name=refreshpolicy size=3>
      {{#POLICY}}
        <option value="{{VALUE}}" {{SELECT}}>{{NAME}}
      {{/POLICY}}
    </select>
  </td>
....
Sample usage of the code:
  TemplateHTMLWriter temp;
  temp.LoadTemplateFile(FLAGS_template_file);
  TemplateHTMLWriter* section_temp;
  section_temp = temp.AddSection("POLICY");
  section_temp->SetValue("VALUE", "120");
  section_temp->SetValue("NAME", "hello");
  section_temp = temp.AddSection("POLICY");
  section_temp->SetValue("VALUE", "259");
  section_temp->SetValue("NAME", "world");
  section_temp->SetValue("SELECT", "select");
  temp->SetValue("INTERVAL", "125");
  string html_body = temp.Finalize();
*/
class TemplateHTMLWriter : public SimpleHTMLWriter {
 public:
  TemplateHTMLWriter();
  ~TemplateHTMLWriter();

  void SetTemplateDirectory(const string& template_dir) {
    template_dir_ = template_dir;
  }
  bool LoadTemplateFile(const string& html_template_file);
  void LoadTemplate(const string& html_template);

  TemplateHTMLWriter* AddSection(const string& section_name);

  void SetValue(const string& tag, const string& value);
  string Finalize();

 private:
  string GetSection(const string& tag);
  void FindAllSections();
  void AddIncludeTemplates();

  set<string> tags_set_;
  map<string, vector<TemplateHTMLWriter*>* > sections_replace_;
  string html_template_;
  string template_dir_;
  DISALLOW_COPY_AND_ASSIGN(TemplateHTMLWriter);
};

}  // namespace orbit

#endif  // HTTP_SERVER_HTML_WRITER_H__
