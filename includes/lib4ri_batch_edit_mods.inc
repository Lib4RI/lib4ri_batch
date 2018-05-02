<?php

function lib4ri_batch_edit_mods_edit_mods($repository, $pid){
    $object = islandora_object_load($pid);
    if (!in_array("ir:citationCModel", $object->models))
        return 0;
    
    $MODS = lib4ri_batch_edit_mods_get_MODS($object);
    $content = $MODS->content;
    
    echo $content;
    
    return $pid;
}


function lib4ri_batch_edit_mods_get_MODS($object){
    $MODS = null;
    foreach ($object as $datastream) {
        if ($datastream->id === 'MODS'){
            $MODS = $datastream;
            var_dump($MODS->content);
            break;
        }
    }
    return $MODS;
}