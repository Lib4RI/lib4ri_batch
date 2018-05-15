#!/bin/bash

USER=<user_name>
PIDSELECT=<put here the PIDs selection rule>
PIDOPTIONS="<PIDs selection rule options>"
COMMAND=<put here the the command to execute>
COMMANDOPTIONS="<PIDs selection rule options>"

DRYRUN=true
if [[ $1 = "dryfalse" ]]
then
    DRYRUN=false
fi

echo drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options=\"$PIDOPTIONS\" --command=$COMMAND --command_options=\"$COMMANDOPTIONS\" --dry=$DRYRUN

drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options="$PIDOPTIONS" --command=$COMMAND --command_options="$COMMANDOPTIONS" --dry=$DRYRUN
