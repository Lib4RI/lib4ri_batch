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
        $dom->loadXML($object['MODS']->content);
        $xpath = new DOMXPath($dom);
        $xpath->registerNamespace('m', 'http://loc.gov/mods/v3');
        
        
        $mods = $xpath->query('//mods:mods');
        $type = $xpath->evaluate('normalize-space(concat(mods:genre[@authorityURI="info:eu-repo/semantics"], " "))', $mods[0]);
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
abstract class batch_edit_mods extends command{ 
    
    private   $mods;
    private   $dc;
    private   $mods_content;
    private   $dc_content;
    private   $dom;
    private   $xpath;
    protected $edited = false;
    protected $update_dc = false; //To be overridden in subclasses if DC update is required
    protected $working_dir;
    
    protected function _dry_exec(){
//        $this->retrieve_islandora_object();
        if (in_array("ir:citationCModel", $this->object->models)){
            $this->retrieve_mods();
            $this->retrieve_mods_content();
            $this->mods_content = $this->edit_mods(); //To be implemented in subclasses
            if ($this->mods_content == $this->mods->content){
                $this->edited = false;
                $this->message = $this->get_pid().': Not edited';
            }
            else{
                $this->edited = true;
                $this->message = $this->get_pid().': Edited';
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->mods_content,'MODS');
                }
            }
            if ($this->update_dc){
                $this->edit_dc();
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->dc_content,'DC');
                }
            }
        }
        else{
            $this->message = $this->get_pid().': Not a citation';
            $this->stop = true;
        }       
    }
    
    protected function _exec(){
        if ($this->edited == true){
            $this->ingest_mods();
            if ($this->update_dc)
                $this->ingest_dc();
        }
        $this->message.=" - Ingested";
    }
    

    protected function get_mods_content(){
        return $this->mods_content;
    }
    
    protected function set_mods_content($content){
        $this->mods_content = $content;
    }
    
        
    private function retrieve_mods(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'MODS'){
                $this->mods = $datastream;
                break;
            }
        }        
        //$this->retrieve_ds($this->mods, 'MODS');
    }

    private function retrieve_mods_content(){
        $this->mods_content = $this->mods->content;
    }
    
    abstract protected function edit_mods();
    
    private function ingest_mods(){
        $this->mods->content = $this->mods_content;
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
        $this->dom->loadXML($this->mods_content);
    }
    
    protected function get_dom(){
        return $this->dom;
    }
    
    protected function set_xpath(){
        $this->xpath = new DOMXPath($this->dom);
        $this->xpath->registerNamespace('mods', 'http://loc.gov/mods/v3');
    }
    
    protected function get_xpath(){
        return $this->xpath;
    }
    
    private function edit_dc(){
        $this->retrieve_dc();
        $this->dc_content = static::runXSLTransform(array(
            'xsl' => drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl',
            'input' => $this->get_mods_content(),
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
    }
}


/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_publisher extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function edit_mods(){
        $this->set_dom();
        $this->set_xpath();
        $section = $this->get_xpath()->query('//mods:mods/mods:relatedItem [@type="host"]')[0];
        $publisher = $section->getElementsByTagName('publisher')->item(0)->nodeValue;
        if ($publisher == $this->options['current']){
            $section->getElementsByTagName('publisher')->item(0)->nodeValue = $this->options['new'];
        } 
        return $this->get_dom()->saveXML();
    }
    
}
/********************************************************************
 * 
 * 
 * 
********************************************************************/
class test_edit extends batch_edit_mods{
    
    protected $update_dc = true;
    
    protected function edit_mods(){
        return str_replace('bsgd','abc',  $this->get_mods_content());
        
    }
}


