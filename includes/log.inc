<?php

class filelog{
    protected $options;
    protected $fname;
    protected $fid;
    protected $log;
    
    public function __construct($options) {
        $this->options = $options;
        $furi = $this->options['fdir'].$this->options['fname'];
        
        if ($this->options['mode'] == 'new'){
            while (file_exists($furi))
                $furi .= '0';
                
            $this->new_log($furi);
        }
        else{
            $this->append_log($furi);
        }
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
        $this->log_plain('');
        $this->open_time = $this->log("Log opening");
        return $this->fid;
    }

    public function get_fname(){
        return $this->fname;
    }
    
    public function get_fid(){
        return $this->fid;
    }
    
    public function close(){
        $this->log('Log closing');
        fclose($this->fid);
        if ($this->options['email'] == true)
            $this->email_notify();
    }
    
    public function log_plain ($message) {
        fwrite($this->fid, $message."\n");
        fflush($this->fid);
        $this->log.=$message."\n";
    }
    
    public function log($message){
        $t = date('Y-m-d H:i:s', time());
        $this->log_plain($t.': '.$message);
        return $t;
    }
    
    protected function email_notify(){
        mail($this->options['addresses'], $this->options['subject'].' - '.$this->open_time , $this->options['mailmessage']."\n\n".$this->log);
    }
}