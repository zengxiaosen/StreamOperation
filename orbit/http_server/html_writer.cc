// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)

#include "html_writer.h"

#include <set>
#include <map>
#include <boost/regex.hpp>

namespace {
static const char* kHtmlBegin = "<html><meta http-equiv=\"content-type\" "
    "content=\"text/html; charset=utf-8\"><body>";
static const char* kHtmlEnding = "</body></html>";
}

namespace orbit {

string HTMLTableWriter::Finalize() {
  string html;
  if (!title_.empty()) {
    html += StringPrintf("<div class='container'><div class='tabletitle'>%s</div>"
			 "<div class='tablestate'>%s</div></div>",
			 title_.c_str(), statement_.c_str());
  }
  html += "<table";
  if (!class_.empty()) {
    html += " class=";
    html += class_;
  }
  if (!border_.empty() || has_border_) {
    if (has_border_) {
      html += " border";
    } else {
      html += " border=";
      html += border_;
    }
  }
  if (!height_.empty()) {
    html += " height=";
    html += height_;
  }
  if (!width_.empty()) {
    html += " width=";
    html += width_;
  }
  if (!cellspacing_.empty()) {
    html += " cellspacing=";
    html += cellspacing_;
  }
  html += ">\n";
  if (has_columns_) {
    html += "<tr>";
    for (int i = 0; i < columns.size(); ++i) {
      html += StringPrintf("<th>%s</th>", columns[i].c_str());
    }
    html += "</tr>";
  }
  if (virtual_columns_ != -1) {
    for (int i = 0; i < rows.size(); ++i) {
      html += "<tr>";
      for (int j = 0; j < virtual_columns_; ++j) {
	html += StringPrintf("<td>%s</td>", rows[i][j].c_str());
      }
      html += "</tr>";
    }
  }
  html += "</table>";
  return html;
}

void HTMLTableWriter::AddColumn(const string& column) {
  has_columns_ = true;
  columns.push_back(column);
}

void HTMLTableWriter::AddRow(const Row& row) {
  if (has_columns_) { 
    if (row.size() != columns.size()) {
      LOG(ERROR) << "Rows size is not equal to columns size";
      return;
    }
  } else if (virtual_columns_ != -1) {
    if (row.size() != virtual_columns_) {
      LOG(ERROR) << "Row size is not consistent with previous rows.";
      return;
    }
  }
  rows.push_back(row);
  if (virtual_columns_ == -1)
    virtual_columns_ = row.size();
}

int HTMLTableWriter::GetRowsLength() {
  return rows.size();
}

void HTMLTableWriter::SetCell(int row_number,
			      int cell_number,
			      const string& cell_value) {
  if (row_number > rows.size()) {
    LOG(ERROR) << "Insert to a invalid row.";
    return;
  }
  if (cell_number > columns.size()) {
    LOG(ERROR) << "Insert to a invalid column.";
    return;
  }
  rows[row_number][cell_number] = cell_value;
}

void SimpleHTMLWriter::AddHTMLSection(const string& html) {
  html_body_ += html;
}
string SimpleHTMLWriter::Finalize() {
  return kHtmlBegin + html_body_ + kHtmlEnding;
}

TemplateHTMLWriter::TemplateHTMLWriter() {
}

TemplateHTMLWriter::~TemplateHTMLWriter() {
  // Release the memory in sections_replace_
  map<string, vector<TemplateHTMLWriter*>* >::const_iterator
    sec_iter = sections_replace_.begin();
  for (; sec_iter != sections_replace_.end(); ++sec_iter) {
    vector<TemplateHTMLWriter*>::const_iterator temp_iter = (sec_iter->second)->begin();
    for (; temp_iter != (sec_iter->second)->end(); ++temp_iter) {
      delete (*temp_iter);
    }
    delete sec_iter->second;
  }
}

bool TemplateHTMLWriter::LoadTemplateFile(const string& html_template_file) {
  string path = GetFilePath(html_template_file);
  if (!path.empty()) {
    SetTemplateDirectory(path);
  }
  SimpleFileLineReader reader(html_template_file);
  if (reader.ReadFile(&html_template_)) {
    AddIncludeTemplates();
    FindAllSections();
    return true;
  }
  return false;
}
void TemplateHTMLWriter::LoadTemplate(const string& html_template) {
  html_template_ = html_template;
  AddIncludeTemplates();
  FindAllSections();
}

TemplateHTMLWriter* TemplateHTMLWriter::AddSection(const string& section_name) {
  if (sections_replace_.find(section_name) ==
      sections_replace_.end()) {
    return NULL;
  }
  string section_html = GetSection(section_name);
  if (section_html.empty())
    return NULL;
  TemplateHTMLWriter* temp = new TemplateHTMLWriter;
  temp->LoadTemplate(section_html);
  sections_replace_[section_name]->push_back(temp);
  return temp;
}

void TemplateHTMLWriter::SetValue(const string& tag, const string& value) {
  string tag_name = StringPrintf("{{%s}}", tag.c_str());
  html_template_ = StringReplace(html_template_, tag_name, value, true);
}

string TemplateHTMLWriter::Finalize() {
  // Iterate all the sections and output the replaced sections.
  map<string, vector<TemplateHTMLWriter*>* >::const_iterator sec_iter =
    sections_replace_.begin();
  for (; sec_iter != sections_replace_.end(); ++sec_iter) {
    string replaced;
    string section_name = sec_iter->first;
    vector<TemplateHTMLWriter*>* new_sections = sec_iter->second;
    for (int i = 0; i < new_sections->size(); ++i) {
      //replaced += (*new_sections)[i];
      replaced += ((*new_sections)[i])->Finalize();
    }

    string section_html = GetSection(section_name);
    if (section_html.empty()) {
      LOG(ERROR) << "Error in finding the section<" << section_name << ">";
      continue;
    }
    string old_section = StringPrintf("{{#%s}}%s{{/%s}}", section_name.c_str(),
				      section_html.c_str(), section_name.c_str());

    html_template_ = StringReplace(html_template_, old_section, replaced, true);
  }

  // Clear all the remaining tags.
  set<string>::const_iterator iter = tags_set_.begin();
  for (; iter != tags_set_.end(); ++iter) {
    SetValue(*iter, "");
  }
  return html_template_;
}

//@private
string TemplateHTMLWriter::GetSection(const string& tag) {
  boost::regex tag_re("\\{\\{#" + tag + "\\}\\}(.*?)\\{\\{/" + tag + "\\}\\}");
  string::const_iterator begin = html_template_.begin();
  string::const_iterator end = html_template_.end();
  boost::match_results<std::string::const_iterator> what;
  boost::match_flag_type flags = boost::match_default;
  while (boost::regex_search(begin, end, what, tag_re, flags)) {
    string content = string(what[1].first, what[1].second);
    return content;
  }
  return "";
}

//@private
void TemplateHTMLWriter::FindAllSections() {
  boost::regex tag_re("\\{\\{(.*?)\\}\\}");
  string::const_iterator begin = html_template_.begin();
  string::const_iterator end = html_template_.end();
  boost::match_results<std::string::const_iterator> what;
  boost::match_flag_type flags = boost::match_default;

  while (boost::regex_search(begin, end, what, tag_re, flags)) {
    string tag = string(what[1].first, what[1].second);
    string section_tag = string(what[1].first + 1, what[1].second);
    string tag_upper = tag;
    UpperString(&tag_upper);
    if (tag == tag_upper) {
      if (HasPrefixString(tag, "#")) {
        sections_replace_[section_tag] = new vector<TemplateHTMLWriter*>();
      } else {
        tags_set_.insert(tag);
      }
    }
    begin = what[0].second; 
    flags |= boost::match_prev_avail; 
    flags |= boost::match_not_bob; 
  }
}

  void TemplateHTMLWriter::AddIncludeTemplates() {
    string template_dir = template_dir_;
    string html_tpl = html_template_;

    boost::regex tag_re("\\{\\{>INCLUDE_TEMPLATE<(.*?)\\}\\}");
    string::const_iterator begin = html_tpl.begin();
    string::const_iterator end = html_tpl.end();
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    
    while (boost::regex_search(begin, end, what, tag_re, flags)) {
      string tag = string(what[1].first, what[1].second);

      if (tag.length() > 0) {
        string template_file = template_dir + tag;
        SimpleFileLineReader reader(template_file);
        string html_template_replaced = "";
        if (reader.ReadFile(&html_template_replaced)) {
          string old_section = StringPrintf("{{>INCLUDE_TEMPLATE<%s}}", tag.c_str());
          html_template_ = StringReplace(html_template_, old_section, html_template_replaced, true);
        } else {
          string old_section = "";
          html_template_ = StringReplace(html_template_, old_section, html_template_replaced, true);
        }
      }

      begin = what[0].second;
      flags |= boost::match_prev_avail;
      flags |= boost::match_not_bob;
    }
  }

}  // namespace orbit
