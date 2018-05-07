<?php

//include_once "includes/lib4ri_batch_edit_mods.inc";
include_once "includes/queries.inc";
include_once "includes/rules.inc";

/**
 * @file
 * MODS bacth edit drush command.
 */
/**
 * Implements hook_drush_command().
 */
function lib4ri_batch_edit_mods_drush_command() {
  return array(
    'edit-mods' => array(
        'description' => dt('Batch edits MODS according to the specified rule and parameters.'),
        'drupal_dependencies' => array('islandora'),
        'examples' => array('drush -u 1 -v edit-mods --namespace=islandora'),
        'options' => array(
            'rule' => array('description' => dt('The edit rule to apply')),
            'namespace' => array('description' => dt('The namespace')),
            'single' => array('description' => dt('Whether to process a single object')),
            'pid' => array('description' => dt('The pid of the islandora object')),
            'dry' => array('description' => dt('Dry run')),
      ),
    ),
  );
}

/**
 * Drush command.
 * drush_[MODULE_NAME]_[COMMAND_NAME]().
 */
function drush_lib4ri_batch_edit_mods_edit_mods($rule = null, $namespace = 'islandora', $single = false, $pid = null, $dry = true ) {
    $rule = drush_get_option('rule');
    
    $single_o = drush_get_option('single');
    if ($single_o == 'true') //Default to false
        $single = true;
    else
        $single = false;

    $dry_o = drush_get_option('dry');
    if ($dry_o == 'false') //Default to true
        $dry = false;
    else
        $dry = true;
    
    $rules = array();
    foreach (get_declared_classes() as $class) {
        if (is_subclass_of($class, batch_edit_mods))
            $rules[] = $class;
    }
    
    if (!in_array($rule, $rules)){
        return 'Unknown editing rule!';
    }
    
    batch_set(lib4ri_batch_edit_mods_edit_mods_batch($rule, drush_get_option('namespace'), $single, drush_get_option('pid'), $dry));
    drush_backend_batch_process();
}

/**
 * Batch process definition for the repository duplicator command.
 *
 */
function lib4ri_batch_edit_mods_edit_mods_batch($rule, $namespace, $single, $pid, $dry) {
    return array(
        'title' => t('drush_lib4ri_batch_edit_mods_edit_mods'),
        'init_message' => t("..."),
        'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining: @estimate.'),
        'error_message' => t('An error has occured.'),
        'operations' => array(
            array(
                'lib4ri_batch_edit_mods_edit_mods_batch_operation',
                array($rule, $namespace, $single, $pid, $dry),
            ),
        ),
    );
}

/**
 * Batch operation for repository duplicator.
 *
 */
function lib4ri_batch_edit_mods_edit_mods_batch_operation($rule, $namespace, $single, $pid, $dry, &$context) {
//     module_load_include('inc', 'lib4ri_batch_edit_mods', 'includes/lib4ri_batch_edit_mods');
    $sandbox = &$context['sandbox'];
    // Establish the connection to tuque; this is where the magic starts.
    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;
    
    if ($single == true){
        $context['message'] = lib4ri_batch_edit_mods_edit_mods($rule, $repository, $pid, $dry);
    }
    else{            
        // If this is our first pass through the batch operation, the batch context's
        // sandbox will be empty, so we know that we have variables to initialize.
        if (empty($sandbox)) {
            // Specifically, we need to know where we are in the batch (current_index),
            // how many things are in the batch (total), and whether or not we are
            // done of the batch (finished, also, maybe?).
            $sandbox['current_index'] = 0;
            $sandbox['total'] = get_object_count_from_namespace($repository, $namespace);
            $context['finished'] = $sandbox['total'] === 0 ? 1 : 0;
            if ($context['finished']) {
                return;
            }
        }
        // Grab the next object in the namespace.
        $object = get_object_set_from_namespace($repository, $namespace, $sandbox['current_index']);
        // Duplicate the object represented by the return value from the query.
        $context['message'] = lib4ri_batch_edit_mods_edit_mods($rule, $repository, $object[0]['pid']['value'], $dry); //DO STUFF HERE!!!!
        // Increment the query offset, otherwise.
        $sandbox['current_index']++;
        // Finish if this was the last object.
        $context['finished'] = $sandbox['current_index'] / $sandbox['total'];
    }
}



function lib4ri_batch_edit_mods_edit_mods($rule, $repository, $pid, $dry){
    $class = $rule;
    $edit = new $class($repository, $pid, $dry);
    return($edit->exec());
    
    
//    new test_edit($repository, $pid);
}

