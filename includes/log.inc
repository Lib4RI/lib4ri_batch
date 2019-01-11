<?php

class filelog{
    protected $options;
    protected $fname;
    protected $fid;
    
    public function __construct($options) {
        $this->options = $options;
        
        $furi = $this->options['fdir'].$this->options['fname'];
        $this->open_log($furi);
    }
        
    protected function open_log($furi){
        $this->furi = $furi;
        $this->fid = fopen($this->furi, 'w+');
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