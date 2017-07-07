/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_test_client.cc
 * ---------------------------------------------------------------------------
 * Implements a test client program to test the contacts server.
 * ---------------------------------------------------------------------------
 * Performance test data:
 *  Command line:
 *  bazel-bin/stream_service/contacts_server/contacts_test_client --logtostderr --test_case=performance
 * ---------------------------------------------------------------------------
 * Wed, May 25, 2016 - on Docker on Mac OSX.
 *   -  Build 100k level-db database ~ 350 s
 *   -  Combine New/Upload/Query over 100k database - 17ms (i.e. qps=58).
 */
#include "stream_service/contacts_server/contacts_service.grpc.pb.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>

// for grpc server builder
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

#include <google/protobuf/text_format.h>

// Contacts service implementation
#include "contacts_service_impl.h"
// For thread
#include <sys/prctl.h>

//#include "flag.h"
#include "leveldb/db.h"

// For getTimeMS()
#include "stream_service/orbit/base/timeutil.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

DEFINE_int32(port, 20000, "Listening port of RPC service");
DEFINE_string(test_case, "test", "Value could be test, performance, query");
DECLARE_string(db_name);
DECLARE_string(phone_db_name);
DECLARE_string(index_db_name);

using orangelab::ContactsService;
using namespace std;
using namespace orangelab;

namespace examples {

  void CreateNewUser(ContactsService::Stub* stub,
                     const string& uid,
                     const string& name,
                     const string& phone_key) {
    orangelab::CreateNewUserRequest request;
    orangelab::CreateNewUserResponse response;
    request.set_uid(uid);
    orangelab::ContactInfo* user_info = request.mutable_user_info();
    user_info->set_name(name);
    user_info->set_phone_key(phone_key);
    ClientContext context;
    
    Status status = stub->CreateNewUser(&context, request, &response);
    if (status.ok()) {
      std::string str;
      google::protobuf::TextFormat::PrintToString(response, &str);
      //LOG(INFO) << "response=" << str;
      VLOG(3) << "response=" << str;
    } else {
      LOG(INFO) << "RPC failed.";
    }
  }

  void GetUserContacts(ContactsService::Stub* stub,
                       const string& uid) {
    orangelab::GetUserContactsRequest request;
    orangelab::GetUserContactsResponse response;
    request.set_uid(uid);
    ClientContext context;    
    Status status = stub->GetUserContacts(&context, request, &response);
    if (status.ok()) {
      std::string str;
      google::protobuf::TextFormat::PrintToString(response, &str);
      LOG(INFO) << "response=" << str;
    } else {
      LOG(INFO) << "RPC failed.";
    }
  }

  void UploadContacts(ContactsService::Stub* stub,
                      const string& uid,
                      const ContactInfoList& contact_info_list) {
    orangelab::UploadContactsRequest request;
    orangelab::UploadContactsResponse response;
    request.set_uid(uid);
    request.mutable_contacts()->CopyFrom(contact_info_list);
    ClientContext context;    
    Status status = stub->UploadContacts(&context, request, &response);
    if (status.ok()) {
      std::string str;
      google::protobuf::TextFormat::PrintToString(response, &str);
      //LOG(INFO) << "response=" << str;
      VLOG(3) << "response=" << str;
    } else {
      LOG(INFO) << "RPC failed.";
    }
  }

  void SetupContactList1(ContactInfoList* contact_info_list) {
    ContactInfo* info;
    info = contact_info_list->add_contacts();
    info->set_phone_key("18511069719");
    info->set_name("zhihan");

    info = contact_info_list->add_contacts();
    info->set_phone_key("13682357042");
    info->set_name("daxing");

    info = contact_info_list->add_contacts();
    info->set_phone_key("13816766434");
    info->set_name("majia");

    info = contact_info_list->add_contacts();
    info->set_phone_key("18962657032");
    info->set_name("helen");    
  }

  void SetupContactList2(ContactInfoList* contact_info_list) {
    ContactInfo* info;
    info = contact_info_list->add_contacts();
    info->set_phone_key("18911860053");
    info->set_name("xucheng");

    info = contact_info_list->add_contacts();
    info->set_phone_key("18912345678");
    info->set_name("zhangchi");    

    info = contact_info_list->add_contacts();
    info->set_phone_key("18962657032");
    info->set_name("helen");
  }

  static bool allow_client_run = false;  
  static bool quit = false;

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

  void gen_random_number(char* s, const int len) {
    static const char digit[] = "0123456789";
    for (int i = 0; i < len; ++i) {
      s[i] = digit[rand() % (sizeof(digit) - 1)];
    }    
    s[len] = 0;    
  }

  void GenerateRandomContactList(ContactInfoList* contact_info_list, int number) {
    ContactInfo* info;
    for (int i = 0; i < number; ++i) {
      char phone_last[8];
      gen_random_number(&phone_last[0], 8);
      string phone_number = "1";
      phone_number += phone_last;

      char user_name[20];
      gen_random(&user_name[0], 20);

      info = contact_info_list->add_contacts();
      info->set_phone_key(phone_number);
      info->set_name(user_name);
    }
  }
    
  static void RunPerformanceTest() {
    // Simulate creating 100k users
    // Simulate uploading 20k users's contact list. Each with 100-300 contact items.
    std::shared_ptr<Channel>
      channel(grpc::CreateChannel("localhost:20000",
                                  grpc::InsecureCredentials()));
    srand(time(NULL));
    long start = orbit::getTimeMS();
    std::unique_ptr<ContactsService::Stub> stub(ContactsService::NewStub(channel));

    for (int i = 0; i < 100000; ++i) {
      char user_id[12];
      gen_random(&user_id[0], 12);

      char user_name[20];
      gen_random(&user_name[0], 20);
      
      char phone_last[8];
      gen_random_number(&phone_last[0], 8);
      string phone_number = "1";
      phone_number += phone_last;
      CreateNewUser(stub.get(), user_id, user_name, phone_number);
      
      if (i % 5 == 0) {
        ContactInfoList contact_info_list;
        int n = 100 + rand() % 200;
        GenerateRandomContactList(&contact_info_list, n);
        LOG(INFO) << "i=" << i << " n=" << n;
        UploadContacts(stub.get(), user_id, contact_info_list);
      }
    }
    LOG(INFO) << "running takes " << orbit::getTimeMS() - start << " ms";
    start = orbit::getTimeMS();
    for (int i = 0; i < 1000; ++i) {
      char user_id[12];
      gen_random(&user_id[0], 12);

      char user_name[20];
      gen_random(&user_name[0], 20);
      
      char phone_last[8];
      gen_random_number(&phone_last[0], 8);
      string phone_number = "1";
      phone_number += phone_last;
      CreateNewUser(stub.get(), user_id, user_name, phone_number);
      
      ContactInfoList contact_info_list;
      int n = 100 + rand() % 200;
      GenerateRandomContactList(&contact_info_list, n);
      UploadContacts(stub.get(), user_id, contact_info_list);

      orangelab::GetUserContactsRequest request;
      orangelab::GetUserContactsResponse response;
      request.set_uid(user_id);
      ClientContext context;    
      Status status = stub->GetUserContacts(&context, request, &response);
      assert(status.ok());
      assert(response.user_contacts().contacts_size() == n);
    }
    LOG(INFO) << "running takes " << orbit::getTimeMS() - start << " ms";
  }
  
  static void RunClient() {
    std::shared_ptr<Channel>
      channel(grpc::CreateChannel("localhost:20000",
                                  grpc::InsecureCredentials()));
    std::unique_ptr<ContactsService::Stub> stub(ContactsService::NewStub(channel));
    CreateNewUser(stub.get(), "AB123456", "xucheng", "18911860053");

    ContactInfoList contact_info_list;
    SetupContactList1(&contact_info_list);
  
    UploadContacts(stub.get(), "AB123456", contact_info_list);
    GetUserContacts(stub.get(), "AB123456");
    
    CreateNewUser(stub.get(), "CD1234567", "helen", "18962657032");
    CreateNewUser(stub.get(), "XC7654321", "majia", "13816766434");

    ContactInfoList contact_info_list2;
    SetupContactList2(&contact_info_list2);
    UploadContacts(stub.get(), "XC7654321", contact_info_list2);

    CreateNewUser(stub.get(), "12346412ABCCC", "zhangchi", "18912345678");
  }

  void DestroyDB() {
    leveldb::Status status;
    leveldb::Options options;
    status = leveldb::DestroyDB(FLAGS_db_name, options);
    assert(status.ok());
    status = leveldb::DestroyDB(FLAGS_phone_db_name, options);
    assert(status.ok());
    status = leveldb::DestroyDB(FLAGS_index_db_name, options);
    assert(status.ok());
  }

  static void SetupServer() {
    std::stringstream server_address;
    server_address << "0.0.0.0:" << FLAGS_port;
    orangelab::ContactsServiceImpl service;

    LOG(INFO) << "Server listening on " << server_address.str();
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address.str(),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    allow_client_run = true;
    //server->Wait();
    while (!quit) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}  // namespace examples

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();

  LOG(INFO) << "Setup a server for testing.";
  std::thread t1([&] () { 
      examples::SetupServer();
    });

  LOG(INFO) << "Test the server using the gRPC.";
  std::thread t2([&] () { 
      while (!examples::allow_client_run) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      if (FLAGS_test_case == "test") {
        examples::RunClient();
      } else if (FLAGS_test_case == "performance") {
        examples::RunPerformanceTest();
      }
    });
  t2.join();

  examples::quit = true;
  t1.join();
  LOG(INFO) << "Destroy the test DB.";
  examples::DestroyDB();

  grpc_shutdown();
  return 0;
}
