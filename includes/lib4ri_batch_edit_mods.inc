<?php

function lib4ri_batch_edit_mods_edit_mods($repository, $pid){
    $object = islandora_object_load($pid);
    if (!in_array("ir:citationCModel", $object->models))
        return 0;
    
    return $pid;
}

