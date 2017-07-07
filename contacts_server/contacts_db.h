/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_db.h
 * ---------------------------------------------------------------------------
 * Defines a database layer for managing the database in contacts storage and
 * indexing.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <string>
#include "leveldb/db.h"
#include "stream_service/contacts_server/contacts_service.grpc.pb.h"

namespace orangelab {

class ContactsDB {
 public:
  ContactsDB(std::string db_name,
             std::string phone_db_name,
             std::string index_db_name) {
    db_name_ = db_name;
    phone_db_name_ = phone_db_name;
    index_db_name_ = index_db_name;
    db_ = NULL;
    phone_db_ = NULL;
    index_db_ = NULL;
  }
  virtual ~ContactsDB();

  void CreateDatabase();
  void OpenDatabase();
  void CloseDatabase();

  // Manage the data in the database.
  bool CreateNewUser(const std::string& uid, const ContactInfo& contact);
  bool FindContactsWithNewUser(const ContactInfo& contact,
                               std::vector<std::string>* reverse_uids);
  // Returns the uid given the phone key of the user.
  // The result is empty string if the phone key has no corresponding user.
  std::string GetUserIdByPhoneKey(const std::string phone_key);

  bool GetUserContacts(const std::string& uid,
                       ContactInfoList* contact);
  bool GetUserContactInfo(const std::string& uid,
                          ContactInfo* contact);
  bool UploadContacts(const std::string& uid,
                      const ContactInfoList& contacts);

  // Manages the index data
  bool UpdateIndexItem(const std::string& contact_phone_key,
                       const ReverseContacts& index_item);
  bool CreateIndexItem(const std::string& contact_phone_key,
                       const std::string& uid);
  bool GetIndexItem(const std::string& phone_key,
                    ReverseContacts* index_item);
 private:
  void DuplicateRemove(ReverseContacts* index_item);
  
  leveldb::DB* db_;
  std::string db_name_;
  leveldb::DB* phone_db_;
  std::string phone_db_name_;
  leveldb::DB* index_db_;
  std::string index_db_name_;
};
 
}  // namespace orangelab 
