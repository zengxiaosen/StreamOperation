#coding=utf-8
"""
upload record files.
"""

import os
import sys
import string

import my_bos_conf
from baidubce import exception
from baidubce.services.bos.bos_client import BosClient

def _delete_file_folder(src):  
    '''delete files and folders''' 
    if os.path.isfile(src):  
        try:  
            os.remove(src)  
        except:  
            pass 
    elif os.path.isdir(src):  
        for item in os.listdir(src):  
            itemsrc = os.path.join(src,item)  
            _delete_file_folder(itemsrc)  
        try:  
            os.rmdir(src)  
        except:  
            pass 

def _find_finished_records(records_path):
    dir = records_path
    finished_record = []
    for session_dir in os.listdir(dir):
        session_path = os.path.join(dir, session_dir)
        flag_file = session_path + "/finish.tag"
        if (os.path.exists(flag_file)):
            finished_record.append(session_path)
    return finished_record

def _upload_records(finished_records, bucket_name):
    # bos
    bos_client = BosClient(my_bos_conf.config)
    if not bos_client.does_bucket_exist(bucket_name):
        print "bucket not exist. ask administrator for it."
        sys.exit(1)

    # upload
    for record_path in finished_records:
        (record_root, record_dir_name) = os.path.split(record_path);
        (room_id, session_id) = record_dir_name.split("_");
        for item in os.listdir(record_path):
            record_file = os.path.join(record_path, item)
            if (item == "finish.tag" or False == os.path.isfile(record_file)):
                continue
            key = room_id + "/" + item
            bos_client.put_object_from_file(bucket_name, key, record_file)
            print "upload " + key
        bos_client.put_object_from_string(bucket_name, "index_files/"+room_id, room_id);
        _delete_file_folder(record_path)


if __name__ == "__main__":
    
    #import logging
    #logging.basicConfig(level=logging.DEBUG)
    #__logger = logging.getLogger(__name__)

    record_path = sys.argv[1]
    if (False == os.path.isdir(record_path)):
        print "invalid record path: " + record_path
        sys.exit(1)

    bucket_name = sys.argv[2]
    if (False == bucket_name.isalpha()):
        print "invalid bucket name: " + bucket_name 
        sys.exit(1)

    #record_path = "/tmp/records"
    #bucket_name = "vipkid"
    finish_records = _find_finished_records(record_path)
    _upload_records(finish_records, bucket_name)

