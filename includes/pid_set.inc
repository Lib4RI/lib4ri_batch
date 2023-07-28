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
        return trim(array_pop($this->pids),"\n\r");
    }
    
    public function get_pids_list(){
        return implode("\n", array_reverse($this->pids));
    }
    
}

class single extends get_pids{
    
    protected function get_pid_set(){
        return [$this->options['pid']];
    }
}

class textfile extends get_pids{
        
    protected function get_pid_set(){
        return array_reverse(array_unique(file($this->options['filename'])));
    }    
}

class solr_link extends get_pids{

    protected function get_pid_set(){
		// Example Solr URL (can be a CSV or Json):
		// http://lib-dora-prod1.emp-eaw.ch:8080/solr/collection1/select?q=PID%3Awsl-aut*%5c%3a*+AND+MADS_family_mt:C*&sort=PID+asc&rows=987654321&fl=PID%2c+MADS_family_mt%2c+MADS_u1_mt%2c+MADS_organization_mt&wt=csv&indent=true&rows=987654321
        if ( !( $solrUrl = @trim($this->options['url']) ) ) {
            $solrUrl = @trim($this->options['link']); // no 'url' option - retry with 'link' (since class is called Solr_LINK)
        }
        if ( substr($solrUrl,0,4) != 'http' ) {
            return; // empty URL or not starting with 'http'
        }
        if ( !( $solrJson = @file_get_contents(str_replace(['?','wt=csv&',],['?wt=json&','wt=json&'],$solrUrl.'&')) ) ) { // download as Json!
            return; // could not access/download this link
        }
        if ( ( $dumpFile = @trim($this->options['dump']) ) && @is_dir(dirname($dumpFile)) ) { 
			@file_put_contents( $dumpFile, $solrJson ); // to dump the file on request/option into an exisiting directory
		}
        if ( !( $solrAry = @json_decode($solrJson,TRUE) ) ) { // true: for an array
            return; // Json decoding failed
        }
        if ( @intval($solrAry['response']['numFound']) < 1 || !isset($solrAry['response']['docs']) ) {
            return; // no Solr results
        }
        return array_unique(array_filter(array_map( function($ary) { return @strval($ary['PID']); }, $solrAry['response']['docs'] )));
    }    
}

class solr_search extends get_pids{
    
    protected function get_params(){
        $this->query = $this->options['query'];
        $this->fq = $this->options['fq'];
        $this->facet = $this->options['facet'];
        $this->facetLimit = $this->options['facetLimit'];
        $this->solrLimit = $this->options['solrLimit']; 
    }
    
    protected function get_pid_set(){
        $this->get_params();
        $qp = new IslandoraSolrQueryProcessor();
//        $qp->buildQuery('PID:' . $namespace . '\:*');
        $qp->buildQuery($this->query);
        $qp->solrParams['fl'] = 'PID';
//         $qp->solrParams['fq'] = array(
//             'RELS_EXT_hasModel_uri_ms:"info:fedora/' . $content_model . '"',
//             'RELS_EXT_isMemberOfCollection_uri_ms:"info:fedora/' . $collection_pid . '"',
//             'mods_genre_ms:"' . $genre . '"',
//         );
        
            
        // Lib4ri appends a number to the id of PDF datastreams beyond the first.
        // look for a datastream with the pattern PDF + any other character to
        // determine multiple PDFs.
//         $qp->solrParams['fq'][] = format_string('-!field:/!prefix[0-9A-Za-z]+/', array(
//             '!field' => 'fedora_datastreams_ms',
//             '!prefix' => variable_get('lib4ridora_extra_pdf_datastream_prefix', 'PDF'),
//         ));
    
        $qp->solrParams['facet'] = $this->facet;
//         $qp->solrParams['facet.mincount'] = 1;
//         $qp->solrParams['facet.field'] = $facet_fields;
        $qp->solrParams['facet.limit'] = $this->facetLimit;
        if ($this->options['plainQuery'] == TRUE){
            $qp->solrParams['fq'] = array();
        }
        $qp->solrLimit = $this->solrLimit;
        $qp->executeQuery(TRUE, FALSE);
        $results = $qp->islandoraSolrResult['response']['objects'];
//         var_dump($qp);
//         var_dump($results);
        $pids = [];
        foreach ($results as $result){
            array_push($pids, $result['PID']);
        }
        return array_reverse($pids);
    }
}

class solr_query extends solr_search{
    protected function get_params(){
        $this->query = $this->options['query'];
        $this->fq = null;
        $this->facet = 'false';
        $this->facetLimit = null;
        $this->solrLimit = 100000000;
    }
}

class title extends solr_search{
    protected function get_params(){
        $this->query = 'dc.title:('.$this->options['title'].')';
        $this->fq = null;
        $this->facet = 'false';
        $this->facetLimit = null;
        $this->solrLimit = 100000000;
    }
}

class dirlist extends get_pids{
    
    protected function get_pid_set(){
        $files = scandir ($this->options['dirname']);
        $to_unset = [];
        foreach ($files as $ii => $file){
            if (($file == '.') || ($file == '..'))
                array_push($to_unset, $ii);
            $file = str_replace('.xml', '', $file);
            $files[$ii] = str_replace('_', ':', $file);
        }
        foreach ($to_unset as $key => $file)
            unset ($files[$key]);
        
        return $files;
    }
}


class name_space extends get_pids{
    
    protected function get_pid_set(){
        $connection = islandora_get_tuque_connection();
        $repository = $connection->repository;
        
        $n = $this->get_object_count_from_namespace($repository, $this->options['namespace']);
         $pids = [];
         for ($ii = 1; $ii <= $n; $ii++) {
             $temp = $this->get_object_set_from_namespace($repository, $this->options['namespace'], $ii-1)[0]['pid']['value'];
             array_push($pids, $temp);
         }
        return array_reverse($pids);
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
    private function get_object_count_from_namespace($repository, $namespace) {
        $query = $this->get_namespace_query($namespace);
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
    private function get_object_set_from_namespace($repository, $namespace, $offset) {
        $query = $this->get_namespace_query($namespace, $offset);
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
    private function get_namespace_query($namespace, $offset = 0) {
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
}
