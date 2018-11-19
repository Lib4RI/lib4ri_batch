#!/bin/bash

USER="<islandora_user>"

PIDSELECT=single
PIDOPTIONS="lib4ri:12345"

COMMAND=pdfa_ingest
COMMANDOPTIONS="pid=optional:1010|pdfa=/tmp/required.pdfa.pdf|dsId=pdf_pdf-a|dsLabel=pdfa-filename|dsRedo=false"

DRYRUN=true
if [[ $1 = "dryfalse" ]]
then
    DRYRUN=false
fi


if [[ $1 = ":" ]]
then
    drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options="$PIDOPTIONS" --command=$COMMAND --command_options="$COMMANDOPTIONS" --dry=$DRYRUN >&1
else
    echo drush -u $USER -r /var/www/html  dora-batch --pids=$PIDSELECT --pids_options=\"$PIDOPTIONS\" --command=$COMMAND --command_options=\"$COMMANDOPTIONS\" --dry=$DRYRUN
fi

# example:
# drush -u 1 -r /var/www/html dora-batch --pids=single --pids_options='pid=eawag:679' --command=pdfa_ingest --command_options='pdfa=/tmp/eawag-679.PDF.1542369045.PDF-A.pdf|dsId=PDF_PDF-A|dsLabel=eawag-679.PDF.1542369045.PDF-A.pdf' --dry=false

