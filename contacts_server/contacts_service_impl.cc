/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_service_impl.cc
 * ---------------------------------------------------------------------------
 * Implements the contacts service interface.
 * ---------------------------------------------------------------------------
 */

#include "contacts_service_impl.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include "flag.h"
namespace orangelab {
  
ContactsServiceImpl::ContactsServiceImpl() {
  db_.reset(new ContactsDB(FLAGS_db_name,
                           FLAGS_phone_db_name,
                           FLAGS_index_db_name));
}

// virtual
grpc::Status ContactsServiceImpl::CreateNewUser(grpc::ServerContext* context,
                                                const CreateNewUserRequest* request,
                                                CreateNewUserResponse* response) {
  response->set_code("OK");
  response->set_message("CreateNewUser okay.");
  VLOG(3) << "CreateNewUser.";
  // Note we should create database first.
  db_->OpenDatabase();
  if (!db_->CreateNewUser(request->uid(), request->user_info())) {
    response->set_code("Error");
    response->set_message("GetUserContacts has error.");    
    return grpc::Status::OK;
  }

  std::vector<std::string> friends;
  db_->FindContactsWithNewUser(request->user_info(), &friends);
  for (auto myfriend : friends) {
    response->mutable_friends()->add_uids(myfriend);
  }
  return grpc::Status::OK;
}

  // virtual
grpc::Status ContactsServiceImpl::GetUserContacts(grpc::ServerContext* context,
                                                  const GetUserContactsRequest* request,
                                                  GetUserContactsResponse* response) {
  VLOG(3) << "GetUserContacts.";
  // Note we should create database first.
  db_->OpenDatabase();
  string uid = request->uid();
  ContactInfo user_info;
  ContactInfoList contacts;
  bool ret = db_->GetUserContactInfo(uid, &user_info);
  if (ret) {
    ret = db_->GetUserContacts(uid, &contacts);
    if (ret) {
      response->set_code("OK");
      response->set_message("GetUserContacts okay.");
      response->set_uid(uid);
      response->mutable_user_info()->CopyFrom(user_info);
      response->mutable_user_contacts()->CopyFrom(contacts);
      return grpc::Status::OK;
    }
  }
  response->set_code("Error");
  response->set_message("GetUserContacts has error.");    
  return grpc::Status::OK;
}

  
//virtual
grpc::Status ContactsServiceImpl::UploadContacts(grpc::ServerContext* context,
                                                 const UploadContactsRequest* request,
                                                 UploadContactsResponse* response) {
  VLOG(3) << "UploadContacts.";
  db_->OpenDatabase();
  string uid = request->uid();
  ContactInfo user_info;
  bool ret = db_->GetUserContactInfo(uid, &user_info);
  if (!ret) { 
    response->set_code("Error");
    response->set_message("UploadContacts has error.");    
    return grpc::Status::OK;
  }

  ContactInfoList contacts = request->contacts();
  ret = db_->UploadContacts(uid, contacts);

  for (auto contact : contacts.contacts()) {
    ContactInfo contact_info;
    string cuid;
    cuid = db_->GetUserIdByPhoneKey(contact.phone_key());
    if (!cuid.empty()) {
      if (db_->GetUserContactInfo(cuid, &contact_info)) {
        RegisteredUserContact* reg_user = response->add_reg_contacts();
        reg_user->mutable_contact()->CopyFrom(contact_info);
        reg_user->set_uid(cuid);
      }
    }
  }
  
  response->set_code("OK");
  response->set_message("UploadContacts okay.");
  return grpc::Status::OK;
}

}  // namespace orangelab
