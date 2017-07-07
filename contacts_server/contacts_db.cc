/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_db.h
 * ---------------------------------------------------------------------------
 * Implements the database layer using levelDB.
 * ---------------------------------------------------------------------------
 */
#include "contacts_db.h"

#include "glog/logging.h"
#include "gflags/gflags.h"

namespace orangelab {
  using namespace std;
  
ContactsDB::~ContactsDB() {
  CloseDatabase();
}

void ContactsDB::CreateDatabase() {
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status;
  status = leveldb::DB::Open(options, db_name_, &db);
  assert(status.ok());
  delete db;
  db = NULL;
  status = leveldb::DB::Open(options, index_db_name_, &db);
  assert(status.ok());
  delete db;
  status = leveldb::DB::Open(options, phone_db_name_, &db);
  assert(status.ok());
  delete db;
  db = NULL;
}

void ContactsDB::OpenDatabase() {
  if (db_ != NULL) {
    return;
  }
  if (index_db_ != NULL) {
    return;
  }
  
  // Open the database.
  leveldb::Options options;
    // Note that the option should be false since it is going to open the database.
  // if the database is not created, we should have an error instead of creating one.
  options.create_if_missing = true;
  //options.create_if_missing = false;
  leveldb::Status status;
  status = leveldb::DB::Open(options, db_name_, &db_);
  assert(status.ok());

  status = leveldb::DB::Open(options, index_db_name_, &index_db_);
  assert(status.ok());  

  status = leveldb::DB::Open(options, phone_db_name_, &phone_db_);
  assert(status.ok());  
}

bool ContactsDB::CreateNewUser(const string& uid, const ContactInfo& contact) {
  if (db_ == NULL) {
    LOG(ERROR) << "Database is not opened. Failed.";
    return false;
  }
  // Update(create) the contacts db item.
  {
    string key = uid;
    std::string buffer;
    ContactBook book;
    book.mutable_myself()->CopyFrom(contact);
    book.SerializeToString(&buffer);
    leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, buffer);
    assert(status.ok());
  }

  // Update the phone-uid mapping
  {
    string key = contact.phone_key();;
    std::string buffer;
    ReverseUserId ruid;
    ruid.set_uid(uid);
    ruid.SerializeToString(&buffer);
    leveldb::Status status = phone_db_->Put(leveldb::WriteOptions(), key, buffer);
    assert(status.ok());    
  }
  return true;
}

bool ContactsDB::FindContactsWithNewUser(const ContactInfo& contact,
                                         vector<string>* reverse_uids) {
  if (db_ == NULL || index_db_ == NULL) {
    LOG(ERROR) << "Database is not opened. Failed.";
    return false;
  }

  if (contact.phone_key().empty()) {
    return false;
  }

  string key = contact.phone_key();
  string value;
  leveldb::Status status = index_db_->Get(leveldb::ReadOptions(), key, &value);
  if (status.IsNotFound()) {
    return false;
  }
  if (!status.ok()) {
    LOG(ERROR) << "Read the data failed. The database is corrupted.";
    return false;
  }
  ReverseContacts reverse_contacts;
  reverse_contacts.ParseFromArray(value.c_str(), value.size());
  VLOG(3) << reverse_contacts.DebugString();
  for (auto uid : reverse_contacts.uids()) {
    reverse_uids->push_back(uid);
  }
  return true;
}

string ContactsDB::GetUserIdByPhoneKey(const string phone_key) {
  string value;
  leveldb::Status status = phone_db_->Get(leveldb::ReadOptions(), phone_key, &value);
  if (status.IsNotFound()) {
    return "";
  }
  if (!status.ok()) {
    LOG(ERROR) << "Read the data failed. The database is corrupted.";
    return "";
  }
  ReverseUserId reverse_user_id;
  reverse_user_id.ParseFromArray(value.c_str(), value.size());
  return reverse_user_id.uid();
}

// Get User's contact info for himself.
bool ContactsDB::GetUserContactInfo(const string& uid,
                                    ContactInfo* contact) {
  string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), uid, &value);
  if (status.IsNotFound()) {
    return false;
  }
  if (!status.ok()) {
    LOG(ERROR) << "Read the data failed. The database is corrupted.";
    return false;
  }
  ContactBook book;
  book.ParseFromArray(value.c_str(), value.size());
  contact->CopyFrom(book.myself());
  return true;
}

bool ContactsDB::GetUserContacts(const string& uid,
                                 ContactInfoList* contacts_list) {
  string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), uid, &value);
  if (status.IsNotFound()) {
    return false;
  }
  if (!status.ok()) {
    LOG(ERROR) << "Read the data failed. The database is corrupted.";
    return false;
  }
  ContactBook book;
  book.ParseFromArray(value.c_str(), value.size());
  contacts_list->CopyFrom(book.contacts());
  return true;
}
  
bool ContactsDB::UploadContacts(const std::string& uid,
                                const ContactInfoList& contacts) {
  string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), uid, &value);
  if (status.IsNotFound()) {
    LOG(ERROR) << "Upload contacts for '" << uid << "' but not found...";
    return false;
  }
  if (!status.ok()) {
    LOG(ERROR) << "Read the data failed. The database is corrupted.";
    return false;
  }
  ContactBook book;
  book.ParseFromArray(value.c_str(), value.size());

  ContactBook new_book;
  new_book.CopyFrom(book);
  new_book.mutable_contacts()->CopyFrom(contacts);

  // NOTE(chengxu): the book contains the previous contact list for the user
  // the new_book contains the updated contact list. If we want to make a diff
  // of book and new_book, we will get the diff of the contacts in the different
  // version. For example, if we want to remove the index for all deleted contacts.
  // For now, we do not have this requirement to do this.
  string key = uid;
  std::string buffer;
  new_book.SerializeToString(&buffer);
  status = db_->Put(leveldb::WriteOptions(), key, buffer);
  assert(status.ok());

  // Update the index.
  for (auto contact : contacts.contacts()) {
    ReverseContacts index_item;
    string contact_phone_key = contact.phone_key();
    if (!GetIndexItem(contact_phone_key, &index_item)) {
      // The index item has not been set up. Create one.
      VLOG(3) << "create...contact_phone_key=" << contact_phone_key
              << " uid=" << uid;
      CreateIndexItem(contact_phone_key, uid);
    } else {
      VLOG(3) << "update...contact_phone_key=" << contact_phone_key
              << " uid=" << uid;
      index_item.add_uids(uid);
      DuplicateRemove(&index_item);
      UpdateIndexItem(contact_phone_key, index_item);
    }
  }
  return true;
}

  // Update an index item in indexDB
  bool ContactsDB::UpdateIndexItem(const std::string& contact_phone_key,
                                   const ReverseContacts& index_item) {
    string key = contact_phone_key;
    std::string buffer;
    index_item.SerializeToString(&buffer);
    leveldb::Status status = index_db_->Put(leveldb::WriteOptions(), key, buffer);
    assert(status.ok());
    return true;
  }

  // Create an index item in indexDB.
  //  <key:contact_phone_key> -> <value: a set of uid which contains this phone> 
  bool ContactsDB::CreateIndexItem(const std::string& contact_phone_key,
                                   const std::string& uid) {
    ReverseContacts index_item;
    index_item.add_uids(uid);
    return UpdateIndexItem(contact_phone_key, index_item);
  }

  // Retrives an index item from the indexDB.
  bool ContactsDB::GetIndexItem(const std::string& phone_key,
                                ReverseContacts* index_item) {
    string value;
    leveldb::Status status = index_db_->Get(leveldb::ReadOptions(), phone_key, &value);
    if (status.IsNotFound()) {
      return false;
    }
    if (!status.ok()) {
      LOG(ERROR) << "Read the data failed. The database is corrupted.";
      return false;
    }
    index_item->ParseFromArray(value.c_str(), value.size());
    return true;
  }

  void ContactsDB::DuplicateRemove(ReverseContacts* index_item) {
    set<string> uids;
    for (int i = 0; i < index_item->uids_size(); ++i) {
      uids.insert(index_item->uids(i));
    }
    index_item->clear_uids();
    assert(index_item->uids_size() == 0);
    for (auto uid : uids) {
      index_item->add_uids(uid);
    }
  }
  
  void ContactsDB::CloseDatabase() {
    if (db_ != NULL) {
      delete db_;
      db_ = NULL;
    }
    if (phone_db_ != NULL) {
      delete phone_db_;
      phone_db_ = NULL;
    }
    if (index_db_ != NULL) {
      delete index_db_;
      index_db_ = NULL;
    }
  }
}  //namespace orangelab
