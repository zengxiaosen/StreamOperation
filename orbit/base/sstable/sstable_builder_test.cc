// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for sstable_builder

#include "gtest/gtest.h"

#include "stream_service/orbit/base/example_test.pb.h"
#include "stream_service/orbit/base/file.h"
#include "stream_service/orbit/base/timeutil.h"
#include "sstable_builder.h"

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

namespace orbit {
namespace {

class SSTableBuilderTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }
  virtual void TearDown() override {
  }
};

// Build a simple sstable.
TEST_F(SSTableBuilderTest, BuildASSTable) {
  string key = "xucheng";
  string value = "110";
  string key2 = "zhihan";
  string value2 = "120";

  string sstable_file = "abc.sst";
  sstable::SimpleSSTableBuilder builder(sstable_file);
  builder.Add(key, value);
  builder.Add(key2, value2);
  builder.Build();

  EXPECT_TRUE(File::Exists(sstable_file.c_str()));

  sstable::SimpleSSTable table;
  EXPECT_TRUE(table.Open(sstable_file));
  vector<string> pp = table.Lookup(key2);
  
  vector<string> values = table.Lookup(key);
  
  EXPECT_EQ(1, values.size());

  EXPECT_EQ(value, values[0]);

  values = table.Lookup(key2);
  
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(value2, values[0]);

  // Lookup failed for non-existent key.
  values = table.Lookup("daxing");
  EXPECT_EQ(0, values.size());
  
  EXPECT_TRUE(File::Delete(sstable_file));
  EXPECT_FALSE(File::Exists(sstable_file.c_str()));
}  
  
// Build a simple sstable.
TEST_F(SSTableBuilderTest, BuildASSTableUsingUncompressedMode) {
  string key = "xucheng";
  string value = "110";
  string key2 = "zhihan";
  string value2 = "120";

  string sstable_file = "abc.sst";
  sstable::SimpleSSTableBuilder builder(sstable_file);
  builder.set_use_compression(false);
  builder.Add(key, value);
  builder.Add(key2, value2);
  builder.Build();

  EXPECT_TRUE(File::Exists(sstable_file.c_str()));

  sstable::SimpleSSTable table;
  table.set_use_compression(false);
  EXPECT_TRUE(table.Open(sstable_file));
  vector<string> pp = table.Lookup(key2);
  
  vector<string> values = table.Lookup(key);
  
  EXPECT_EQ(1, values.size());

  EXPECT_EQ(value, values[0]);

  values = table.Lookup(key2);
  
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(value2, values[0]);

  // Lookup failed for non-existent key.
  values = table.Lookup("daxing");
  EXPECT_EQ(0, values.size());

  EXPECT_TRUE(File::Delete(sstable_file));
  EXPECT_FALSE(File::Exists(sstable_file.c_str()));
}

// Build a simple sstable with replicated keys.
TEST_F(SSTableBuilderTest, BuildASSTableWithReplicatedKeys) {
  string key = "xucheng";
  string value = "110";
  string key2 = "zhihan";
  string value2 = "120";
  string key3 = "xucheng";
  string value3 = "130";

  string sstable_file = "abc.sst";
  sstable::SimpleSSTableBuilder builder(sstable_file);
  builder.Add(key, value);
  builder.Add(key2, value2);
  builder.Add(key3, value3);
  builder.Build();

  EXPECT_TRUE(File::Exists(sstable_file.c_str()));

  sstable::SimpleSSTable table;
  EXPECT_TRUE(table.Open(sstable_file));
  vector<string> values = table.Lookup(key);
  
  EXPECT_EQ(2, values.size());
  EXPECT_EQ(value, values[0]);
  EXPECT_EQ(value3, values[1]);

  values = table.Lookup(key2);
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(value2, values[0]);

  // Lookup failed for non-existent key.
  values = table.Lookup("daxing");
  EXPECT_EQ(0, values.size());

  EXPECT_TRUE(File::Delete(sstable_file));
  EXPECT_FALSE(File::Exists(sstable_file.c_str()));
}

TEST_F(SSTableBuilderTest, BuildSSTableWithProtocolBuffer) {
  string sstable_file = "abc.sst";

  sstable::SimpleSSTableBuilder builder(sstable_file);

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

  StudentMember another_proto;
  another_proto.set_type(MemberType::STUDENT);
  another_proto.set_name("Richard");
  another_proto.set_department("Art science");
  another_proto.set_age(33);

  score1 = another_proto.add_scores();
  score1->set_course_id(231);
  score1->set_score(50);

  score2 = another_proto.add_scores();
  score2->set_course_id(239);
  score2->set_score(75);

  builder.AddProtocolMessage<StudentMember>(export_proto.name(), export_proto);
  builder.AddProtocolMessage<StudentMember>(another_proto.name(), another_proto);
  builder.Build();  
  
  sstable::SimpleSSTable table;
  EXPECT_TRUE(table.Open(sstable_file));
  vector<StudentMember*> values = table.LookupProtocolMessage<StudentMember>("Richard");
  
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(2, values[0]->scores_size());
  EXPECT_EQ("Art science", values[0]->department());
  EXPECT_EQ(50, values[0]->scores(0).score());
  EXPECT_EQ(75, values[0]->scores(1).score());

  values = table.LookupProtocolMessage<StudentMember>("xucheng");
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(2, values[0]->scores_size());
  EXPECT_EQ("computer science", values[0]->department());
  EXPECT_EQ(90, values[0]->scores(0).score());
  EXPECT_EQ(85, values[0]->scores(1).score());

  // Lookup failed for non-existent key.
  values = table.LookupProtocolMessage<StudentMember>("daxing");
  EXPECT_EQ(0, values.size());
  
  EXPECT_TRUE(File::Delete(sstable_file));
  EXPECT_FALSE(File::Exists(sstable_file.c_str()));
}  

void gen_random(char *s, const int len) {
  static const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  
  for (int i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  
  s[len] = 0;
}

// Build two sstable using compression or non-compression and test the performance.
TEST_F(SSTableBuilderTest, RandomBuildSSTableComparingCompressionRate) {
  string sstable_file = "abc.sst";
  string uncompress_file = "def.sst";

  sstable::SimpleSSTableBuilder builder(sstable_file);
  sstable::SimpleSSTableBuilder builder2(uncompress_file);
  builder2.set_use_compression(false);

  srand (time(NULL));
  vector<string> key_in_table;
  
  for (int i = 0; i < 100000; ++i) {
    char key[6];
    gen_random(&key[0], 6);
    char value[128];
    gen_random(&value[0], 128);
    //LOG(INFO) << "key=" << key << " value=" << value;
    if (rand() % 100 < 5) {
      key_in_table.push_back(key);
    }
    builder.Add(key, value);
    builder2.Add(key, value);
  }
  long start = getTimeMS();
  builder.Build();
  LOG(INFO) << "running takes " << getTimeMS() - start << " ms";

  start = getTimeMS();
  builder2.Build();
  LOG(INFO) << "running takes " << getTimeMS() - start << " ms";

  EXPECT_TRUE(File::Exists(sstable_file.c_str()));
  EXPECT_TRUE(File::Exists(uncompress_file.c_str()));

  File* fp = File::Open(sstable_file.c_str(), "r");
  int size1 = fp->Size();
  LOG(INFO) << size1;
  fp->Close();

  fp = File::Open(uncompress_file.c_str(), "r");
  int size2 = fp->Size();
  LOG(INFO) << size2;
  fp->Close();

  EXPECT_TRUE(size1 < size2);

  LOG(INFO) << "Open Table.";

  start = getTimeMS();
  
  sstable::SimpleSSTable table;
  EXPECT_TRUE(table.Open(sstable_file));
  sstable::SimpleSSTable table2;
  table2.set_use_compression(false);
  EXPECT_TRUE(table2.Open(uncompress_file));

  LOG(INFO) << "Open the file, and it takes " << getTimeMS() - start << " ms";
  
  int hit = 0;
  int total = 10000;
  start = getTimeMS();
  for (int i = 0; i < total; ++i) {
    char key[6];
    gen_random(&key[0], 6);
    vector<string> values = table.Lookup(key);
    vector<string> values2 = table2.Lookup(key);
    if (values.size() > 0) {
      hit++;
      LOG(INFO) << "key=" << " value=" << values[0];
    }
    
    EXPECT_EQ(values.size(), values2.size());
  }

  LOG(INFO) << "Lookup " << total << " times and it takes " << getTimeMS() - start << " ms";
  LOG(INFO) << "hit rate=" << (double)hit/total;

  start = getTimeMS();
  
  for (string key : key_in_table) {
    vector<string> values = table.Lookup(key);
    vector<string> values2 = table2.Lookup(key);
    if (values.size() > 0) {
      hit++;
    }

    EXPECT_EQ(values.size(), values2.size());    
  }
  
  LOG(INFO) << "Lookup " << key_in_table.size() << " times and it takes " << getTimeMS() - start << " ms";
  LOG(INFO) << "hit rate=" << (double)hit/total;

  EXPECT_TRUE(File::Delete(sstable_file));
  EXPECT_FALSE(File::Exists(sstable_file.c_str()));
  EXPECT_TRUE(File::Delete(uncompress_file));
  EXPECT_FALSE(File::Exists(uncompress_file.c_str()));
}
  
}  // annoymouse namespace
}  // namespace orbit
