<?php
abstract class command{
    protected   $pid;
    protected   $dry;
    protected   $object;
    protected   $message;
    protected   $stop;
    
    public function __construct($pid, $dry, $options) {
        $this->pid = $pid;
        $this->dry = $dry;
        $this->options = $options;
        $this->message = '';
        $this->stop = false;
    }

    public function exec(){
        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        
        if ($this->stop == false)
            $this->_dry_exec();
        
        if (($this->dry == false) && ($this->stop == false))
            $this->_exec();
        
        if ($this->dry == true)
            $this->message = "DRY RUN - ".$this->message;
        
        return $this->message;
    }
    
    abstract protected function _dry_exec();
    
    abstract protected function _exec();
        
    protected function retrieve_islandora_object(){
        $this->object = islandora_object_load($this->pid);
    }  
    
    protected function get_pid(){
        return $this->pid;
    }
    
    protected function has_children (){
        $query = <<<EOQ
ASK {
  ?subject <fedora-model:hasModel> <info:fedora/fedora-system:FedoraObject-3.0> ;
           ?pred <info:fedora/$this->pid> .
}
EOQ;
        
        $child_results = $this->object->repository->ri->sparqlQuery($query);
        return $child_results[0]['k0']['value'] == 'true';
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/

class purge extends command{

    protected function _dry_exec(){
//        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        elseif ($this->has_children()){
            $this->message = $this->get_pid().': Has children. Cannot purge';
            $this->stop = true;
        }
        else{
            $this->message = $this->get_pid().': Purging...';
        }
    }

    protected function _exec(){
        $this->object->repository->purgeObject($this->pid);
        $this->message.=' Purged';
    }    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class show_type extends command{
    
    private function get_pub_type(AbstractObject $object) {
        $dom = new DOMDocument();
        $mods = $object['MODS']->content;
        if ($mods){
            $dom->loadXML($object['MODS']->content);
            $xpath = new DOMXPath($dom);
            $xpath->registerNamespace('m', 'http://loc.gov/mods/v3');
            
            
            $mods = $xpath->query('//mods:mods');
            $type = $xpath->evaluate('normalize-space(concat(mods:genre[@authorityURI="info:eu-repo/semantics"], " "))', $mods[0]);
        }
        else
            $type = 'WARNING: No MODS!!!';
        return $type;
        
    }
    protected function _dry_exec(){
        $this->message.= $this->get_pid().": ".$this->get_pub_type($this->object);
    }
    
    protected function _exec(){
        return 0;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mxds extends command{ 
    
    private   $mxds;
    private   $dc;
    private   $mxds_content;
    private   $dc_content;
    private   $dom;
    private   $xpath;
    protected $edited = false;
    protected $update_dc = false; //To be overridden in subclasses if DC update is required
    protected $mxds_type = ''; //To be overridden in subclasses
    protected $mxds_namespaceURI = ''; //To be overridden in subclasses
    protected $working_dir;
    
    protected function _dry_exec(){
//        $this->retrieve_islandora_object();
        if (in_array("ir:citationCModel", $this->object->models) or in_array("islandora:personCModel", $this->object->models)){
            $this->retrieve_mxds();
            $this->retrieve_mxds_content();
            $this->mxds_content = $this->edit_mxds(); //To be implemented in subclasses
            if ($this->mxds_content == $this->mxds->content){
                $this->edited = false;
                $this->message = $this->get_pid().": $this->mxds_type not edited";
            }
            else{
                $this->edited = true;
                $this->message = $this->get_pid().": $this->mxds_type edited";
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->mxds_content,$this->mxds_type.'_new');
                    $this->log_ds($this->mxds->content,$this->mxds_type.'_old');
                }
            }
            if ($this->update_dc && $this->edited){
                $this->edit_dc();
                $this->message.=', DC regenerated';
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->dc_content,'DC_new');
                    $this->log_ds($this->dc->content,'DC_old');
                }
            }
        }
        else{
            $this->message = $this->get_pid().': Not a valid object';
            $this->stop = true;
        }       
    }
    
    protected function _exec(){
        if ($this->edited == true){
            $this->ingest_mxds();
            if ($this->update_dc)
                $this->ingest_dc();
            $this->message.=" - Ingested";
        }
        else 
            $this->message.=" - Not ingested";
    }
    

    protected function get_mxds_content(){
        return $this->mxds_content;
    }
    
    protected function set_mxds_content($content){
        $this->mxds_content = $content;
    }
    
        
    private function retrieve_mxds(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === $this->mxds_type){
                $this->mxds = $datastream;
                break;
            }
        }        
        //$this->retrieve_ds($this->mods, 'MODS');
    }

    private function retrieve_mxds_content(){
        $this->mxds_content = $this->mxds->content;
    }
    
    abstract protected function edit_mxds();
    
    private function ingest_mxds(){
        $this->mxds->content = $this->mxds_content;
    }

    private function retrieve_dc(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'DC'){
                $this->dc = $datastream;
                break;
            }
        }
        
//        $this->retrieve_ds($this->dc, 'DC');
    }
    
    protected function set_dom(){
        $this->dom = new DOMDocument();
        $this->dom->loadXML($this->mxds_content);
    }
    
    protected function get_dom(){
        return $this->dom;
    }
    
    protected function set_xpath(){
        $this->xpath = new DOMXPath($this->dom);
        $this->xpath->registerNamespace(strtolower($this->mxds_type), $this->mxds_namespaceURI);
    }
    
    protected function get_xpath(){
        return $this->xpath;
    }
    
    abstract protected function get_mxds_to_dc();
    
    private function edit_dc(){
        $this->retrieve_dc();
        $this->dc_content = static::runXSLTransform(array(
            'xsl' => $this->get_mxds_to_dc(),
//            'xsl' => drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl',
            'input' => $this->get_mxds_content(),
        ));
        
    }
    
    private function ingest_dc(){
        if (!($this->dc_content === $this->dc->content)) {
            $this->dc->content = $this->dc_content;
        }
    }

    private static function runXSLTransform($info) {
        $xsl = new DOMDocument();   
        $xsl->load($info['xsl']);
        
        $input = new DOMDocument();
        $input->loadXML($info['input']);
        
        $processor = new XSLTProcessor();
        $processor->importStylesheet($xsl);
        
        return $processor->transformToXML($input);
    }
    
    private function retrieve_ds($class_var, $ds_string){
        foreach ($this->object as $datastream) {
            if ($datastream->id === '$ds_string'){
                $class_var = $datastream;
                break;
            }
        }
    }  
    
    private function log_ds($ds_content, $ds_name){
        $filename = date('Y-m-d_H-i-s').'_'.$this->pid.'_'.$ds_name;
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
        else
            $wd = null;
        $fid = fopen($wd.$filename, 'w');
        fwrite($fid, $ds_content);
        fclose($fid);
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mods extends batch_edit_mxds{
    protected $mxds_type = 'MODS'; 
    protected $mxds_namespaceURI = 'http://loc.gov/mods/v3'; 
    
    function get_mxds_to_dc(){
        return drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl';
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mads extends batch_edit_mxds{
    protected $mxds_type = 'MADS'; 
    protected $mxds_namespaceURI = '';

    function get_mxds_to_dc(){
        return drupal_get_path('module', 'zip_importer') . '/xsl/mads_to_dc.xsl';
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_publisher extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $section = $this->get_xpath()->query('//mods:mods/mods:relatedItem [@type="host"]')[0];
        if($section){
            $publisher = $section->getElementsByTagName('publisher')->item(0)->nodeValue;
        }
        
        if ($publisher == $this->options['current']){
            $section->getElementsByTagName('publisher')->item(0)->nodeValue = $this->options['new'];
            return $this->get_dom()->saveXML();
        } 
        return $this->get_mxds_content();
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_cit_dep_name extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $sections = $this->get_xpath()->query('//mods:mods/mods:name [@type="personal"]');
          
        $edited = false;
        $roles = array('author', 'thesis advisor', 'editor');
        $authors = [];
        foreach ($sections as $section){
#            if ($section->getElementsByTagName('roleTerm')->item(0)->nodeValue == 'author'){
            if (in_array($section->getElementsByTagName('roleTerm')->item(0)->nodeValue, $roles)){
                $affiliation = $section->getElementsByTagName('affiliation')->item(0)->nodeValue;
                if ($affiliation == $this->options['current']){
                    $section->getElementsByTagName('affiliation')->item(0)->nodeValue = $this->options['new'];
                    $elements = $section->getElementsByTagName('nameIdentifier');
                    foreach ($elements as $element){
                        if ($element->getAttribute('type')=='authorId'){
                            array_push($authors, $element->nodeValue);
                        }
                    }
                    $edited = true;
                }
            }
        }
        
        $this->save_authors($authors);
        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }
    
    private function save_authors($authors){
        $filename = $this->options['authors_file'];
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
        else
            $wd = null;
        
        $fid = fopen($wd.$filename, 'a');
        foreach ($authors as $author){
            fwrite($fid, $author."\n");
        }
        fclose($fid);
    }    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_aut_dep_name extends batch_edit_mads{
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $sections = $this->get_xpath()->query('//mads:mads/mads:affiliation');
        
        $edited = false;
        foreach ($sections as $section){
            $affiliation = $section->getElementsByTagName('organization')->item(0)->nodeValue;
            if ($affiliation == $this->options['current']){
                $section->getElementsByTagName('organization')->item(0)->nodeValue = $this->options['new'];
                $edited = true;
            }
        }
        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }   
}

/********************************************************************
 * 
 * 
 * 
********************************************************************/
class test_edit extends batch_edit_mods{
    
    protected $update_dc = true;
    
    protected function edit_mxds(){
        return str_replace('bsgd','abc',  $this->get_mxds_content());
        
    }
}


