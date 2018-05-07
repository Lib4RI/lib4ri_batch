<?php

abstract class get_pids{
    protected $pids;
    protected $pids_count;
    
    public function __construct($options) {
        $this->options = $options;
        $this->pids = $this->get_pid_set();
        $this->pids_count = $this->get_pids_current_count();
    }

    public function get_pids_count(){
        return $this->pids_count;
    }
    
    protected function get_pids_current_count(){
        return sizeof($this->pids);
    }
    
    public function get_pid(){
        var_dump($this->pids);
        return array_pop($this->pids);
    }
    
}

class single extends get_pids{
    
    protected function get_pid_set(){
        return [$this->options['pid']];
    }
}

class textfile extends get_pids{
        
    protected function get_pid_set(){
        return file($this->options['filename']);
    }    
}

class name_space extends get_pids{
    
    protected function get_pid_set(){
        return 0;
    }
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
function get_object_count_from_namespace($repository, $namespace) {
    $query = get_namespace_query($namespace);
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
function get_object_set_from_namespace($repository, $namespace, $offset) {
    $query = get_namespace_query($namespace, $offset);
    return $repository->ri->sparqlQuery($query, 1);
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
function get_namespace_query($namespace, $offset = 0) {
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
