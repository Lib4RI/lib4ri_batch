<?php

abstract class batch_edit_mods{
    private   $repository;
    private   $pid;
    private   $object;
    private   $mods;
    private   $dc;
    private   $mods_content;
    protected $update_dc = false; //To be overridden in subclasses if DC update is required
    
    public function __construct($repository, $pid) {
        $this->repository = $repository;
        $this->pid = $pid; 
        $this->retrieve_islandora_object();
        $this->retrieve_mods();
        $this->retrieve_mods_content();
        $this->edit_mods(); //To be implemented in subclasses
        $this->ingest_mods();
        if ($this->update_dc)
            $this->ingest_dc();
    }

    protected function get_mods_content(){
        return $this->mods_content;
    }
    
    protected function set_mods_content($content){
        $this->mods_content = $content;
    }
    
    private function retrieve_islandora_object(){
        $this->object = islandora_object_load($this->pid);
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
    
    private function ingest_dc(){
        $this->retrieve_dc();        
        $new_dc = static::runXSLTransform(array(
            'xsl' => drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl',
            'input' => $this->get_mods_content(),
        ));
        if ($new_dc) {
            $this->dc->content = $new_dc;
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
}


/********************************************************************
 * 
 * 
 * 
********************************************************************/

class test_edit extends batch_edit_mods{
    
#    protected $update_dc = true;
    
    protected function edit_mods(){
        $this->set_mods_content(str_replace( 'nice joke', 'bsgd',   $this->get_mods_content()));
    }
}
