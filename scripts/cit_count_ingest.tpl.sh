#!/bin/bash

USER=<islandora_user>
XMLDIR='<full_path_of_cit_count_xml_file_to_ingest>'

PIDSELECT=dirlist
PIDOPTIONS="dirname=$XMLDIR"

# The two following lines for testing purpose. Uncomment to test on a single islandora object 
#PIDSELECT=single
#PIDOPTIONS="pid=<islandora_objct_pid>"

COMMAND=cit_count_ingest
COMMANDOPTIONS="dirname=$XMLDIR"

DRYRUN=true
if [[ $1 = "dryfalse" ]]
then
    DRYRUN=false
fi

echo drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options=\"$PIDOPTIONS\" --command=$COMMAND --command_options=\"$COMMANDOPTIONS\" --dry=$DRYRUN

drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options="$PIDOPTIONS" --command=$COMMAND --command_options="$COMMANDOPTIONS" --dry=$DRYRUN >&1
