<?php

include_once "includes/pid_set.inc";
include_once "includes/commands.inc";

/**
 * @file
 * MODS bacth edit drush command.
 */
/**
 * Implements hook_drush_command().
 */
function lib4ri_batch_drush_command() {
  return array(
    'dora-batch' => array(
        'description' => dt('Generic DORA batch command execution.'),
        'drupal_dependencies' => array('islandora'),
        'examples' => array('drush -u 1 -v dora-batch --pids=<> --pids_options="" --command=<> --command_options=<> --dry =false'),
        'options' => array(
            'pids' => array('description' => dt('Pids selection method')),
            'pids_options' => array('description' => dt('Option relevent to pids selection method')),
            'command' => array('description' => dt('The command to apply')),
            'command_options' => array('description' => dt('Option relevent to command')),
            'dry' => array('description' => dt('Dry run')),
      ),
    ),
  );
}

/**
 * Drush command.
 * drush_[MODULE_NAME]_[COMMAND_NAME]().
 */
function drush_lib4ri_batch_dora_batch($pids = null, $pids_options = null, $command = null, $command_options = null, $dry = true ) {
    $command = drush_get_option('command');
    
    $dry_o = drush_get_option('dry');
    if ($dry_o == 'false') //Default to true
        $dry = false;
    else
        $dry = true;
    
    $commands = array();
    foreach (get_declared_classes() as $class) {
        if (is_subclass_of($class, command))
            $commands[] = $class;
    }
    
    if (!in_array($command, $commands)){
        return 'Command unknown!';
    }
    
    batch_set(lib4ri_batch_dora_batch_batch(drush_get_option('pids'), read_options(drush_get_option('pids_options')), $command, read_options(drush_get_option('commands_options')), $dry));
    drush_backend_batch_process();
}

/**
 * Batch process definition for the repository duplicator command.
 *
 */
function lib4ri_batch_dora_batch_batch($pids, $pids_options, $command, $command_options, $dry) {
    return array(
        'title' => t('drush_lib4ri_batch_dora_batch'),
        'init_message' => t("..."),
        'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining: @estimate.'),
        'error_message' => t('An error has occured.'),
        'operations' => array(
            array(
                'lib4ri_batch_dora_batch_batch_operation',
                array($pids, $pids_options, $command, $command_options, $dry),
            ),
        ),
    );
}

/**
 * Batch operation for repository duplicator.
 *
 */
function lib4ri_batch_dora_batch_batch_operation($pids, $pids_options, $command, $command_options, $dry, &$context) {
//     module_load_include('inc', 'lib4ri_batch_edit_mods', 'includes/lib4ri_batch_edit_mods');
    $sandbox = &$context['sandbox'];
    // Establish the connection to tuque; this is where the magic starts.
    // If this is our first pass through the batch operation, the batch context's
    // sandbox will be empty, so we know that we have variables to initialize.
    if (empty($sandbox)) {
        // Specifically, we need to know where we are in the batch (current_index),
        // how many things are in the batch (total), and whether or not we are
        // done of the batch (finished, also, maybe?).
        $sandbox['pids_class'] = new $pids($pids_options);
        
        $sandbox['current_index'] = 0;
        $sandbox['total'] = $sandbox['pids_class']->get_pids_count();
        $context['finished'] = $sandbox['total'] === 0 ? 1 : 0;
        if ($context['finished']) {
            return;
        }
    }
    // Grab the next object in the namespace.
    $pid = $sandbox['pids_class']->get_pid();
    // Duplicate the object represented by the return value from the query.
    $context['message'] = lib4ri_batch_exec_command($command, $pid, $dry); //DO STUFF HERE!!!!
    // Increment the query offset, otherwise.
    $sandbox['current_index']++;
    // Finish if this was the last object.
    $context['finished'] = $sandbox['current_index'] / $sandbox['total'];

}

function read_options($options_str){
    $options = [];
    $options_str_a = explode(';',$options_str);
    foreach ($options_str_a as $option){
        $kv = explode('=',$option);
        $options[$kv[0]]=$kv[1];
    }
    return $options;
}

function lib4ri_batch_exec_command($rule, $pid, $dry){
    $class = $rule;
    $edit = new $class($pid, $dry);
    return($edit->exec());
    
    
//    new test_edit($repository, $pid);
}
