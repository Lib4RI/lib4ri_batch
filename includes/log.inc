<?php

class filelog{
    protected $options;
    protected $fname;
    protected $fid;
    
    public function __construct($options) {
        $this->options = $options;
        $furi = $this->options['fdir'].$this->options['fname'];
        while (file_exists($furi))
            $furi .= '0';
            
        if ($this->options['mode'] == 'new')
            $this->new_log($furi);
        else 
            $this->append_log($furi);
    }
        
    public function get_furi(){
        return $this->furi;
    }
    
    public function new_log($furi){
        $this->open_log($furi, 'x');
    }

    public function append_log($furi){
        $this->open_log($furi, 'a');
    }
    
    protected function open_log($furi, $mode){
        $this->furi = $furi;
        $this->fid = fopen($this->furi, $mode);
        return $this->fid;
    }

    public function get_fname(){
        return $this->fname;
    }
    
    public function get_fid(){
        return $this->fid;
    }
    
    public function close(){
        fclose($this->fid);
    }
    
    public function log_plain ($message) {
        fwrite($this->fid, $message."\n");
        fflush($this->fid);
    }
    
    public function log($message){
        $this->log_plain(date('Y-m-d H:i:s', time()).': '.$message);
    }
    
    public function email_notify($addresses){
        
    }
}