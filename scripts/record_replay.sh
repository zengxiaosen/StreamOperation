#!/bin/bash
## replay webm to mp4, and upload to bos
## @param records_root the root path of records 
set -e

convert_webm_to_mp4()
{
    folder=${1}
    for filename in $(ls ${folder}/*.webm)  
    do
        webmfile=${folder}/${filename}
        # TODO 
    done
}

process_record()
{
    folder=${1}
    # replay pb file
    pbfile=`ls ${folder}/*.pb`
    echo ./replay_pipeline_main \
		--logtostderr \
		--replay_files=${pbfile} \
		--export_directory=${folder}/ \
		--record_format=webm \
		--use_ntp_time=true \
		--debug_console=false
    
    # convert to mp4
    convert_webm_to_mp4 ${folder}

    # clear
    #rm ${folder}/*.pb
    #rm ${folder}/*.webm
}

## main 
records_root=${1}
for folder in $(ls ${records_root})
do
   record_folder=${records_root}/$folder 
   echo process_record ${record_folder} 
   process_record ${record_folder} 
done

## upload
cd record_upload
bos_bucket=testvipkid
python run_upload_records.py ${records_root} ${bos_bucket} 
cd -

echo "success"

