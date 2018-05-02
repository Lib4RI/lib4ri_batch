<?php

abstract class batch_edit_mods{
    private   $repository;
    private   $pid;
    private    $object;
    private   $mods;
    private   $dc;
    protected $mods_content;
    
    function __construct($repository, $pid) {
        $this->repository = $repository;
        $this->pid = $pid; 
        $this->get_islandora_object();
        $this->get_mods();
        $this->get_mods_content();
        $this->edit_mods();
        $this->update_mods();
        $this->update_dc();
    }

    private function get_islandora_object(){
        $this->object = islandora_object_load($this->pid);
    }
    
    
    private function get_mods(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'MODS'){
                $this->mods = $datastream;
                break;
            }
        }        
        //$this->get_ds($this->mods, 'MODS');
    }

    private function get_mods_content(){
        $this->mods_content = $this->mods->content;
    }
    
    abstract protected function edit_mods();
    
    private function update_mods(){
        $this->mods->content = $this->mods_content;
    }

    private function get_dc(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'DC'){
                $this->dc = $datastream;
                break;
            }
        }
        
//        $this->get_ds($this->dc, 'DC');
    }
    
    private function update_dc(){
        $this->get_dc();
        $xsl = new DOMDocument();
        $xsl->load(drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl');
        
        $input = new DOMDocument();
        $input->loadXML($this->dc->content);
        
        $processor = new XSLTProcessor();
        $processor->importStylesheet($xsl);
        
        $new_dc = $processor->transformToXML($input);
        
        var_dump(drupal_get_path('module', 'islandora_importer'));
        if ($new_dc)
            $this->dc->content = $new_dc;
        
        // TODO: actual dc update
    }
    
    
    protected function get_ds($class_var, $ds_string){
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
    
    protected function edit_mods(){
        $this->mods_content = str_replace('bsgd', 'nice joke',    $this->mods_content);
    }
}
