// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for recordio.h/cc

#include <gtest/gtest.h>

#include "recordio.h"
#include "stream_service/orbit/base/example_test.pb.h"

using std::string;
using namespace orbit;

TEST(RecordIOTest, WriteProtoToFileAndReadProtoFromFile) {
  File* file;
  const string export_file = "abc.pb";
  if (!file::Open(export_file, "wb", &file, file::Defaults()).ok()) {
      LOG(WARNING) << "Cannot open " << export_file;
  }

  StudentMember export_proto;
  export_proto.set_type(MemberType::STUDENT);
  export_proto.set_name("xucheng");
  export_proto.set_department("computer science");
  export_proto.set_age(25);

  CourseScore* score1 = export_proto.add_scores();
  score1->set_course_id(123);
  score1->set_score(90);

  CourseScore* score2 = export_proto.add_scores();
  score2->set_course_id(235);
  score2->set_score(85);

  RecordWriter writer(file);
  writer.set_use_compression(true);
  writer.WriteProtocolMessage(export_proto);
  writer.Close();

  File* rfile;
  EXPECT_TRUE(file::Open(export_file, "rb", &rfile, file::Defaults()).ok());

  RecordReader reader(rfile);
  StudentMember member;
  EXPECT_TRUE(reader.ReadProtocolMessage(&member));

  EXPECT_TRUE(reader.Close());

  std::string proto_string;
  google::protobuf::TextFormat::PrintToString(member, &proto_string);
  LOG(INFO) << "proto_string=\n" << proto_string;

  EXPECT_EQ("xucheng", member.name());
  EXPECT_EQ(2, member.scores_size());

  EXPECT_EQ(235, member.scores(1).course_id());
  EXPECT_EQ(85, member.scores(1).score());

  EXPECT_TRUE(File::Delete(export_file));
}
