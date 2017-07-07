#!/bin/bash

while true
do
   python run_upload_records.py /tmp/records testvipkid >> run.log
   sleep 60 
done
