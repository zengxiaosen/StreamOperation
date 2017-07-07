// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for file.h/cc

#include <gtest/gtest.h>

#include "file.h"
#include "stream_service/orbit/base/example_test.pb.h"
#include <google/protobuf/util/message_differencer.h>

using std::string;
using namespace orbit;

TEST(FileTest, WriteProtoToFile) {
  File* file;
  const string export_file = "testtest.pb.txt";
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

  EXPECT_TRUE(file::WriteProtoToASCIIFile(export_proto, export_file));
  EXPECT_TRUE(File::Delete(export_file));
}

TEST(FileTest, ReadProtoFromFile) {
  const string export_file = "stream_service/orbit/base/testdata/test_example.pb.txt";
  StudentMember member;
  EXPECT_TRUE(file::ReadFileToProto(export_file, &member));

  std::string proto_string;
  google::protobuf::TextFormat::PrintToString(member, &proto_string);
  LOG(INFO) << "proto_string=\n" << proto_string;
  EXPECT_EQ("xucheng", member.name());
  EXPECT_EQ(2, member.scores_size());

  EXPECT_EQ(235, member.scores(1).course_id());
  EXPECT_EQ(85, member.scores(1).score());
}

TEST(FileTest, WriteProtoToFileAndReadProtoFromFile) {
  File* file;
  const string export_file = "testtest2.pb.txt";
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

  EXPECT_TRUE(file::WriteProtoToASCIIFile(export_proto, export_file));

  StudentMember member;
  EXPECT_TRUE(file::ReadFileToProto(export_file, &member));

  google::protobuf::util::MessageDifferencer md;
  EXPECT_TRUE(md.Equals(export_proto, member));

  EXPECT_TRUE(File::Delete(export_file));
}

TEST(FileTest, CreateDirectoryAndDelete) {
  // First make sure that the directory is not exist.
  EXPECT_FALSE(File::Exists("/tmp/abc/123"));
  // Create the directory /tmp/abc and /tmp/abc/123, recursively.
  File::RecursivelyCreateDir("/tmp/abc/123");
  // Expect the directory exists now.
  EXPECT_TRUE(File::Exists("/tmp/abc/123"));
  // Now test the recursively delete function.
  // Delete the directory we just created.
  File::RecursivelyDeleteDirOrFile("/tmp/abc/123");
  // Expect the the directory is not exist any more
  EXPECT_FALSE(File::Exists("/tmp/abc/123"));
  // But the parent directory should be there.
  EXPECT_TRUE(File::Exists("/tmp/abc"));
  // Delete the parent directory.
  File::RecursivelyDeleteDirOrFile("/tmp/abc");
  // Expect the parent directory is deleted.
  EXPECT_FALSE(File::Exists("/tmp/abc"));  
}


