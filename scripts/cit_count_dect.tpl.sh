#!/bin/bash

USER=<user>
IDLIST='<path_relative_to_the_working_dir_to_save_data_to>'

PIDSELECT=name_space
PIDOPTIONS="namespace=eawag"

# The two following lines for testing purpose. Uncomment to test on a single islandora object 
#PIDSELECT=single
#PIDOPTIONS="pid=<islandora_objct_pid>"

COMMAND=cit_count_detect
COMMANDOPTIONS="idlabels=doi;scopus;pmid|idsep=,|outfile=$IDLIST|wd=$PWD"

DRYRUN=true
if [[ $1 = "dryfalse" ]]
then
    DRYRUN=false
fi

echo drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options=\"$PIDOPTIONS\" --command=$COMMAND --command_options=\"$COMMANDOPTIONS\" --dry=$DRYRUN

drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options="$PIDOPTIONS" --command=$COMMAND --command_options="$COMMANDOPTIONS" --dry=$DRYRUN >&1
