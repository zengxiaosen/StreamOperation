/* Defines the flags for db operation. */
#pragma once

DEFINE_string(db_name, "/tmp/testdb",
              "The levelDB database name for contacts storage.");
DEFINE_string(phone_db_name, "/tmp/testphone",
              "The levelDB database name for phone-user indexing.");
DEFINE_string(index_db_name, "/tmp/testindex",
              "The levelDB database name for contacts indexing.");

