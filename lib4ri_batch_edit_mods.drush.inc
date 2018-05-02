<?php

//include_once "includes/lib4ri_batch_edit_mods.inc";
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
      'examples' => array('drush -u 1 -v edit-mods --source_namespace=islandora'),
      'options' => array(
        'namespace' => array('description' => dt('The namespace')),
      ),
    ),
    'edit-single-mods' => array(
        'description' => dt('Batch edits a single MODS according to the specified rule and parameters.'),
        'drupal_dependencies' => array('islandora'),
        'examples' => array('drush -u 1 -v edit-single-mods --pid=eawag:458'),
        'options' => array(
            'pid' => array('description' => dt('The pid of the islandora object')),
        ),
    )  
  );
}

/**
 * Drush command.
 * drush_[MODULE_NAME]_[COMMAND_NAME]().
 */
function drush_lib4ri_batch_edit_mods_edit_mods($namespace = 'islandora') {
    batch_set(lib4ri_batch_edit_mods_edit_mods_batch(drush_get_option('namespace')));
    drush_backend_batch_process();
}

/**
 * Batch process definition for the repository duplicator command.
 *
 */
function lib4ri_batch_edit_mods_edit_mods_batch($namespace) {
    return array(
        'title' => t('drush_lib4ri_batch_edit_mods_edit_mods'),
        'init_message' => t("..."),
        'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining: @estimate.'),
        'error_message' => t('An error has occured.'),
        'operations' => array(
            array(
                'lib4ri_batch_edit_mods_edit_mods_batch_operation',
                array($namespace),
            ),
        ),
    );
}

/**
 * Batch operation for repository duplicator.
 *
 */
function lib4ri_batch_edit_mods_edit_mods_batch_operation($namespace, &$context) {
//     module_load_include('inc', 'lib4ri_batch_edit_mods', 'includes/lib4ri_batch_edit_mods');
    
    $sandbox = &$context['sandbox'];
    // Establish the connection to tuque; this is where the magic starts.
    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;
    // If this is our first pass through the batch operation, the batch context's
    // sandbox will be empty, so we know that we have variables to initialize.
    if (empty($sandbox)) {
        // Specifically, we need to know where we are in the batch (current_index),
        // how many things are in the batch (total), and whether or not we are
        // done of the batch (finished, also, maybe?).
        $sandbox['current_index'] = 0;
        $sandbox['total'] = lib4ri_batch_edit_mods_get_object_count_from_namespace($repository, $namespace);
        $context['finished'] = $sandbox['total'] === 0 ? 1 : 0;
        if ($context['finished']) {
            return;
        }
    }
    // Grab the next object in the namespace.
    $object = lib4ri_batch_edit_mods_get_object_set_from_namespace($repository, $namespace, $sandbox['current_index']);
    // Duplicate the object represented by the return value from the query.
    $context['message'] = lib4ri_batch_edit_mods_edit_mods($repository, $object[0]['pid']['value']); //DO STUFF HERE!!!!
    // Increment the query offset, otherwise.
    $sandbox['current_index']++;
    // Finish if this was the last object.
    $context['finished'] = $sandbox['current_index'] / $sandbox['total'];

}



/**
 * Returns the appropriate SPARQL query for the source namespace and offset.
 *
 * @param string $source_namespace
 *   The namespace to search for objects in.
 * @param int $offset
 *   The amount to offset the query by. Defaults to 0.
 *
 * @return string
 *   A SPARQL query for finding all objects based on a given namespace at a
 *   given offset, minus content models.
 */
function lib4ri_batch_edit_mods_get_namespace_query($namespace, $offset = 0) {
    // To start, we want to SELECT ?pids from the resource index.
    $query = <<<EOQ
SELECT ?pid
FROM <#ri>
EOQ;
    // Next, we're narrowing our search to ?pids that have the FedoraObject
    // content model.
    $query .= <<<EOQ
WHERE {
  ?pid <fedora-model:hasModel> <info:fedora/fedora-system:FedoraObject-3.0>
EOQ;
    // Now, it gets a bit tricky. To remove ContentModel objects, we'll need to
    // set up an OPTIONAL matching pattern for the ContentModel so that we can
    // later filter on it if it isn't BOUND to a value on the graph. Negation
    // as failure ftw.
    // XXX: We shouldn't have to add createdDate to our OPTIONAL pattern - it
    // should be as simple as filtering any ?pid hasModel ?model inside the
    // OPTIONAL where ?model is ContentModel. However, Mulgara doesn't match the
    // spec properly, and requires an object to be found for the OPTIONAL to work.
    // So, we match both a property that might be there (ContentModel), and a
    // property that will definitely be there (createdDate), and filter on the
    // property that will definitely be there in cases where they're both set.
    $query .= <<<EOQ
    OPTIONAL {
      ?pid <fedora-model:hasModel> <info:fedora/fedora-system:ContentModel-3.0> ;
           <fedora-model:createdDate> ?date .
    }
  FILTER(!BOUND(?date))
EOQ;
    // Next, we filter all results from the graph where the string literal version
    // of the ?pid starts with info:fedora/namespace:. The carat reads from the
    // start of a string in regex.
    $query .= <<<EOQ
  FILTER(REGEX(STR(?pid), "^info:fedora/$namespace:"))
}
EOQ;
    // Lastly, we establish the offset to start the result set from.
    $query .= <<<EOQ
OFFSET $offset
EOQ;
    // Return the generated query.
    return $query;
}


/**
 * Gets the count of existing objects in the RI for the given namespace.
 *
 * @param IslandoraFedoraRepository $repository
 *   The repository to query in.
 * @param $source_namespace
 *   The namespace to filter the query by.
 *
 * @return int
 *   The number of objects in the RI for the given namespace.
 */
function lib4ri_batch_edit_mods_get_object_count_from_namespace($repository, $namespace) {
    $query = lib4ri_batch_edit_mods_get_namespace_query($namespace);
    return $repository->ri->countQuery($query, 'sparql');
}


/**
 * Gets an object from the RI for the given namespace at the given offset.
 *
 * @param IslandoraFedoraRepository $repository
 *   The repository to query in.
 * @param string $source_namespace
 *   The namespace to filter the query by.
 * @param int $offset
 *   The amount to offset the query by.
 *
 * @return array
 *   An array representing the result of the query.
 */
function lib4ri_batch_edit_mods_get_object_set_from_namespace($repository, $namespace, $offset) {
    $query = lib4ri_batch_edit_mods_get_namespace_query($namespace, $offset);
    return $repository->ri->sparqlQuery($query, 1);
}

/************************************************************************/

function drush_lib4ri_batch_edit_mods_edit_single_mods($pid = null) {
    batch_set(lib4ri_batch_edit_mods_edit_single_mods_batch(drush_get_option('pid')));
    drush_backend_batch_process();
}

function lib4ri_batch_edit_mods_edit_single_mods_batch($pid) {
    return array(
        'title' => t('drush_lib4ri_batch_edit_mods_edit_single_mods'),
        'init_message' => t("..."),
        'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining: @estimate.'),
        'error_message' => t('An error has occured.'),
        'operations' => array(
            array(
                'lib4ri_batch_edit_mods_edit_single_mods_batch_operation',
                array($pid),
            ),
        ),
    );
}

function lib4ri_batch_edit_mods_edit_single_mods_batch_operation($pid, &$context) {
    $sandbox = &$context['sandbox'];
    // Establish the connection to tuque; this is where the magic starts.
    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;
    
    $context['message'] = lib4ri_batch_edit_mods_edit_mods($repository, $pid);
}


function lib4ri_batch_edit_mods_edit_mods($repository, $pid){
    new test_edit($repository, $pid);
}

