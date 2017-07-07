// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for html_writer.h/cc

#include "gtest/gtest.h"

#include "html_writer.h"

namespace orbit {
namespace {

class HtmlWriterTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};


// Test html_writer.
TEST_F(HtmlWriterTest, OutputEmptyHtml) {
  TemplateHTMLWriter temp;
  string html = temp.Finalize();
  EXPECT_EQ(0, html.length());
}

TEST_F(HtmlWriterTest, OutputHtml) {
  const string kHtmlTemplate = "<select name=refreshpolicy size=3>{{#POLICY}}<option value=\"{{VALUE}}\" {{SELECT}}>{{NAME}}</option>{{/POLICY}}</select>";
  const string kExpectedHtml = "<select name=refreshpolicy size=3><option value=\"120\" >hello</option></select>";

  TemplateHTMLWriter temp;
  temp.LoadTemplate(kHtmlTemplate);

  TemplateHTMLWriter* section_temp;
  section_temp = temp.AddSection("POLICY");
  section_temp->SetValue("VALUE", "120");
  section_temp->SetValue("NAME", "hello");

  string html = temp.Finalize();
  EXPECT_EQ(kExpectedHtml, html);
}

  
TEST_F(HtmlWriterTest, OutputHtmlWithRemainingTagsRemoved) {
  const string kHtmlTemplate = "<select name=refreshpolicy size=3>{{#POLICY}}<option value=\"{{VALUE}}\" {{SELECT}}>{{NAME}}</option>{{/POLICY}}</select>";
  const string kExpectedHtml = "<select name=refreshpolicy size=3></select>";
  const string kExpectedHtml2 = "<select name=refreshpolicy size=3><option value=\"123\" >xucheng</option></select>";

  TemplateHTMLWriter temp;
  temp.LoadTemplate(kHtmlTemplate);

  // The POLICY and SELECT are not set, so those remaining tags will be
  // removed and will not remain in the output html.
  string html = temp.Finalize();
  EXPECT_EQ(kExpectedHtml, html);

  TemplateHTMLWriter temp2;
  temp2.LoadTemplate(kHtmlTemplate);
  TemplateHTMLWriter* section_temp;
  section_temp = temp2.AddSection("POLICY");
  section_temp->SetValue("VALUE", "123");
  section_temp->SetValue("NAME", "xucheng");
  string html2 = temp2.Finalize();
  EXPECT_EQ(kExpectedHtml2, html2);
}

TEST_F(HtmlWriterTest, OutputRecurringHtmlTemplate) {
  const string kHtmlTemplate = "<select name=refreshpolicy size=3>{{#POLICY}}<option value=\"{{VALUE}}\" {{SELECT}}>{{NAME}}</option>{{/POLICY}}</select>";
  const string kExpectedHtml = "<select name=refreshpolicy size=3><option value=\"120\" >hello</option><option value=\"259\" select>world</option></select>";

  TemplateHTMLWriter temp;
  temp.LoadTemplate(kHtmlTemplate);

  TemplateHTMLWriter* section_temp;
  section_temp = temp.AddSection("POLICY");
  section_temp->SetValue("VALUE", "120");
  section_temp->SetValue("NAME", "hello");

  section_temp = temp.AddSection("POLICY");
  section_temp->SetValue("VALUE", "259");
  section_temp->SetValue("NAME", "world");
  section_temp->SetValue("SELECT", "select");

  string html = temp.Finalize();
  EXPECT_EQ(kExpectedHtml, html);
}

TEST_F(HtmlWriterTest, OutputHtmlTemplateAndIgnoreJsTemplate) {
  const string kHtmlTemplate = "<html>Process started:{{START_TIME}}<div>{{#stream_infos}}<tr><td class=\"text-center\">{{stream_id}}<br/>({{user_name}})</td></tr>{{/stream_infos}}</div></html>";
  const string kExpectedHtml = "<html>Process started:12345<div>{{#stream_infos}}<tr><td class=\"text-center\">{{stream_id}}<br/>({{user_name}})</td></tr>{{/stream_infos}}</div></html>";

  TemplateHTMLWriter temp;
  temp.LoadTemplate(kHtmlTemplate);

  temp.SetValue("START_TIME", "12345");

  string html = temp.Finalize();
  EXPECT_EQ(kExpectedHtml, html);
}

TEST_F(HtmlWriterTest, RecurringTemplateTagWorkingProperly) {
  const string kHtmlTemplate = "<html>Process started:{{START_TIME}}<div><table><tbody>{{#DISPLAY_LINE}}<tr>{{#COLUMN}}<td>{{DATA_INFO}}</td>{{/COLUMN}}</tr>{{/DISPLAY_LINE}}</tbody></table></div></html>";
  const string kExpectedHtml = "<html>Process started:12345<div><table><tbody><tr><td>hello</td></tr><tr><td>world</td></tr></tbody></table></div></html>";

  TemplateHTMLWriter temp;
  temp.LoadTemplate(kHtmlTemplate);

  temp.SetValue("START_TIME", "12345");

  TemplateHTMLWriter* line_temp;
  line_temp = temp.AddSection("DISPLAY_LINE");
  TemplateHTMLWriter* column_temp;
  column_temp = line_temp->AddSection("COLUMN");
  column_temp->SetValue("DATA_INFO", "hello");

  line_temp = temp.AddSection("DISPLAY_LINE");
  column_temp = line_temp->AddSection("COLUMN");
  column_temp->SetValue("DATA_INFO", "world");
  
  string html = temp.Finalize();
  EXPECT_EQ(kExpectedHtml, html);
}

TEST_F(HtmlWriterTest, TestIncludeTemplateTag) {
  const string kHtmlTemplate = "<html>Process started:{{START_TIME}} {{>INCLUDE_TEMPLATE<statusz_include_test.tpl}}  {{#TEST_INFO}}<td>{{DATA_INFO}}</td>{{/TEST_INFO}}</html>";
  const string kExpectedHtml = "<html>Process started:12345 <td class=\"text-right\">test  111</td>\n  </html>";
  const string kExpectedHtml2 = "<html>Process started:12345 <td class=\"text-right\">test  111</td>\n  <td>info_ok</td></html>";
  {
    TemplateHTMLWriter temp;
    string str_template_dir = "stream_service/orbit/http_server/html/";
    temp.SetTemplateDirectory(str_template_dir);
    temp.LoadTemplate(kHtmlTemplate);

    temp.SetValue("START_TIME", "12345");
    temp.SetValue("TEST", "111");
    string html = temp.Finalize();

    EXPECT_EQ(kExpectedHtml, html);
  }
  {
    TemplateHTMLWriter temp;
    string str_template_dir = "stream_service/orbit/http_server/html/";
    temp.SetTemplateDirectory(str_template_dir);
    temp.LoadTemplate(kHtmlTemplate);

    temp.SetValue("START_TIME", "12345");
    temp.SetValue("TEST", "111");

    TemplateHTMLWriter* colum_temp;
    colum_temp = temp.AddSection("TEST_INFO");
    colum_temp->SetValue("DATA_INFO", "info_ok");

    string html2 = temp.Finalize();
    EXPECT_EQ(kExpectedHtml2, html2);
  }
  LOG(INFO) << "here.";
}

}  // namespace annoymous

}  // namespace orbit
