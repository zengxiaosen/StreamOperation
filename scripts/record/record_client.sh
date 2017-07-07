#!/bin/bash
## rsync record videos
## @param records_root the root path of records 
set -e

# rsync server configure
rsync_host=54.223.74.128
rsync_port=8731
rsync_user=root
rsync_password=xchlrsync
rsync_floder=records

# do rsync
# @param record_folder
do_rsync()
{
    folder=${1}
    if [ -d "${folder}" ]
    then
        echo ${rsync_password} > ps.file && chmod 0600 ps.file
        rsync -vzrt --progress ${folder} ${rsync_user}@${rsync_host}::${rsync_floder} \
		--port=${rsync_port} --password-file=ps.file
        rm ps.file
    else 
        echo the folder \"${folder}\" not exist

    fi   
}

# check if a record_folder can be rsync
# @param record_folder
can_rsync_record()
{
    folder=${1}
    testfile=${folder}/*timeline.json
    if [ -f $testfile ]
    then
	echo "yes" 
    else 
	echo "no"
    fi 
}

# clear records that had rsync
# @param record_folder
clear_records()
{
    folder=${1}
    if [ -d "${folder}" ]
    then
        rm -rf ${folder}
    fi
}

## main 
records_root=${1}
for folder in $(ls ${records_root})
do
   record_folder=${records_root}/$folder 
   result=`can_rsync_record ${record_folder}`
   if [ "${result}" == "yes" ]
   then
       echo rsync ${record_folder} 
       do_rsync ${record_folder} 
       clear_records ${record_folder}
   fi
done
echo "success"

