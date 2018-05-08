<?php

abstract class batch_edit_mods{
    private   $repository;
    private   $pid;
    private   $dry;
    private   $object;
    private   $mods;
    private   $dc;
    private   $mods_content;
    protected $ingest = false;
    protected $update_dc = false; //To be overridden in subclasses if DC update is required
    
    public function __construct($repository, $pid, $dry) {
        $this->repository = $repository;
        $this->pid = $pid;
        $this->dry = $dry;
    }
    
    public function exec(){
        $this->retrieve_islandora_object();
        if (in_array("ir:citationCModel", $this->object->models)){
            $this->retrieve_mods();
            $this->retrieve_mods_content();            
            $ret = $this->edit_mods(); //To be implemented in subclasses
            
            if (($this->ingest == true) && ($this->dry == false)){
                $this->ingest_mods();
                if ($this->update_dc)
                    $this->ingest_dc();
            }
        }
        else{
            $ret = $this->get_pid().': Not a citation';
        }
        if ($this->dry == true)
            $ret="DRY RUN - ".$ret;
            
        return $ret;
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
    
    protected function get_pid(){
        return $this->pid;
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
    
    protected $update_dc = false;
    
    protected function edit_mods(){
        $old_content = $this->get_mods_content();
        $new_content = str_replace( 'abc','bsgd',  $old_content);
        
        if ($new_content == $old_content){
            $this->ingest = false;
            return $this->get_pid().': Not edited';
        }
        else{
            $this->set_mods_content($new_content);
            $this->ingest = true;
            return $this->get_pid().': Edited';
        }
    }
}
