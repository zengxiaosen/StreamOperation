/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_service_impl.h
 * ---------------------------------------------------------------------------
 * Defines the contacts service interface based.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/contacts_server/contacts_service.grpc.pb.h"
#include "contacts_db.h"

namespace orangelab {
  using namespace google::protobuf;

  class ContactsServiceImpl : public ContactsService::Service {
  public:
    ContactsServiceImpl();
    virtual ~ContactsServiceImpl() {
    }
    virtual grpc::Status CreateNewUser(grpc::ServerContext* context,
                                       const CreateNewUserRequest* request,
                                       CreateNewUserResponse* response);
    virtual grpc::Status GetUserContacts(grpc::ServerContext* context,
                                         const GetUserContactsRequest* request,
                                         GetUserContactsResponse* response);
    virtual grpc::Status UploadContacts(grpc::ServerContext* context,
                                        const UploadContactsRequest* request,
                                        UploadContactsResponse* response);
  private:
    std::unique_ptr<ContactsDB> db_;
  };

}  // namespace orangelab 
